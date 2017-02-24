#ifndef QUEUE_HPP
#define QUEUE_HPP

extern "C"
{
#include <libavformat/avformat.h>
}

#include <list>
#include <mutex>
#include <thread>

namespace miniplayer
{

class AVFrameQueue
{
public:
    AVFrameQueue() {}

    ~AVFrameQueue()
    {
        clear();
    }

    void append(AVFrame* frame)
    {
        std::lock_guard<std::mutex> l(mMutex);
        mDatas.push_back(frame);
    }

    bool acquire(AVFrame** frame)
    {
        std::lock_guard<std::mutex> l(mMutex);
        if (mDatas.size() == 0)
            return false;
        *frame = mDatas.front();
        mDatas.pop_front();
        return true;
    }

    void clear()
    {
        std::lock_guard<std::mutex> l(mMutex);
        if(mDatas.size() > 0)
        {
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

private:
    mutable std::mutex mMutex;
    std::list<AVFrame *> mDatas;
};

class AVPacketQueue
{
public:
    AVPacketQueue() : mDataSize(0)
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
        std::lock_guard<std::mutex> l(mMutex);
        mDatas.push_back(pkt);
        mDataSize += pkt.size;
    }

    void appendFlushPacket()
    {
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
        return true;
    }

    void clear()
    {
        std::lock_guard<std::mutex> l(mMutex);
        if(mDatas.size() > 0)
        {
            mDataSize = 0;
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

    bool isFlushPacket(AVPacket & pkt) const
    {
        return pkt.data == (uint8_t *)&mFlushPacket;
    }

private:
    mutable std::mutex mMutex;
    std::list<AVPacket> mDatas;
    AVPacket mFlushPacket;
    int64_t mDataSize;
};

}

#endif // QUEUE_HPP
