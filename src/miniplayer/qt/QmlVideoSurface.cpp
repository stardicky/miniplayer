#include "QmlVideoSurface.hpp"
#include "QmlMiniPlayer.hpp"
#include "SGVideoNode.hpp"

QmlVideoSurface::QmlVideoSurface(QQuickItem *parent)
    : QQuickItem(parent), mFillMode(PreserveAspectFit), mFrameUpdated(false), mSource(nullptr)
{
    qDebug() << __FUNCTION__;
    setFlag(QQuickItem::ItemHasContents, true);
}

QmlVideoSurface::~QmlVideoSurface()
{
    qDebug() << __FUNCTION__;
    setSource(nullptr);
}

void QmlVideoSurface::setFillMode(FillMode mode)
{
    qDebug() << __FUNCTION__;
    if(mFillMode == mode)
        return;

    mFillMode = mode;
    update();
    emit fillModeChanged(mode);
}


QmlMiniPlayer* QmlVideoSurface::source() const {
    return mSource;
}

void QmlVideoSurface::setSource(QmlMiniPlayer * source)
{
    qDebug() << __FUNCTION__;
    if(mSource == source)
        return;

    if(mSource)
        mSource->unregisterVideoSurface(this);
    mSource = source;
    if(mSource)
        mSource->registerVideoSurface(this);
    emit sourceChanged();
}

QSGNode* QmlVideoSurface::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    SGVideoNode* node = static_cast<SGVideoNode*>(oldNode);
    if(!mFrame)
    {
        delete node;
        return nullptr;
    }

    if(!node)
        node = new SGVideoNode;

    QRectF outRect(0, 0, width(), height());
    QRectF srcRect(0, 0, 1., 1.);

    if(Stretch != fillMode())
    {
        const uint16_t fw = mFrame->width;
        const uint16_t fh = mFrame->height;

        const qreal frameAspect = qreal(fw) / fh;
        const qreal itemAspect = width() / height();

        if(PreserveAspectFit == fillMode())
        {
            qreal outWidth = width();
            qreal outHeight = height();
            if(frameAspect > itemAspect)
                outHeight = outWidth / frameAspect;
            else if(frameAspect < itemAspect)
                outWidth = outHeight * frameAspect;

            outRect = QRectF((width() - outWidth) / 2, (height() - outHeight) / 2, outWidth, outHeight);
        }
        else if(PreserveAspectCrop == fillMode())
        {
            if(frameAspect > itemAspect)
            {
                srcRect.setX((1. - itemAspect / frameAspect) / 2);
                srcRect.setWidth(1. - srcRect.x() - srcRect.x());
            }
            else if(frameAspect < itemAspect)
            {
                srcRect.setY((1. - frameAspect / itemAspect) / 2);
                srcRect.setHeight(1. - srcRect.y() - srcRect.y());
            }
        }
    }

    if(mFrameUpdated)
    {
        node->setFrame(mFrame);
        mFrameUpdated = false;
    }
    node->setRect(outRect, srcRect);

    return node;
}

void QmlVideoSurface::presentFrame(const std::shared_ptr<const QI420VideoFrame>& frame)
{
    mFrame = frame;
    mFrameUpdated = true;
    update();
}
