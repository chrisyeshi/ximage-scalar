#include "viewer.h"
#include <QMouseEvent>
#include <iostream>
#include <cassert>
#include <sys/time.h>
#include "boost/shared_ptr.hpp"

using namespace std;

Viewer::Viewer(QWidget *parent) :
    QGLWidget(parent),
    vboQuad(QOpenGLBuffer::VertexBuffer),
    texTf(QOpenGLTexture::Target1D),
    texAlpha(QOpenGLTexture::Target1D),
    texArrRaf(NULL), texArrDep(NULL),
    zoomFactor(1.f), texArrNml(NULL), texFeature(NULL)
{
    std::cout << "OpenGL Version: " << this->context()->format().majorVersion() << "." << this->context()->format().minorVersion() << std::endl;
    int maxLayer; glGetIntegerv(GL_MAX_FRAMEBUFFER_LAYERS, &maxLayer);
    std::cout << "GL_MAX_FRAMEBUFFER_LAYERS: " << maxLayer << std::endl;
    setFocusPolicy(Qt::StrongFocus);
}

Viewer::~Viewer()
{
    if (texArrRaf) delete texArrRaf;
    if (texArrDep) delete texArrDep;
    if (texFeature) delete texFeature;
}

//
//
// The interface
//
//

void Viewer::renderRAF(const hpgv::ImageRAF &image)
{
    makeCurrent();
    imageRaf = image;
    updateTexRAF();
    updateTexNormal();
}

void Viewer::getFeatureMap(const hpgv::ImageRAF &image)
{
    imageRaf = image;
    int w = imageRaf.getWidth();
    int h = imageRaf.getHeight();

    featureTracker.setDim(w, h);

    std::vector<float*> deps;
    deps.push_back(image.getDepths().get());
    for (int i = 1; i < nBins; ++i) {
        deps.push_back(deps.front() + w*h*i);
    }

    mask = std::vector<float>(w*h, 0.0f);
    featureTracker.track(deps[8], mask.data());
    nFeatures = featureTracker.getNumFeatures();

    for (auto& p : mask) {
        p /= (float)nFeatures;
    }

    std::cout << "nFeatures: " << nFeatures;

    // clean up
    if (texFeature) {
        assert(!texFeature->isBound());
        delete texFeature; texFeature = NULL;
    }
    // new texture
    texFeature = new QOpenGLTexture(QOpenGLTexture::Target2D);
    texFeature->setSize(w, h);
    texFeature->setFormat(QOpenGLTexture::R32F);
    texFeature->allocateStorage();
    texFeature->setWrapMode(QOpenGLTexture::ClampToEdge);
    texFeature->setMinMagFilters(QOpenGLTexture::Nearest, QOpenGLTexture::Nearest);
    texFeature->setData(QOpenGLTexture::Red, QOpenGLTexture::Float32, mask.data());

//    updateRAF(texFeature, featureMap.get(), w, h);
//    texFeature->setMinMagFilters(QOpenGLTexture::Nearest, QOpenGLTexture::Nearest);

    updateGL();
}

//
//
// Public slots
//
//

void Viewer::tfChanged(mslib::TF& tf)
{
    assert(tf.resolution() == nBins);
    texTf.setData(QOpenGLTexture::RGBA, QOpenGLTexture::Float32, tf.colorMap());

    updateGL();
}

void Viewer::snapshot(const std::string &filename)
{
    makeCurrent();
    QImage image = grabFrameBuffer();
    image.save(filename.c_str(), "PNG");
}

//
//
// Inherited from QGLWidget
//
//

void Viewer::initializeGL()
{
    initQuadVbo();
    updateProgram();
    updateVAO();
    initTF();
    qglClearColor(QColor(0, 0, 0, 0));
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
}

void Viewer::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!texArrRaf) return;

    progRaf.bind();
    vao.bind();
    texTf.bind(0);
    texAlpha.bind(1);
    texArrRaf->bind(2);
    texArrNml->bind(3);
    texArrDep->bind(4);
    texFeature->bind(5);

    progRaf.setUniformValue("invVP", 1.f / float(imageRaf.getWidth()), 1.f / float(imageRaf.getHeight()));
    progRaf.setUniformValue("selectedID", float(SelectedFeature)/nFeatures);
    progRaf.setUniformValue("featureHighlight", int(HighlightFeatures ? 1 : 0));

    glDrawArrays(GL_TRIANGLE_FAN, 0, nVertsQuad);

    texFeature->release();
    texArrDep->release();
    texArrNml->release();
    texArrRaf->release();
    texAlpha.release();
    texTf.release();
    vao.release();
    progRaf.release();
}

void Viewer::resizeGL(int width, int height)
{
    glViewport(0, 0, width, height);
    updateShaderMVP();
}

void Viewer::EnableHighlighting(float x, float y)
{
    cout << "Feature Highlighting Enabled!" << endl;
    HighlightFeatures = true;

    int idx = int(x) + int(y) * imageRaf.getWidth();
    SelectedFeature = int(mask[idx] * nFeatures + 0.5);

    std::cout << "SelectedFeature: " << SelectedFeature << std::endl;

    update();
}

void Viewer::DisableHighlighting()
{
    cout << "Feature Highlighting Disabled!" << endl;
    HighlightFeatures = false;
    update();
}

void Viewer::mousePressEvent(QMouseEvent *e)
{
    cursorPrev = e->localPos();
    int wx = e->x();
    int wy = height() - e->y();
    int x = double(wx) / double(width()) * double(imageRaf.getWidth());
    int y = double(wy) / double(height()) * double(imageRaf.getHeight());

    if (x < 0 || x >= imageRaf.getWidth() || y < 0 || y >= imageRaf.getHeight())
        return;

    if(e->buttons() & Qt::RightButton)
        DisableHighlighting();
    if(e->buttons() & Qt::LeftButton)
        EnableHighlighting(x, y);

}

void Viewer::mouseMoveEvent(QMouseEvent *e)
{
    // camera movement
    if (e->buttons() & Qt::LeftButton)
    {
        QPointF dir = e->localPos() - cursorPrev;
        dir.rx() = dir.x() / float(width()) * zoomFactor * 2.f;
        dir.ry() = -dir.y() / float(height()) * zoomFactor * 2.f;
        focal.rx() -= dir.x();
        focal.ry() -= dir.y();
        cursorPrev = e->localPos();
        updateShaderMVP();
        updateGL();
    } else
    {
        int wx = e->x();
        int wy = height() - e->y();
        int x = double(wx) / double(width()) * double(imageRaf.getWidth());
        int y = double(wy) / double(height()) * double(imageRaf.getHeight());
        if (x < 0 || x >= imageRaf.getWidth() || y < 0 || y >= imageRaf.getHeight())
            return;
        std::cout << "[" << x << "," << y << "]: value: ";
        for (unsigned int i = 0; i < imageRaf.getNBins(); ++i)
            std::cout << imageRaf.getRafs()[imageRaf.getWidth() * imageRaf.getHeight() * i + (y * imageRaf.getWidth() + x)] << ", ";
        std::cout << std::endl;
        std::cout << "[" << x << "," << y << "]: depth: ";
        for (unsigned int i = 0; i < imageRaf.getNBins(); ++i)
            std::cout << imageRaf.getDepths()[imageRaf.getWidth() * imageRaf.getHeight() * i + (y * imageRaf.getWidth() + x)] << ", ";
        std::cout << std::endl;
    }
    // feature extraction/tracking
    if(e->buttons() & Qt::RightButton)
        DisableHighlighting();
    if(e->buttons() & Qt::LeftButton)
        EnableHighlighting(e->x(), e->y());
}

void Viewer::keyPressEvent(QKeyEvent *e)
{
    switch (e->key())
    {
    case Qt::Key_F5:
        std::cout << "F5: update shader program." << std::endl;
        updateProgram();
        updateVAO();
        updateGL();
        break;
    case Qt::Key_Right:
        SelectedFeature++;
        if (SelectedFeature > nFeatures)
            SelectedFeature = 0;
        std::cout << "FID: " << SelectedFeature << std::endl;
        updateGL();
        break;
    case Qt::Key_Left:
        SelectedFeature--;
        if (SelectedFeature < 0)
            SelectedFeature = nFeatures;
        std::cout << "FID: " << SelectedFeature << std::endl;
        updateGL();
        break;
    case Qt::Key_F:
        if (HighlightFeatures)
            DisableHighlighting();
        else
            EnableHighlighting(0, 0);
        break;
    }

}

void Viewer::wheelEvent(QWheelEvent *e)
{
    int numSteps = e->angleDelta().y() / 8 / 15;
    zoomFactor *= 1.f - float(numSteps) / 15.f;
    zoomFactor = std::min(1.f, zoomFactor);
    updateShaderMVP();
    updateGL();
}

//
//
// My functions
//
//

void Viewer::initQuadVbo()
{
    makeCurrent();
    static GLfloat vtx[] = { -1, -1,  1, -1,  1,  1, -1,  1,
                              0,  0,  1,  0,  1,  1,  0,  1};
    // QOpenGLBuffer
    vboQuad.create();
    vboQuad.setUsagePattern(QOpenGLBuffer::StaticDraw);
    vboQuad.bind();
    vboQuad.allocate(vtx, nBytesQuad() * 2);
    vboQuad.release();
}

void Viewer::updateProgram()
{
    progRaf.removeAllShaders();
    progRaf.addShaderFromSourceFile(QOpenGLShader::Vertex,   "../../viewer/shaders/raf.vert");
    progRaf.addShaderFromSourceFile(QOpenGLShader::Fragment, "../../viewer/shaders/new.frag");
    progRaf.link();
    progRaf.bind();
    progRaf.setUniformValue("nBins", nBins);
    progRaf.setUniformValue("mvp", mvp());
    progRaf.setUniformValue("tf", 0);
    progRaf.setUniformValue("rafa", 1);
    progRaf.setUniformValue("rafarr", 2);
    progRaf.setUniformValue("nmlarr", 3);
    progRaf.setUniformValue("deparr", 4);
    progRaf.setUniformValue("featureID", 5);
    progRaf.release();
}

void Viewer::updateShaderMVP()
{
    matView.setToIdentity();
    float w = float(width()) / float(height()) * zoomFactor;
    float h = 1.f * zoomFactor;
    matView.ortho(focal.x() - w, focal.x() + w, focal.y() - h, focal.y() + h, -1.f, 1.f);
    if (!progRaf.isLinked())
        return;
    progRaf.bind();
    progRaf.setUniformValue("mvp", mvp());
    progRaf.release();
}

void Viewer::updateVAO()
{
    vao.create();
    vao.bind();
    vboQuad.bind();
    progRaf.enableAttributeArray("vertex");
    progRaf.setAttributeBuffer("vertex", GL_FLOAT, 0, nFloatsPerVertQuad);
    progRaf.enableAttributeArray("texCoord");
    progRaf.setAttributeBuffer("texCoord", GL_FLOAT, nBytesQuad(), nFloatsPerVertQuad);
    vao.release();
}

void Viewer::initTF()
{
    mslib::TF tf(nBins,nBins);
    // QOpenGLTexture
    texTf.setSize(tf.resolution());
    texTf.setFormat(QOpenGLTexture::RGBA32F);
    texTf.allocateStorage();
    texTf.setData(QOpenGLTexture::RGBA, QOpenGLTexture::Float32, tf.colorMap());
    texTf.setWrapMode(QOpenGLTexture::ClampToEdge);
    texTf.setMinMagFilters(QOpenGLTexture::Nearest, QOpenGLTexture::Nearest);
    // texAlpha
    texAlpha.setSize(tf.resolution());
    texAlpha.setFormat(QOpenGLTexture::R32F);
    texAlpha.allocateStorage();
    texAlpha.setData(QOpenGLTexture::Red, QOpenGLTexture::Float32, tf.alphaArray());
    texAlpha.setWrapMode(QOpenGLTexture::ClampToEdge);
    texAlpha.setMinMagFilters(QOpenGLTexture::Nearest, QOpenGLTexture::Nearest);
}

void Viewer::updateTexRAF()
{
    // RAF
    // clean up
    if (texArrRaf)
    {
        assert(!texArrRaf->isBound());
        delete texArrRaf; texArrRaf = NULL;
    }
    // new texture array
    texArrRaf = new QOpenGLTexture(QOpenGLTexture::Target2DArray);
    texArrRaf->setSize(imageRaf.getWidth(), imageRaf.getHeight());
    texArrRaf->setLayers(imageRaf.getNBins());
    texArrRaf->setFormat(QOpenGLTexture::R32F);
    texArrRaf->allocateStorage();
    texArrRaf->setWrapMode(QOpenGLTexture::ClampToEdge);
    texArrRaf->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
    for (unsigned int layer = 0; layer < imageRaf.getNBins(); ++layer)
    {
        texArrRaf->setData(0, layer, QOpenGLTexture::Red, QOpenGLTexture::Float32,
                &(imageRaf.getRafs().get()[layer * imageRaf.getWidth() * imageRaf.getHeight()]));
    }

    // Depth
    if (texArrDep)
    {
        assert(!texArrDep->isBound());
        delete texArrDep; texArrDep = NULL;
    }
    // new texture array
    texArrDep = new QOpenGLTexture(QOpenGLTexture::Target2DArray);
    texArrDep->setSize(imageRaf.getWidth(), imageRaf.getHeight());
    texArrDep->setLayers(imageRaf.getNBins());
    texArrDep->setFormat(QOpenGLTexture::R32F);
    texArrDep->allocateStorage();
    texArrDep->setWrapMode(QOpenGLTexture::ClampToEdge);
    texArrDep->setMinMagFilters(QOpenGLTexture::Nearest, QOpenGLTexture::Nearest);
    for (unsigned int layer = 0; layer < imageRaf.getNBins(); ++layer)
    {
        texArrDep->setData(0, layer, QOpenGLTexture::Red, QOpenGLTexture::Float32,
                &(imageRaf.getDepths().get()[layer * imageRaf.getWidth() * imageRaf.getHeight()]));
    }
}

void Viewer::updateTexNormal()
{
    // clean up
    if (texArrNml)
    {
        delete texArrNml;
        texArrNml = NULL;
    }
    // texture
    texArrNml = new QOpenGLTexture(QOpenGLTexture::Target2DArray);
    texArrNml->setSize(imageRaf.getWidth(), imageRaf.getHeight());
    texArrNml->setLayers(imageRaf.getNBins());
    texArrNml->setFormat(QOpenGLTexture::RGB32F);
    texArrNml->allocateStorage();
    texArrNml->setWrapMode(QOpenGLTexture::ClampToEdge);
    texArrNml->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
    // calculate normal in CPU -- this is going to be painful
    timeval start; gettimeofday(&start, NULL);
    for (int layer = 0; layer < imageRaf.getNBins(); ++layer)
    {
        int w = imageRaf.getWidth();
        int h = imageRaf.getHeight();
        float* normals = new float [3 * w * h];
        float* depths = &(imageRaf.getDepths().get()[layer * w * h]);
#pragma omp parallel for
        for (int y = 0; y < w; ++y)
#pragma omp parallel for
        for (int x = 0; x < h; ++x)
        {
            // 3 2 1
            // 4 C 0
            // 5 6 7
            float tx = (float(x) + 0.5f) / float(w);
            float ty = (float(y) + 0.5f) / float(h);
            float dx = 1.f / float(w);
            float dy = 1.f / float(h);
            QPoint pos[8];
            pos[0] = QPoint(x+1,y+0);
            pos[1] = QPoint(x+1,y+1);
            pos[2] = QPoint(x+0,y+1);
            pos[3] = QPoint(x-1,y+1);
            pos[4] = QPoint(x-1,y+0);
            pos[5] = QPoint(x-1,y-1);
            pos[6] = QPoint(x+0,y-1);
            pos[7] = QPoint(x+1,y-1);
            for (int i = 0; i < 8; ++i)
            {
                pos[i].rx() = std::min(pos[i].x(), w-1);
                pos[i].rx() = std::max(pos[i].x(), 0);
                pos[i].ry() = std::min(pos[i].y(), h-1);
                pos[i].ry() = std::max(pos[i].y(), 0);
            }
            QVector3D coordsCtr(tx, ty, depths[w*y+x]);
            QVector3D coords[8];
            coords[0] = QVector3D(tx+dx, ty   , depths[w*pos[0].y()+pos[0].x()]);
            coords[1] = QVector3D(tx+dx, ty+dy, depths[w*pos[1].y()+pos[1].x()]);
            coords[2] = QVector3D(tx   , ty+dy, depths[w*pos[2].y()+pos[2].x()]);
            coords[3] = QVector3D(tx-dx, ty+dy, depths[w*pos[3].y()+pos[3].x()]);
            coords[4] = QVector3D(tx-dx, ty   , depths[w*pos[4].y()+pos[4].x()]);
            coords[5] = QVector3D(tx-dx, ty-dy, depths[w*pos[5].y()+pos[5].x()]);
            coords[6] = QVector3D(tx   , ty-dy, depths[w*pos[6].y()+pos[6].x()]);
            coords[7] = QVector3D(tx+dx, ty-dy, depths[w*pos[7].y()+pos[7].x()]);
            QVector3D dirs[8];
            for (int i = 0; i < 8; ++i)
                dirs[i] = coords[i] - coordsCtr;
            QVector3D sum;
            for (int i = 0; i < 7; ++i)
            {
                float delta = 0.001f;
                QVector3D normal;
                if (dirs[i].z() > delta || dirs[i+1].z() > delta)
                    normal = QVector3D(0.f,0.f,0.f);
                else
                    normal = QVector3D::crossProduct(dirs[i],dirs[i+1]);
                sum += normal;
            }
            QVector3D theNormal = sum.normalized();
            normals[3 * (w * y + x) + 0] = (theNormal.x() + 1.f) * 0.5f;
            normals[3 * (w * y + x) + 1] = (theNormal.y() + 1.f) * 0.5f;
            normals[3 * (w * y + x) + 2] = (theNormal.z() + 1.f) * 0.5f;
        }
        texArrNml->setData(0, layer,
                QOpenGLTexture::RGB, QOpenGLTexture::Float32,
                normals);
        delete [] normals;
    }
    timeval end; gettimeofday(&end, NULL);
    double time_normal = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_usec - start.tv_usec) / 1000.0;
    std::cout << "Time: Normal:  " << time_normal << " ms" << std::endl;
}
