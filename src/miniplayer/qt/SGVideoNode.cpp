#include "SGVideoNode.hpp"

using namespace miniplayer;


char const* const* QSGVideoFrameMaterialShader::attributeNames() const
{
    static const char *names[] =
    {
        "a_vertex",
        "a_texcoord",
        0
    };
    return names;
}

const char* QSGVideoFrameMaterialShader::vertexShader() const
{
    return
        "uniform highp mat4 matrix;"
        "attribute highp vec4 a_vertex;"
        "attribute highp vec2 a_texCoord;"
        "varying highp vec2 v_texCoord;"
        "void main() {"
        "    v_texCoord = a_texCoord;"
        "    gl_Position = matrix * a_vertex;"
        "}";
}

const char* QSGVideoFrameMaterialShader::fragmentShader() const
{
    return
        "uniform mediump mat4 rgbMatrix;"
        "uniform lowp float opacity;"
        "uniform sampler2D yPlaneTex;"
        "uniform sampler2D uPlaneTex;"
        "uniform sampler2D vPlaneTex;"
        "varying highp vec2 v_texCoord;"
        "void main() {"
        "    mediump float y = texture2D(yPlaneTex, v_texCoord).r;"
        "    mediump float u = texture2D(uPlaneTex, v_texCoord).r;"
        "    mediump float v = texture2D(vPlaneTex, v_texCoord).r;"
        "    gl_FragColor = vec4(y - .0625, u - .5, v - .5, 1.) * rgbMatrix * opacity;"
        "}";
}

void QSGVideoFrameMaterialShader::initialize()
{
    mMatrixId = program()->uniformLocation("matrix");
    mRgbMatrixId = program()->uniformLocation("rgbMatrix");
    mOpacityId = program()->uniformLocation("opacity");
    mYPlaneTexId = program()->uniformLocation("yPlaneTex");
    mUPlaneTexId = program()->uniformLocation("uPlaneTex");
    mVPlaneTexId = program()->uniformLocation("vPlaneTex");
}

void QSGVideoFrameMaterialShader::updateState(const RenderState& state, QSGMaterial* newMaterial, QSGMaterial* /*oldMaterial*/)
{
    if(state.isOpacityDirty())
        program()->setUniformValue(mOpacityId, GLfloat(state.opacity()));

    if(state.isMatrixDirty())
        program()->setUniformValue(mMatrixId, state.combinedMatrix());

    static const QMatrix4x4 rgbMatrix
      ( 1.164f,  1.164f,  1.164f,   .0f,
         .0f,   -0.391f,  2.018f,   .0f,
        1.596f, -0.813f,   .0f,     .0f,
         .0f,     .0f,     .0f,    1.0f );

    program()->setUniformValue(mRgbMatrixId, rgbMatrix);

    QSGVideoFrameMaterial* fm = static_cast<QSGVideoFrameMaterial*>(newMaterial);

    program()->setUniformValue(mYPlaneTexId, 0);
    program()->setUniformValue(mUPlaneTexId, 1);
    program()->setUniformValue(mVPlaneTexId, 2);

    fm->bindPlanes();
}

//------------------------------------------------------------------------------------------------------

QSGVideoFrameMaterial::QSGVideoFrameMaterial()
{
    memset(mPlaneTexIds, 0, sizeof(mPlaneTexIds));
    setFlag(Blending, false);
}

QSGVideoFrameMaterial::~QSGVideoFrameMaterial()
{
    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    if(mPlaneTexIds[0] || mPlaneTexIds[1] || mPlaneTexIds[2])
        f->glDeleteTextures(sizeof(mPlaneTexIds) / sizeof(mPlaneTexIds[0]), mPlaneTexIds);
}

QSGMaterialType* QSGVideoFrameMaterial::type() const
{
    static QSGMaterialType theType;
    return &theType;
}

QSGMaterialShader* QSGVideoFrameMaterial::createShader() const
{
    return new QSGVideoFrameMaterialShader;
}

int QSGVideoFrameMaterial::compare(const QSGMaterial* other) const
{
    const QSGVideoFrameMaterial* m = static_cast<const QSGVideoFrameMaterial*>(other);
    const unsigned texCount = sizeof(mPlaneTexIds) / sizeof(mPlaneTexIds[0]);

    for(unsigned i = 0; i < texCount; ++i)
    {
        if(mPlaneTexIds[i] != m->mPlaneTexIds[i])
            return mPlaneTexIds[i] - m->mPlaneTexIds[i];
    }

    return 0;
}

void QSGVideoFrameMaterial::setFrame(const std::shared_ptr<const QI420VideoFrame>& frame)
{
    mFrame = frame;
}

void QSGVideoFrameMaterial::bindPlanes()
{
    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    if(0 == mPlaneTexIds[0] && 0 == mPlaneTexIds[1] && 0 == mPlaneTexIds[2])
        f->glGenTextures(sizeof(mPlaneTexIds) / sizeof(mPlaneTexIds[0]), mPlaneTexIds);

    std::shared_ptr<const QI420VideoFrame> tmpFrame;
    mFrame.swap(tmpFrame);

    if(tmpFrame)
    {
        Q_ASSERT(0 == (tmpFrame->width & 1) && 0 == (tmpFrame->height&1));//width and height should be even
        const quint16 tw = tmpFrame->width;
        const quint16 th = tmpFrame->height;

        bindPlane(GL_TEXTURE1, mPlaneTexIds[1], tmpFrame->uPlane, tw / 2, th / 2);
        bindPlane(GL_TEXTURE2, mPlaneTexIds[2], tmpFrame->vPlane, tw / 2, th / 2);
        bindPlane(GL_TEXTURE0, mPlaneTexIds[0], tmpFrame->yPlane, tw, th);
    }
    else
    {
        bindPlane(GL_TEXTURE1, mPlaneTexIds[1], 0, 0, 0);
        bindPlane(GL_TEXTURE2, mPlaneTexIds[2], 0, 0, 0);
        bindPlane(GL_TEXTURE0, mPlaneTexIds[0], 0, 0, 0);
    }
}

void QSGVideoFrameMaterial::bindPlane(GLenum texUnit, GLuint texId, const void* plane, quint16 width, quint16 height)
{
    QOpenGLFunctions* f = QOpenGLContext::currentContext()->functions();
    f->glActiveTexture(texUnit);
    f->glBindTexture(GL_TEXTURE_2D, texId);
    if(plane)
    {
        f->glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, width, height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, plane);
        f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        f->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
}

//------------------------------------------------------------------------------------------------------

SGVideoNode::SGVideoNode()
    : mGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4)
{
    setGeometry(&mGeometry);
    setMaterial(&mMaterial);
}

void SGVideoNode::setFrame(const std::shared_ptr<const QI420VideoFrame>& frame)
{
    mMaterial.setFrame(frame);
    markDirty(QSGNode::DirtyMaterial);
}

void SGVideoNode::setRect(const QRectF& rect, const QRectF& sourceRect)
{
    if(rect.width() == 0 || rect.height() == 0
            || sourceRect.width() == 0 || sourceRect.height() == 0)
        return;

    QSGGeometry::updateTexturedRectGeometry(&mGeometry, rect, sourceRect);
    markDirty(QSGNode::DirtyGeometry);
}
