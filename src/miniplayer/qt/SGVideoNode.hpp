#ifndef SGVIDEONODE_HPP
#define SGVIDEONODE_HPP

#include <QtQuick>
#include "QVideoFrame.hpp"


namespace miniplayer
{

class QSGVideoFrameMaterialShader : public QSGMaterialShader
{
public:
    virtual char const* const* attributeNames() const;
    virtual const char* vertexShader() const;
    virtual const char* fragmentShader() const;
    virtual void initialize();
    virtual void updateState(const RenderState& state, QSGMaterial* newMaterial, QSGMaterial* oldMaterial);

private:
    int mMatrixId;
    int mRgbMatrixId;
    int mOpacityId;
    int mYPlaneTexId;
    int mUPlaneTexId;
    int mVPlaneTexId;
};

//------------------------------------------------------------------------------------------------------

class QSGVideoFrameMaterial : public QSGMaterial
{
public:
    QSGVideoFrameMaterial();
    ~QSGVideoFrameMaterial();

    virtual QSGMaterialType* type() const;
    virtual QSGMaterialShader* createShader() const;
    virtual int compare(const QSGMaterial *other) const;

    void setFrame(const std::shared_ptr<const QI420VideoFrame>& frame);
    void bindPlanes();

private:
    void bindPlane(GLenum texUnit, GLuint texId, const void* plane, quint16 width, quint16 height);

private:
    std::shared_ptr<const QI420VideoFrame> mFrame;
    GLuint mPlaneTexIds[3];
};

//------------------------------------------------------------------------------------------------------

class SGVideoNode : public QSGGeometryNode
{
public:
    SGVideoNode();
    void setRect( const QRectF& rect, const QRectF& sourceRect );
    void setFrame( const std::shared_ptr<const QI420VideoFrame>& frame );

private:
    QSGGeometry mGeometry;
    QSGVideoFrameMaterial mMaterial;
};

}

#endif // SGVIDEONODE_HPP
