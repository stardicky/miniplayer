#include "MiniPlayer.hpp"
#include <cassert>

using namespace miniplayer;

MiniPlayer::MiniPlayer(Callback * callback, AudioOutput * audioOutput) :
    mCallback(callback),
    mAudioOutput(audioOutput),
    mBusy(false),
    mAbort(false),
    mAudioInited(false),
    mSeekable(false),
    mVideoStream(nullptr),
    mAudioStream(nullptr),
    mFormatContext(nullptr),
    mAudioClock(-1),
    mAudioClockDrift(-1),
    mVideoClock(-1),
    mVideoClockDrift(-1),
    mClockBase(-1),
    mMaxPacketBufferSize(5 * 1024 * 1024),
    mMaxFrameQueueSize(40),
    mPosition(-1),
    mDuration(-1),
    mSeekToPosition(-1),
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
    //-----------------------------------------------
    bool success = false;
    std::unique_ptr<int, std::function<void (int *)>> scope((int *)1, [&](void*)
    {
        if(!success)
        {
            qWarning() << __FUNCTION__ << "failure";
            changeState(-1, State::Stopped);
            mAbort = true;
        }
        onCommandFinished();
    });

    mFormatContext = avformat_alloc_context();
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
            mVideoStream = mFormatContext->streams[i];
            mVideoWidth = mVideoStream->codec->width;
            mVideoHeight = mVideoStream->codec->height;
            qDebug() << __FUNCTION__ << "video size width:" << mVideoWidth << ", height:" << mVideoHeight;
        }
        else if(codecType == AVMediaType::AVMEDIA_TYPE_AUDIO)
        {
            mAudioStream = mFormatContext->streams[i];
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

    for(; !mAbort; )
    {
        //seek begin ------------------------------------------
        if(mSeekable && mSeekToPosition >= 0)
        {
            eof = false;
            mVideoPacketQueue.clear();
            mAudioPacketQueue.clear();
            mVideoFrameQueue.clear();
            mAudioFrameQueue.clear();
            mVideoPacketQueue.appendFlushPacket();
            mAudioPacketQueue.appendFlushPacket();
            mAudioOutput->stop();
            mClockBase = -1;

            auto seekToPosition = mSeekToPosition;
            mPosition = seekToPosition;
            mSeekToPosition = -1;

            int seekFlags = 0;
            int64_t pos = seekToPosition * AV_TIME_BASE;
            int ret = avformat_seek_file(mFormatContext, -1, INT64_MIN, pos, INT64_MAX, seekFlags);
            if(ret < 0)
                qWarning() << __FUNCTION__ << "avformat_seek_file" << "failure" << ret;

            if(mSeekToPosition == seekToPosition)
                mSeekToPosition = -1;
            continue;
        }
        //seek end --------------------------------------------
        int64_t packetBufferSize = mVideoPacketQueue.dataSize() + mAudioPacketQueue.dataSize();

        if(packetBufferSize > mMaxPacketBufferSize || eof)
        {
            if(eof)
            {
                if(mVideoPacketQueue.size() == 0 && mAudioPacketQueue.size() == 0 &&
                    mVideoFrameQueue.size() == 0 && mAudioFrameQueue.size() == 0)
                {
                    qWarning() << __FUNCTION__ << "packet buffer size:" << packetBufferSize << ", eof:" << eof;
                    mCallback->onEndReached();
                    break;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        av_init_packet(&packet);

        int ret = av_read_frame(mFormatContext, &packet);
        if (ret < 0)
        {
            if(ret == AVERROR_EOF || avio_feof(mFormatContext->pb))
            {
                qWarning() << __FUNCTION__ << "av_read_frame" << "EOF" << ret;
                eof = true;
                continue;
            }
            else if (ret == AVERROR(EAGAIN))
            {
                qWarning() << __FUNCTION__ << "av_read_frame" << "EAGAIN" << ret;
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                continue;
            }

            qWarning() << __FUNCTION__ << "av_read_frame" << "failure todo" << ret;
            eof = true;
            continue;
        }

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
            avcodec_flush_buffers(mVideoStream->codec);
            continue;
        }

        std::unique_ptr<AVPacket, decltype(freePacketFunc)> freePacket(&packet, freePacketFunc);

        int gotPicture = 0;

        packet2 = packet;
        while (packet2.size > 0 && !mAbort)
        {
            int ret = avcodec_decode_video2(mVideoStream->codec, decodedFrame, &gotPicture, &packet2);
            if (ret < 0) {
                qWarning() << __FUNCTION__ << "avcodec_decode_video2" << "failure" << ret;
                break;
            }

            if (gotPicture)
                break;

            packet2.size -= ret;
            packet2.data += ret;

            if (packet2.size <= 0)
                break;
        }

        if (gotPicture)
        {
//            assert(decodedFrame->data[0]);
//            assert(decodedFrame->data[1]);
//            assert(decodedFrame->data[2]);
//            assert(decodedFrame->linesize[0]);
//            assert(decodedFrame->linesize[1]);
//            assert(decodedFrame->linesize[2]);
//            assert(decodedFrame->width);
//            assert(decodedFrame->height);
            // 计算pts等信息.
            decodedFrame->pts = av_frame_get_best_effort_timestamp(decodedFrame);
            mVideoFrameQueue.append(av_frame_clone(decodedFrame));
        }

        //qDebug() << __FUNCTION__ << "got:" << gotPicture;
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

        if(gotFrame)
        {
            if(!mAudioInited)
            {
                mAudioInited = true;
                mAudioOutput->open(decodedFrame);
            }
            // 计算pts等信息.
            decodedFrame->pts = av_frame_get_best_effort_timestamp(decodedFrame);
            mAudioFrameQueue.append(av_frame_clone(decodedFrame));
        }

        //qDebug() << __FUNCTION__ << "got:" << gotFrame;
    }

    qDebug() << __FUNCTION__ << "end";
}

void MiniPlayer::videoRenderThread()
{
    qDebug() << __FUNCTION__ << "start";

    for(; !mAbort; )
    {
        if(mState == State::Paused)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        AVFrame* renderFrame = nullptr;
        bool ret = mVideoFrameQueue.acquire(&renderFrame);
        //qDebug() << __FUNCTION__ << ret;
        if (!ret)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }

        // 获取当前系统时间.
        if (mClockBase < 0)
            mClockBase = systemClock();

        // 起始时间.
        if (mVideoClockDrift < 0)
            mVideoClockDrift = av_q2d(mVideoStream->time_base) * mVideoStream->start_time;

        // 设置当前clock.
        setVideoClock(renderFrame->pts);

        mCallback->onVideoRender(renderFrame);

        // 同步逻辑.
        auto vClock = videoClock();
        if(mSeekToPosition == -1 && abs(vClock - mPosition) > 0.3f)
        {
            mPosition = vClock;
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

        if(paused)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }
        //paused end -------------------------------------------------------------

        AVFrame* renderFrame = nullptr;
        bool ret = mAudioFrameQueue.acquire(&renderFrame);

        //qDebug() << __FUNCTION__ << ret;
        if (!ret)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }

        std::unique_ptr<AVFrame, decltype(freeFrameFunc)> freeFrame(renderFrame, freeFrameFunc);

        // 获取当前系统时间.
        if (mClockBase < 0)
            mClockBase = systemClock();

        // 起始时间.
        if (mAudioClockDrift < 0)
            mAudioClockDrift = av_q2d(mAudioStream->time_base) * mAudioStream->start_time;

        // 设置当前clock.
        setAudioClock(renderFrame->pts);

        // 记录当前时间, 并计算出目标时间和时长, 然后开始渲染.
        auto targetTime = systemClock() + (av_q2d(mAudioStream->time_base) * renderFrame->pkt_duration);

        if(mAudioOutput->isStopped())
        {
            auto diff = videoClock() - audioClock();
            if(diff >= 0.5)
                continue;
            else if (diff < -0.5)
            {
                if(mVideoFrameQueue.size() > 0)
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
        }

        bool worked = mAudioOutput->render(renderFrame);

        // 判断是否需要延迟一会会.
        auto delay = targetTime - systemClock();
        if (delay > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int64_t>(delay * 1000 - (worked ? 10 : 0))));
    }
    qDebug() << __FUNCTION__ << "end";
}
