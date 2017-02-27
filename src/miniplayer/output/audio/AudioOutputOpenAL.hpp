#ifndef AUDIOOUTPUTOPENAL_HPP
#define AUDIOOUTPUTOPENAL_HPP

#include "AudioOutput.hpp"
#include <al.h>
#include <alc.h>
#include <mutex>

#define AL_NUM_BUFFERS 6

class AudioOutputOpenAL : public AudioOutput
{
public:
    AudioOutputOpenAL();
    virtual ~AudioOutputOpenAL();
    bool open(AVFrame * avFrame);
    bool stop();
    bool close();
    bool render(AVFrame * avFrame);
    bool setVolume(float value);
    float getVolume();
    bool setMute(bool value);
    bool getMute();
private:
    bool swrConvert(AVFrame * src);
    void swrFree();
private:
    ALCcontext* mContext;
    ALCdevice* mDevice;
    AVFrame * mSwrTempFrame;
    SwrContext * mSwrContext;
    uint8_t * mSwrBuffer;
    int mSwrBufferSize;
    uint64_t mSwrChannelLayout;
    int mSwrChannels;
    int mSwrSampleRate;
    AVSampleFormat mSwrFormat;
    int mSwrNbSamples;
    ALuint mALSource;
    ALuint mALBuffers[AL_NUM_BUFFERS];
    ALenum mALFormat;
    int mALBufferState;
    float mVolume;
    bool mMute;

    static std::mutex globalMutex;
};

#endif // AUDIOOUTPUTOPENAL_HPP
