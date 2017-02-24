#ifndef QVIDEOFRAME_HPP
#define QVIDEOFRAME_HPP

#include <QByteArray>
#include <QMutex>
#include <memory>

extern "C"
{
#include <libavformat/avformat.h>
}

namespace miniplayer
{

struct QVideoFrame
{
    AVFrame* frame;

    QVideoFrame(AVFrame* f = nullptr) : frame(f)
    {

    }

    virtual ~QVideoFrame()
    {
        av_frame_free(&frame);
    }
};

struct QI420VideoFrame : public QVideoFrame
{
    QI420VideoFrame() : width( 0 ), height( 0 ),
          yPlane( 0 ), yPlaneSize( 0 ),
          uPlane( 0 ), uPlaneSize( 0 ),
          vPlane( 0 ), vPlaneSize( 0 )
    {

    }

    QI420VideoFrame(AVFrame * f) : QVideoFrame(f)
    {
        width = f->width;
        height = f->height;
        yPlane = f->data[0];
        yPlaneSize = f->height * f->linesize[0];
        uPlane = f->data[1];
        uPlaneSize = (f->height / 2) * f->linesize[1];
        vPlane = f->data[2];
        vPlaneSize = (f->height / 2) * f->linesize[2];
    }

    void clear()
    {
        width = height = 0;
        yPlane = uPlane = vPlane = 0;
        yPlaneSize = uPlaneSize = vPlaneSize = 0;
    }

    bool isValid()
    {
        return width > 0 && height > 0;
    }

    quint16 width;
    quint16 height;

    void* yPlane;
    quint32 yPlaneSize;

    void* uPlane;
    quint32 uPlaneSize;

    void* vPlane;
    quint32 vPlaneSize;
};

}
#endif // QVIDEOFRAME_HPP
