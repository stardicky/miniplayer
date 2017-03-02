#include "MiniPlayer.hpp"
#include <cassert>

using namespace miniplayer;

MiniPlayer::MiniPlayer(Callback * callback, AudioOutput * audioOutput) :
    mCallback(callback),
    mAudioOutput(audioOutput),
    mBusy(false),
    mSynced(false),
    mAbort(false),
    mAudioInited(false),
    mSeekable(false),
    mBuffering(false),
    mVideoStream(nullptr),
    mAudioStream(nullptr),
    mFormatContext(nullptr),
    mAudioClock(-1),
    mAudioClockDrift(-1),
    mVideoClock(-1),
    mVideoClockDrift(-1),
    mClockBase(-1),
    mMaxPacketBufferSize(5 * 1024 * 1024),
    mMaxBufferDuration(5),
    mMaxFrameQueueSize(40),
    mPosition(-1),
    mDuration(-1),
    mSeekToPosition(-1),
    mTotalBytes(0),
    mDownloadSpeed(0),
    mFps(0),
    mEndReached(false),
    mState(State::Stopped)
{
    qDebug() << __FUNCTION__;
}

MiniPlayer::~MiniPlayer()
{
    qDebug() << __FUNCTION__;

    mAbort = true;
    clearCommand();

    if(mOpenThread.joinable())
        mOpenThread.join();
    if(mStopThread.joinable())
        mStopThread.join();
    if(mReadPacketThread.joinable())
        mReadPacketThread.join();
    if(mVideoDecodeThread.joinable())
        mVideoDecodeThread.join();
    if(mAudioDecodeThread.joinable())
        mAudioDecodeThread.join();
    if(mVideoRenderThread.joinable())
        mVideoRenderThread.join();
    if(mAudioRenderThread.joinable())
        mAudioRenderThread.join();

    if(mFormatContext) {
        avcodec_close(mVideoStream->codec);
        avcodec_close(mAudioStream->codec);
        avformat_close_input(&mFormatContext);
        mVideoStream = nullptr;
        mAudioStream = nullptr;
    }

    mVideoPacketQueue.clear();
    mAudioPacketQueue.clear();
    mVideoFrameQueue.clear();
    mAudioFrameQueue.clear();

    if(mAudioInited)
    {
        mAudioInited = false;
        mAudioOutput->close();
    }
}

static void onFFmpegLogCallback(void* ctx, int level,const char* fmt, va_list vl)
{
    AVClass *c = ctx ? *(AVClass**)ctx : 0;
    QString qmsg = QString().sprintf("[FFmpeg:%s] ", c ? c->item_name(ctx) : "?") + QString().vsprintf(fmt, vl);
    qmsg = qmsg.trimmed();
    if (level > AV_LOG_WARNING)
        qDebug() << qPrintable(qmsg);
    else if (level > AV_LOG_PANIC)
        qWarning() << qPrintable(qmsg);
}

void MiniPlayer::init()
{
    av_register_all();
    avcodec_register_all();
    avformat_network_init();
    //av_log_set_callback(onFFmpegLogCallback);
}

void MiniPlayer::uninit()
{
    avformat_network_deinit();
}

void MiniPlayer::stopThread()
{
    qDebug() << __FUNCTION__ << "start";

    mAbort = true;

    if(mReadPacketThread.joinable())
        mReadPacketThread.join();
    if(mVideoDecodeThread.joinable())
        mVideoDecodeThread.join();
    if(mAudioDecodeThread.joinable())
        mAudioDecodeThread.join();
    if(mVideoRenderThread.joinable())
        mVideoRenderThread.join();
    if(mAudioRenderThread.joinable())
        mAudioRenderThread.join();

    std::unique_ptr<int, std::function<void (int *)>> scope((int *)1, [&](void*)
    {
        changeState(-1, State::Stopped);
        setBuffering(false);
        onCommandFinished();
    });

    if(mFormatContext)
    {
        avcodec_close(mVideoStream->codec);
        avcodec_close(mAudioStream->codec);
        avformat_close_input(&mFormatContext);
        mVideoStream = nullptr;
        mAudioStream = nullptr;
    }

    mVideoPacketQueue.clear();
    mAudioPacketQueue.clear();
    mVideoFrameQueue.clear();
    mAudioFrameQueue.clear();

    if(mAudioInited)
    {
        mAudioInited = false;
        mAudioOutput->close();
    }

    clearClock();

    mPosition = mDuration = -1;
    mSeekable = false;
    mSeekToPosition = -1;
    mSynced = false;
    mTotalBytes = 0;
    mDownloadSpeed = 0;
    mFps = 0;
    mEndReached = false;

    qDebug() << __FUNCTION__ << "end";
}

void MiniPlayer::openThread()
{
    qDebug() << __FUNCTION__ << "start";

    mAbort = true;
    if(mReadPacketThread.joinable())
        mReadPacketThread.join();
    if(mVideoDecodeThread.joinable())
        mVideoDecodeThread.join();
    if(mAudioDecodeThread.joinable())
        mAudioDecodeThread.join();
    if(mVideoRenderThread.joinable())
        mVideoRenderThread.join();
    if(mAudioRenderThread.joinable())
        mAudioRenderThread.join();

    if(mFormatContext) {
        avcodec_close(mVideoStream->codec);
        avcodec_close(mAudioStream->codec);
        avformat_close_input(&mFormatContext);
        mVideoStream = nullptr;
        mAudioStream = nullptr;
    }
    mVideoPacketQueue.clear();
    mAudioPacketQueue.clear();
    mVideoFrameQueue.clear();
    mAudioFrameQueue.clear();

    mAbort = false;
    if(mAudioInited)
    {
        mAudioInited = false;
        mAudioOutput->close();
    }
    clearClock();
    mPosition = mDuration = -1;
    mSeekable = false;
    mSeekToPosition = -1;
    mTotalBytes = 0;
    mDownloadSpeed = 0;
    mFps = 0;
    mSynced = false;
    mEndReached = false;
    setBuffering(true);
    //-----------------------------------------------
    bool success = false;
    std::unique_ptr<int, std::function<void (int *)>> scope((int *)1, [&](void*)
    {
        if(!success)
        {
            qWarning() << __FUNCTION__ << "failure";
            changeState(-1, State::Stopped);
            setBuffering(false);
            mAbort = true;
        }
        onCommandFinished();
    });

    mFormatContext = avformat_alloc_context();
    //mFormatContext->flags |= AVFMT_FLAG_GENPTS;
    mFormatContext->interrupt_callback.opaque = (void *)this;
    mFormatContext->interrupt_callback.callback = &MiniPlayer::onInterruptCallback;

    int ret = avformat_open_input(&mFormatContext, mMediaPath.c_str(), NULL, NULL);
    if (ret < 0)
    {
        qWarning() << __FUNCTION__ << "avformat_open_input" << "failure";
        return;
    }

    ret = avformat_find_stream_info(mFormatContext, NULL);
    if(ret < 0)
    {
        qWarning() << __FUNCTION__ << "avformat_find_stream_info" << "failure";
        return;
    }

    if(mFormatContext->duration >= 0)
    {
        mPosition = 0;
        mDuration = (double)mFormatContext->duration / AV_TIME_BASE;

        if(mFormatContext->pb->seekable & AVIO_SEEKABLE_NORMAL)
            mSeekable = true;
    }

    mVideoStream = mAudioStream = nullptr;
    for (uint i = 0; i < mFormatContext->nb_streams; i++)
    {
        auto codecType = mFormatContext->streams[i]->codec->codec_type;
        if (codecType == AVMediaType::AVMEDIA_TYPE_VIDEO)
        {
            if(mVideoStream)
                continue;
            mVideoStream = mFormatContext->streams[i];
            mVideoWidth = mVideoStream->codec->width;
            mVideoHeight = mVideoStream->codec->height;
            auto timeBase = av_q2d(mVideoStream->time_base);
            mVideoPacketQueue.setTimeBase(timeBase);
            mVideoFrameQueue.setTimeBase(timeBase);
            qDebug() << __FUNCTION__ << "video size width:" << mVideoWidth << ", height:" << mVideoHeight;
        }
        else if(codecType == AVMediaType::AVMEDIA_TYPE_AUDIO)
        {
            if(mAudioStream)
                continue;
            mAudioStream = mFormatContext->streams[i];
            auto timeBase = av_q2d(mAudioStream->time_base);
            mAudioPacketQueue.setTimeBase(timeBase);
            mAudioFrameQueue.setTimeBase(timeBase);
        }
    }

    AVCodec * codec = avcodec_find_decoder(mVideoStream->codec->codec_id);
    if (!codec)
    {
        qWarning() << __FUNCTION__ << "avcodec_find_decoder for video" << "failure";
        return;
    }

   ret = avcodec_open2(mVideoStream->codec,codec,NULL);
   if(ret < 0)
   {
       qWarning() << __FUNCTION__ << "avcodec_open2 for video" << "failure";
       return;
   }

   codec = avcodec_find_decoder(mAudioStream->codec->codec_id);
   if (!codec)
   {
       qWarning() << __FUNCTION__ << "avcodec_find_decoder for audio" << "failure";
       return;
   }

   ret = avcodec_open2(mAudioStream->codec,codec,NULL);
   if(ret < 0)
   {
       qWarning() << __FUNCTION__ << "avcodec_open2 for audio" << "failure";
       return;
   }

   qDebug() << __FUNCTION__ << "duration:" << static_cast<double_t>(mFormatContext->duration) / AV_TIME_BASE;

   changeState(-1, State::Playing);
   mReadPacketThread = std::thread(&MiniPlayer::readPacketThread,this);
   mVideoDecodeThread = std::thread(&MiniPlayer::videoDecodeThread,this);
   mAudioDecodeThread = std::thread(&MiniPlayer::audioDecodeThread,this);
   mVideoRenderThread = std::thread(&MiniPlayer::videoRenderThread,this);
   mAudioRenderThread = std::thread(&MiniPlayer::audioRenderThread,this);
   success = true;
   qDebug() << __FUNCTION__ << "end";
}

void MiniPlayer::readPacketThread()
{
    qDebug() << __FUNCTION__ << "start";

    AVPacket packet = { 0 };
    bool eof = false;
    bool feof = false;

    for(; !mAbort; )
    {
        //seek begin ------------------------------------------
        if(mSeekable && mSeekToPosition >= 0)
        {
            qDebug() << __FUNCTION__ << "seek start";
            setBuffering(true);
            mVideoPacketQueue.clear();
            mAudioPacketQueue.clear();
            mVideoFrameQueue.clear();
            mAudioFrameQueue.clear();
            mVideoPacketQueue.appendFlushPacket();
            mAudioPacketQueue.appendFlushPacket();
            mAudioOutput->stop();
            mSynced = false;
            eof = false;
            feof = false;
            clearClock();

            auto seekToPosition = mSeekToPosition;
            mPosition = seekToPosition;

            int seekFlags = 0;
            int64_t pos = seekToPosition * AV_TIME_BASE;
            int ret = avformat_seek_file(mFormatContext, -1, INT64_MIN, pos, INT64_MAX, seekFlags);
            if(ret < 0)
                qWarning() << __FUNCTION__ << "avformat_seek_file" << "failure" << ret;

            if(mSeekToPosition == seekToPosition)
            {
                mSeekToPosition = -1;
                qDebug() << __FUNCTION__ << "seek complete";
            }
            continue;
        }
        //seek end --------------------------------------------
        int64_t packetBufferSize = mVideoPacketQueue.dataSize() + mAudioPacketQueue.dataSize();

        if(packetBufferSize > mMaxPacketBufferSize || eof)
        {
            setBuffering(false);
            bool empty = mVideoPacketQueue.size() == 0 && mAudioPacketQueue.size() == 0 &&
                    mVideoFrameQueue.size() == 0 && mAudioFrameQueue.size() == 0;

            if(empty && eof)
            {
                mAbort = true;
                if(mVideoDecodeThread.joinable())
                    mVideoDecodeThread.join();
                if(mAudioDecodeThread.joinable())
                    mAudioDecodeThread.join();
                if(mVideoRenderThread.joinable())
                    mVideoRenderThread.join();
                if(mAudioRenderThread.joinable())
                    mAudioRenderThread.join();
                if(mFormatContext) {
                    avcodec_close(mVideoStream->codec);
                    avcodec_close(mAudioStream->codec);
                    avformat_close_input(&mFormatContext);
                    mVideoStream = nullptr;
                    mAudioStream = nullptr;
                }
                mAbort = false;
                if(mAudioInited)
                {
                    mAudioInited = false;
                    mAudioOutput->close();
                }
                clearClock();
                mSeekable = false;
                mSeekToPosition = -1;
                mTotalBytes = 0;
                mDownloadSpeed = 0;
                mFps = 0;
                mSynced = false;
                setBuffering(false);
                mEndReached = feof;
                changeState(-1, State::Stopped);
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        if(!mBuffering && (mVideoPacketQueue.size() == 0 || mVideoFrameQueue.size() == 0))
            setBuffering(true);

        av_init_packet(&packet);
        int ret = av_read_frame(mFormatContext, &packet);
        if (ret < 0)
        {
            if(ret == AVERROR(EAGAIN))
            {
                qWarning() << __FUNCTION__ << "av_read_frame" << "EAGAIN" << ret;
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                continue;
            }
            eof = true;
            //feof = ret == AVERROR_EOF || avio_feof(mFormatContext->pb);
            feof = ret == AVERROR_EOF;
            qWarning() << __FUNCTION__ << "av_read_frame" << ret << ",feof:" << feof;
            continue;
        }

        mTotalBytes += packet.size;

        if (packet.stream_index == mVideoStream->index)
        {
            av_dup_packet(&packet);
            mVideoPacketQueue.append(packet);
        }
        else if (packet.stream_index == mAudioStream->index)
        {
            av_dup_packet(&packet);
            mAudioPacketQueue.append(packet);
        }

        if(mBuffering)
        {
            auto bufferedDuration = mVideoPacketQueue.duration() + mVideoFrameQueue.duration();
            if(bufferedDuration >= mMaxBufferDuration && mVideoFrameQueue.size() > 0)
                setBuffering(false);
        }
    }

    qDebug() << __FUNCTION__ << "end";
}

void MiniPlayer::videoDecodeThread()
{
    qDebug() << __FUNCTION__ << "start";

    AVPacket packet = {0}, packet2 = {0};
    AVFrame * decodedFrame = av_frame_alloc();

    auto freePacketFunc = [&](AVPacket * packet){ av_free_packet(packet); };
    auto freeFrameFunc = [&](AVFrame * frame){ av_frame_free(&frame); };

    std::unique_ptr<AVFrame, decltype(freeFrameFunc)> freeFrame(decodedFrame, freeFrameFunc);

    for(; !mAbort; )
    {
        if(mVideoFrameQueue.size() > mMaxFrameQueueSize)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }

        av_init_packet(&packet);

        bool ret = mVideoPacketQueue.acquire(packet);
        if (!ret)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }

        if(mVideoPacketQueue.isFlushPacket(packet))
        {
            mVideoPacketQueue.clear();
            mVideoFrameQueue.clear();
            avcodec_flush_buffers(mVideoStream->codec);
            continue;
        }

        std::unique_ptr<AVPacket, decltype(freePacketFunc)> freePacket(&packet, freePacketFunc);

        int gotFrame = 0;

        packet2 = packet;
        while (packet2.size > 0 && !mAbort)
        {
            int ret = avcodec_decode_video2(mVideoStream->codec, decodedFrame, &gotFrame, &packet2);
            if (ret < 0) {
                qWarning() << __FUNCTION__ << "avcodec_decode_video2" << "failure" << ret;
                break;
            }

            if (gotFrame)
                break;

            packet2.size -= ret;
            packet2.data += ret;

            if (packet2.size <= 0)
                break;
        }

        if (gotFrame && mSeekToPosition == -1)
        {
            decodedFrame->pts = av_frame_get_best_effort_timestamp(decodedFrame);
            mVideoFrameQueue.append(av_frame_clone(decodedFrame));
        }
    }

    qDebug() << __FUNCTION__ << "end";
}

void MiniPlayer::audioDecodeThread()
{
    qDebug() << __FUNCTION__ << "start";

    AVPacket packet = {0}, packet2 = {0};
    AVFrame * decodedFrame = av_frame_alloc();

    auto freePacketFunc = [&](AVPacket * packet){ av_free_packet(packet); };
    auto freeFrameFunc = [&](AVFrame * frame){ av_frame_free(&frame); };

    std::unique_ptr<AVFrame, decltype(freeFrameFunc)> freeFrame(decodedFrame, freeFrameFunc);

    for(; !mAbort; )
    {
        if(mAudioFrameQueue.size() > mMaxFrameQueueSize)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }

        av_init_packet(&packet);

        bool ret = mAudioPacketQueue.acquire(packet);
        if (!ret)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }

        if(mAudioPacketQueue.isFlushPacket(packet))
        {
            mAudioPacketQueue.clear();
            mAudioFrameQueue.clear();
            avcodec_flush_buffers(mAudioStream->codec);
            continue;
        }

        std::unique_ptr<AVPacket, decltype(freePacketFunc)> freePacket(&packet, freePacketFunc);

        int gotFrame = 0;
        packet2 = packet;

        while(!mAbort)
        {
            int ret = avcodec_decode_audio4(mAudioStream->codec, decodedFrame, &gotFrame, &packet2);
            if (ret < 0) {
                qWarning() << __FUNCTION__ << "avcodec_decode_audio4" << "failure" << ret;
                break;
            }

            if(gotFrame)
                break;

            packet2.size -= ret;
            packet2.data += ret;

            if (packet2.size <= 0)
                break;
        }

        if(gotFrame && mSeekToPosition == -1)
        {
            if(!mAudioInited)
            {
                mAudioInited = true;
                mAudioOutput->open(decodedFrame);
            }
            decodedFrame->pts = av_frame_get_best_effort_timestamp(decodedFrame);
            mAudioFrameQueue.append(av_frame_clone(decodedFrame));
        }
    }

    qDebug() << __FUNCTION__ << "end";
}

void MiniPlayer::videoRenderThread()
{
    qDebug() << __FUNCTION__ << "start";

    int64_t baseTime = av_gettime_relative();
    int64_t timeCount = 0;
    int64_t totalFrame = 0;

    for(; !mAbort; )
    {
        int64_t time = av_gettime_relative();
        if((time - baseTime) / 1000000.f >= timeCount)
        {
            timeCount ++;
            //---------------------------------------------------------------------
            if(mDownloadSpeed == 0)
                mDownloadSpeed = static_cast<int64_t>(mTotalBytes);
            else
                mDownloadSpeed = ((mDownloadSpeed * 5) + (mTotalBytes * 3)) / 8.0f;
            mTotalBytes = 0;
            //---------------------------------------------------------------------
            mFps = totalFrame;
            totalFrame = 0;
        }

        if(mBuffering || mState == State::Paused || mSeekToPosition >= 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        AVFrame* renderFrame = nullptr;
        bool ret = mVideoFrameQueue.acquire(&renderFrame);
        if (!ret)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }

        if (mClockBase < 0)
            mClockBase = systemClock();

        if (mVideoClockDrift < 0)
            mVideoClockDrift = av_q2d(mVideoStream->time_base) * mVideoStream->start_time;

        setVideoClock(renderFrame->pts);

        if(!mSynced)
        {
            while(!mAbort && mState == State::Playing && mAudioClock == -1)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            while(!mAbort && !mSynced && mState == State::Playing)
            {
                auto diff = videoClock() - audioClock();
                if(diff >= 0.3) //audio slowest waiting drop audio
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
                else if (diff < -0.3) //audio fastest drop video
                {
                    qDebug() << "drop video frame ----------" << videoClock();
                    break;
                }
                else
                {
                    mSynced = true;
                    qDebug() << __FUNCTION__ << "synced" << videoClock() << audioClock();
                    break;
                }
            }

            if(!mSynced)
            {
                av_frame_free(&renderFrame);
                continue;
            }
        }

        totalFrame ++;
        mCallback->onVideoRender(renderFrame);

        auto vClock = videoClock();
        if(mSeekToPosition == -1 && abs(vClock - mPosition) > 0.3f)
        {
            //qDebug() << "pos:" << vClock << mPosition;
            mPosition = vClock;
            if(!mAbort)
                mCallback->onPositionChanged(mPosition);
        }

        auto duration = av_q2d(mVideoStream->time_base) * renderFrame->pkt_duration;
        auto delay = vClock - masterClock();
        delay = std::min(delay, duration * 2);
        if (delay > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int64_t>(delay * 1000)));        
    }
    qDebug() << __FUNCTION__ << "end";
}

void MiniPlayer::audioRenderThread()
{
    qDebug() << __FUNCTION__ << "start";
    auto freeFrameFunc = [&](AVFrame * frame){ av_frame_free(&frame); };

    bool paused = false;
    for(; !mAbort; )
    {
        //paused start -------------------------------------------------------------
        if(!paused && mState == State::Paused)
        {
            mAudioOutput->stop();
            paused = true;
        }
        else if(paused && mState == State::Playing)
            paused = false;

        if(paused || mSeekToPosition >= 0 || mBuffering)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        //paused end -------------------------------------------------------------

        AVFrame* renderFrame = nullptr;
        bool ret = mAudioFrameQueue.acquire(&renderFrame);

        if (!ret)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }

        std::unique_ptr<AVFrame, decltype(freeFrameFunc)> freeFrame(renderFrame, freeFrameFunc);

        if (mClockBase < 0)
            mClockBase = systemClock();

        if (mAudioClockDrift < 0)
            mAudioClockDrift = av_q2d(mAudioStream->time_base) * mAudioStream->start_time;

        setAudioClock(renderFrame->pts);

        if(!mSynced)
        {
            while(!mAbort && mState == State::Playing && mVideoClock == -1)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            while(!mAbort && !mSynced && mState == State::Playing)
            {
                auto diff = videoClock() - audioClock();
                if(diff >= 0.3) //audio slowest drop audio
                {
                    qDebug() << "drop audio frame ++++++++++" << audioClock();
                    break;
                }
                else if (diff < -0.3) //audio fastest waiting drop video
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
                else
                {
                    mSynced = true;
                    qDebug() << __FUNCTION__ << "synced" << videoClock() << audioClock();
                    break;
                }
            }

            if(!mSynced)
                continue;
        }

        bool worked = mAudioOutput->render(renderFrame);
        auto delay = av_q2d(mAudioStream->time_base) * renderFrame->pkt_duration;
        if (delay > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int64_t>(delay * 1000 - (worked ? 10 : 0))));
    }
    qDebug() << __FUNCTION__ << "end";
}
