#ifndef QMLVIDEOSURFACE_HPP
#define QMLVIDEOSURFACE_HPP

#include <memory>
#include <QtQuick>

#include "QVideoFrame.hpp"

using namespace miniplayer;

class QmlMiniPlayer;

class QmlVideoSurface : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(FillMode fillMode READ fillMode WRITE setFillMode NOTIFY fillModeChanged)
    Q_PROPERTY(QmlMiniPlayer * source READ source WRITE setSource NOTIFY sourceChanged)
public:
    explicit QmlVideoSurface(QQuickItem *parent = nullptr);
    virtual ~QmlVideoSurface();

    enum FillMode
    {
        Stretch            = Qt::IgnoreAspectRatio,
        PreserveAspectFit  = Qt::KeepAspectRatio,
        PreserveAspectCrop = Qt::KeepAspectRatioByExpanding
    };
    Q_ENUMS(FillMode)

    FillMode fillMode() const { return mFillMode; }
    void setFillMode(FillMode mode);

    QmlMiniPlayer* source() const;
    void setSource(QmlMiniPlayer* source);
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data);

public slots:
    void presentFrame(const std::shared_ptr<const QI420VideoFrame>& frame);

signals:
    void sourceChanged();
    void fillModeChanged(FillMode mode);

private:
    FillMode mFillMode;
    QmlMiniPlayer * mSource;
    bool mFrameUpdated;
    std::shared_ptr<const QI420VideoFrame> mFrame;
};


#endif // QMLVIDEOSURFACE_HPP
