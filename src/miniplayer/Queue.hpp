#ifndef QUEUE_HPP
#define QUEUE_HPP

extern "C"
{
#include <libavformat/avformat.h>
}

#include <list>
#include <mutex>
#include <thread>
#include <QDebug>

namespace miniplayer
{

class AVFrameQueue
{
public:
    AVFrameQueue() :
        mTimeBase(0),
        mDuration(0)
    {}

    ~AVFrameQueue()
    {
        clear();
    }

    void append(AVFrame* frame)
    {
        //qDebug() << __FUNCTION__;
        std::lock_guard<std::mutex> l(mMutex);
        mDatas.push_back(frame);
        mDuration += static_cast<int64_t>(frame->pkt_duration * mTimeBase * 1000);
    }

    bool acquire(AVFrame** frame)
    {
        std::lock_guard<std::mutex> l(mMutex);
        if (mDatas.size() == 0)
            return false;
        *frame = mDatas.front();
        mDatas.pop_front();
        mDuration -= static_cast<int64_t>((*frame)->pkt_duration * mTimeBase * 1000);
        return true;
    }

    void clear()
    {
        std::lock_guard<std::mutex> l(mMutex);
        if(mDatas.size() > 0)
        {
            mDuration = 0;
            for (auto& data : mDatas)
                av_frame_free(&data);
            mDatas.clear();
        }
    }

    std::size_t size() const
    {
        std::lock_guard<std::mutex> l(mMutex);
        return mDatas.size();
    }

    void setTimeBase(double timeBase)
    {
        mTimeBase = timeBase;
    }

    double duration() const
    {
        return mDuration / 1000.f;
    }

private:
    mutable std::mutex mMutex;
    std::list<AVFrame *> mDatas;
    double mTimeBase;
    int64_t mDuration;
};

class AVPacketQueue
{
public:
    AVPacketQueue() :
        mDataSize(0),
        mTimeBase(0),
        mDuration(0)
    {
        av_init_packet(&mFlushPacket);
        mFlushPacket.data = (uint8_t *)&mFlushPacket;
        mFlushPacket.size = 0;
    }

    ~AVPacketQueue()
    {
        clear();
    }

    void append(const AVPacket& pkt)
    {
        //qDebug() << __FUNCTION__;
        std::lock_guard<std::mutex> l(mMutex);
        mDatas.push_back(pkt);
        mDataSize += pkt.size;
        mDuration += static_cast<int64_t>(pkt.duration * mTimeBase * 1000);
    }

    void appendFlushPacket()
    {
        //qDebug() << __FUNCTION__;
        std::lock_guard<std::mutex> l(mMutex);
        mDatas.push_back(mFlushPacket);
    }

    bool acquire(AVPacket& pkt)
    {
        std::lock_guard<std::mutex> l(mMutex);
        if (mDatas.size() == 0)
            return false;
        pkt = mDatas.front();
        mDatas.pop_front();
        mDataSize -= pkt.size;
        mDuration -= static_cast<int64_t>(pkt.duration * mTimeBase * 1000);
        return true;
    }

    void clear()
    {
        std::lock_guard<std::mutex> l(mMutex);
        if(mDatas.size() > 0)
        {
            mDataSize = 0;
            mDuration = 0;
            for (auto& pkt : mDatas)
            {
                if(!isFlushPacket(pkt))
                    av_free_packet(&pkt);
            }
            mDatas.clear();
        }
    }

    std::size_t size() const
    {
        std::lock_guard<std::mutex> l(mMutex);
        return mDatas.size();
    }

    int64_t dataSize() const
    {
        return mDataSize;
    }

    double duration() const
    {
        return mDuration / 1000.f;
    }

    bool isFlushPacket(AVPacket & pkt) const
    {
        return pkt.data == (uint8_t *)&mFlushPacket;
    }

    void setTimeBase(double timeBase)
    {
        mTimeBase = timeBase;
    }

private:
    mutable std::mutex mMutex;
    std::list<AVPacket> mDatas;
    AVPacket mFlushPacket;
    int64_t mDataSize;
    double mTimeBase;
    int64_t mDuration;
};

}

#endif // QUEUE_HPP
