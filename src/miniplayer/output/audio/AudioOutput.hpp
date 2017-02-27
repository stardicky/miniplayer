#ifndef AUDIOOUTPUT_HPP
#define AUDIOOUTPUT_HPP

extern "C"
{
#include <libavformat/avformat.h>
#include <libavutil/log.h>
#include <libswresample/swresample.h>
}

class AudioOutput
{
public:
    AudioOutput() {}

    virtual ~AudioOutput() {}

    virtual bool open(AVFrame * avFrame) = 0;
    virtual bool stop() = 0;
    virtual bool close() = 0;
    virtual bool render(AVFrame * avFrame) = 0;
    virtual bool setVolume(float value) = 0;
    virtual float getVolume() = 0;
};

#endif // AUDIOOUTPUT_HPP
