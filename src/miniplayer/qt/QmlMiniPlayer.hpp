#ifndef QMLMINIPLAYER_HPP
#define QMLMINIPLAYER_HPP

#include <memory>
#include <deque>
#include <list>

#include <QtQuick>
#include "../MiniPlayer.hpp"
#include "QVideoFrame.hpp"
#include "../output/audio/AudioOutputOpenAL.hpp"

using namespace miniplayer;

class QmlVideoSurface;

class QmlDumpInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int packetBufferSize READ packetBufferSize CONSTANT)
    Q_PROPERTY(int maxPacketBufferSize READ maxPacketBufferSize CONSTANT)
    Q_PROPERTY(int maxFrameQueueSize READ maxFrameQueueSize CONSTANT)
    Q_PROPERTY(int videoPacketQueueSize READ videoPacketQueueSize CONSTANT)
    Q_PROPERTY(int audioPacketQueueSize READ audioPacketQueueSize CONSTANT)
    Q_PROPERTY(int videoFrameQueueSize READ videoFrameQueueSize CONSTANT)
    Q_PROPERTY(int audioFrameQueueSize READ audioFrameQueueSize CONSTANT)
    Q_PROPERTY(double videoClock READ videoClock CONSTANT)
    Q_PROPERTY(double audioClock READ audioClock CONSTANT)
public:    
    explicit QmlDumpInfo(QObject *parent = NULL) : QObject(parent)
    {}

    int packetBufferSize() const { return (int)data.packetBufferSize; }
    int maxPacketBufferSize() const { return (int)data.maxPacketBufferSize; }
    int maxFrameQueueSize() const { return (int)data.maxFrameQueueSize; }
    int videoPacketQueueSize() const { return (int)data.videoPacketQueueSize; }
    int audioPacketQueueSize() const { return (int)data.audioPacketQueueSize; }
    int videoFrameQueueSize() const { return (int)data.videoFrameQueueSize; }
    int audioFrameQueueSize() const { return (int)data.audioFrameQueueSize; }
    double videoClock() const { return data.videoClock; }
    double audioClock() const { return data.audioClock; }
public:
    MiniPlayer::DumpInfo data;
};

class QmlMiniPlayer : public QObject, public QQmlParserStatus, private MiniPlayer::Callback
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(double position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(double duration READ duration)
    Q_PROPERTY(bool seekable READ seekable)
    Q_PROPERTY(float volume READ volume WRITE setVolume)

private:
    AudioOutputOpenAL mAudioOutput;
    std::shared_ptr<MiniPlayer> mPlayer;
    std::list<QmlVideoSurface*> mAttachedSurfaces;
    std::shared_ptr<const QI420VideoFrame> mVideoRenderFrame;
    std::mutex mVideoRenderMutex;

public:
    enum State
    {
        Stopped = MiniPlayer::Stopped ,
        Stopping = MiniPlayer::Stopping,
        Opening = MiniPlayer::Opening,
        Playing = MiniPlayer::Playing,
        Paused =MiniPlayer::Paused
    };
    Q_ENUMS(State)

    explicit QmlMiniPlayer(QQuickItem *parent = nullptr);
    virtual ~QmlMiniPlayer();

    void classBegin() override { qDebug() << __FUNCTION__; }
    void componentComplete() override { qDebug() << __FUNCTION__; }

    static void init();
    static void uninit();

    void registerVideoSurface(QmlVideoSurface* videoSurface);
    void unregisterVideoSurface(QmlVideoSurface* videoSurface);

    State state();
    double position();
    void setPosition(double pos);
    double duration();
    bool seekable();
    float volume();
    void setVolume(float val);

private: //MiniPlayer::Callback
    void onVideoRender(AVFrame * frame);
    void onPositionChanged(double pos);
    void onEndReached();
    void onStateChanged(int state);

public slots:
    void dump(QmlDumpInfo * info);
    void open(const QString & mediaPath);
    void stop();
    void play();
    void pause();
    void togglePause();

private slots:
    void videoFrameUpdated();

signals:
    void positionChanged(double pos);
    void stateChanged(State state);
    void endReached();
};

#endif // QMLMINIPLAYER_HPP
