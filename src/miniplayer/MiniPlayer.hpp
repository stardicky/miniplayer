#ifndef MINIPLAYER_HPP
#define MINIPLAYER_HPP

extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/time.h>
#include <libavutil/log.h>
}

#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <memory>

#include <QDebug>

#include "Queue.hpp"
#include "Command.hpp"
#include "output/audio/AudioOutput.hpp"

namespace miniplayer
{

class MiniPlayer
{
public:

    typedef struct {
        int64_t packetBufferSize;
        int64_t maxPacketBufferSize;
        size_t maxFrameQueueSize;
        size_t videoPacketQueueSize;
        size_t audioPacketQueueSize;
        size_t videoFrameQueueSize;
        size_t audioFrameQueueSize;
        double videoClock;
        double audioClock;
    } DumpInfo;

    typedef enum {
        Stopped = 0 ,
        Stopping,
        Opening,
        Playing,
        Paused
    } State;

    class Callback
    {
    public:
        virtual void onVideoRender(AVFrame * frame) = 0;
        virtual void onPositionChanged(double pos) = 0;
        virtual void onEndReached() = 0;
        virtual void onStateChanged(int state) = 0;
    };

private:
    std::string mMediaPath;
    AVFormatContext* mFormatContext;
    std::thread mOpenThread;
    std::thread mStopThread;
    std::thread mReadPacketThread;
    std::thread mVideoDecodeThread;
    std::thread mAudioDecodeThread;
    std::thread mVideoRenderThread;
    std::thread mAudioRenderThread;
    AVStream * mVideoStream;
    AVStream * mAudioStream;
    AVPacketQueue mVideoPacketQueue;
    AVPacketQueue mAudioPacketQueue;
    AVFrameQueue mVideoFrameQueue;
    AVFrameQueue mAudioFrameQueue;
    int64_t mMaxPacketBufferSize;
    size_t mMaxFrameQueueSize;
    std::atomic_int mVideoWidth;
    std::atomic_int mVideoHeight;
    int mState;
    std::mutex mStateMutex;
    std::atomic_bool mBusy;
    std::atomic_bool mAbort;
    std::atomic_bool mAudioInited;
    std::atomic_bool mSeekable;
    std::atomic_bool mSynced;
    std::shared_ptr<Command> mPendingCommand;
    std::mutex mCommandMutex;
    Callback * mCallback;
    AudioOutput * mAudioOutput;
    double mPosition;
    double mDuration;
    double mSeekToPosition;

    double mAudioClock;
    double mAudioClockDrift;
    double mVideoClock;
    double mVideoClockDrift;
    double mClockBase;
public:
    MiniPlayer(Callback * callback,AudioOutput * audioOutput);
    virtual ~MiniPlayer();
    static void init();
    static void uninit();

    void open(const std::string & mediaPath)
    {
        qDebug() << __FUNCTION__ << mediaPath.c_str();
        std::shared_ptr<Command> cmd(new OpenCommand(mediaPath));
        submitCommand(cmd);
    }

    void mute()
    {
        //todo
    }

    void stop()
    {
        qDebug() << __FUNCTION__;
        std::shared_ptr<Command> cmd(new StopCommand());
        submitCommand(cmd);
    }

    void play()
    {
        changeState(State::Paused,State::Playing);
    }

    void pause()
    {
        changeState(State::Playing,State::Paused);
    }

    void togglePause()
    {
        if(mState == State::Paused)
            play();
        else if(mState == State::Playing)
            pause();
    }

    void seek(double pos)
    {
        if(!mSeekable)
            return;
        if(pos < 0)
            pos = 0;
        else if(pos > mDuration)
            pos = mDuration;
        qDebug() << __FUNCTION__ << pos;
        mSeekToPosition = pos;
        mPosition = pos;
    }

    double getPosition() { return mPosition; }
    double getDuration() { return mDuration; }
    bool isSeekable() { return mSeekable; }
    int getState() { return mState; }

    void dump(DumpInfo * info)
    {
        info->videoPacketQueueSize = mVideoPacketQueue.size();
        info->videoFrameQueueSize = mVideoFrameQueue.size();
        info->audioPacketQueueSize = mAudioPacketQueue.size();
        info->audioFrameQueueSize = mAudioFrameQueue.size();
        info->packetBufferSize = mVideoPacketQueue.dataSize() + mAudioPacketQueue.dataSize();
        info->maxPacketBufferSize = mMaxPacketBufferSize;
        info->maxFrameQueueSize = mMaxFrameQueueSize;
        info->videoClock = videoClock();
        info->audioClock = audioClock();
    }

private:
    void printDumpInfo()
    {
        qDebug() << __FUNCTION__
                 << "VPQ:" << mVideoPacketQueue.size()
                 << "APQ:" << mAudioPacketQueue.size()
                 << "VFQ:" << mVideoFrameQueue.size()
                 << "AFQ:" << mAudioFrameQueue.size()
                 << "BS:" << (mVideoPacketQueue.dataSize() + mAudioPacketQueue.dataSize()) << "/" << mMaxPacketBufferSize;
    }

    void openThread();
    void stopThread();
    void readPacketThread();
    void videoDecodeThread();
    void audioDecodeThread();
    void videoRenderThread();
    void audioRenderThread();

    void changeState(int from,int to)
    {
        std::lock_guard<std::mutex> l(mStateMutex);
        if(mState == to)
            return;

        if(from != -1 && from != mState)
        {
            qWarning() << __FUNCTION__ << "cancel, current state:" << mState
                       << "," << from << "->" << to;
            return;
        }

        int oldState = mState;
        mState = to;
        qDebug() << __FUNCTION__ << oldState << "->" << mState;
        mCallback->onStateChanged(mState);
    }

    void submitCommand(std::shared_ptr<Command> cmd)
    {
        std::lock_guard<std::mutex> l(mCommandMutex);
        if(mBusy)
            mPendingCommand = cmd;
        else
            doCommand(cmd);
    }

    void clearCommand()
    {
        std::lock_guard<std::mutex> l(mCommandMutex);
        mPendingCommand = nullptr;
    }

    void onCommandFinished()
    {
        mBusy = false;
        std::lock_guard<std::mutex> l(mCommandMutex);
        auto command = mPendingCommand;
        if(command)
            doCommand(command);
        mPendingCommand = nullptr;
    }

    void doCommand(std::shared_ptr<Command> cmd)
    {
        qDebug() << __FUNCTION__ << cmd->id << cmd->type;
        if(cmd->type == CommandType::Open)
        {
            auto openCmd = std::dynamic_pointer_cast<OpenCommand>(cmd);
            mMediaPath = openCmd->mediaPath;
            mBusy = true;
            changeState(-1, State::Opening);
            if(mOpenThread.joinable())
                mOpenThread.detach();
            mOpenThread = std::thread(&MiniPlayer::openThread, this);
        }
        else if(cmd->type == CommandType::Stop)
        {
            mBusy = true;
            changeState(-1, State::Stopping);
            if(mStopThread.joinable())
                mStopThread.detach();
            mStopThread = std::thread(&MiniPlayer::stopThread, this);
        }
    }

    static int onInterruptCallback(void * ctx)
    {
        //qDebug() << __FUNCTION__;
        MiniPlayer * player = (MiniPlayer *)ctx;
        return player->mAbort;
    }

    //clock
    void clearClock()
    {
        mAudioClock = -1;
        mAudioClockDrift = -1;
        mVideoClock = -1;
        mVideoClockDrift = -1;
        mClockBase = -1;
    }

    double systemClock() const
    {
        return (double)av_gettime_relative() / 1000000.0f;
    }

    double videoClock() const
    {
        return mVideoClock - mVideoClockDrift;
    }

    double audioClock() const
    {
        return mAudioClock - mAudioClockDrift;
    }

    double masterClock() const
    {
        return audioClock();
    }

    void setAudioClock(const int64_t& pts)
    {
        mAudioClock = av_q2d(mAudioStream->time_base) * pts;
    }

    void setVideoClock(const int64_t& pts)
    {
        mVideoClock = av_q2d(mVideoStream->time_base) * pts;
    }
};

}
#endif // MINIPLAYER_HPP
