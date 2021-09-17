#include "Viewer.h"
#include <QKeyEvent>
#include <QPainter>
#include <QScreen>
#include <QDebug>

Viewer::Viewer(const QString &file) :
    m_image(file)

{
    setWindowFlag(Qt::Dialog);
    setWindowFlag(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_NoBackground);
    setAttribute(Qt::WA_OpaquePaintEvent);
//    setAttribute(Qt::WA_PaintOnScreen);
    m_scaled = m_image;
    m_geometryTimer.setSingleShot(true);
    m_geometryTimer.setInterval(0);
    connect(&m_geometryTimer, &QTimer::timeout, this, &Viewer::updateGeometry);
    setSize(m_image.size(), true);
//    m_geo = geometry();
}

Viewer::~Viewer()
{
}


void Viewer::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Up) {
        setSize(size()  * 1.1);
    } else if (event->key() == Qt::Key_Down) {
        setSize(size()  / 1.1);
    } else {
        return;
    }
}

void Viewer::paintEvent(QPaintEvent *e)
{
//    qDebug() << "PAINT";
//    qDebug() << "region:" << e->region();
    QPainter p(this);
    if (m_geo != geometry()) {
//        p.translate(pos() - m_geo.topLeft());
        p.translate(m_geo.topLeft() - pos());
//        qDebug() << m_geo << geometry();
//        qDebug() << "Skipping during update";
//        return;
    }
    p.setClipRegion(e->region());
//    qDebug() << geometry() << m_geo;
//    QRect drawRect = m_geo;
//    qDebug()
    p.drawImage(QRect(QPoint(0, 0), m_geo.size()), m_image);
//    p.drawImage(QRect(QPoint(0, 0), size()), m_image);
//    p.drawImage(0, 0, m_scaled);
//    qDebug() << m_scaled.size();
}

//void Viewer::resizeEvent(QResizeEvent *)
//{
//    m_geometryTimer.start();
//}

void Viewer::updateGeometry()
{
    setGeometry(m_geo);
//    m_updating = false;
//    m_scaled = m_image.scaled(size());
//    update();
//    if (height() == m_scaled.height()) {
//        return;
//    }
//    QSize newSize = size().scale()
//    const QSize maxSize(screen()->availableSize());
//    if (newSize.width() >= maxSize.width() || newSize.height() >= maxSize.height()) {
//        return;
//    }
//    m_scaled = m_image.scaledToHeight(height());
//    QRect geo = rect();
//    QPoint center = geo.center();
//    geo.setSize(m_scaled.size());
//    geo.moveCenter(center);
    //    setGeometry(geo);
}

void Viewer::setSize(QSize newSize, bool centerOnScreen)
{
    const QSize maxSize(screen()->availableSize());
    if (newSize.width() > maxSize.width()|| newSize.height() >= maxSize.height()) {
        newSize.scale(maxSize.width(), maxSize.height(), Qt::KeepAspectRatio);
    }
    if (newSize.width() < 10 || newSize.height() < 10) {
        return;
    }

    if (newSize.isEmpty()) {
        return;
    }
    if (newSize == size()) {
        return;
    }
    QRect geo = geometry();
    QPoint center = geo.center();
    geo.setSize(newSize);
    if (centerOnScreen) {
        geo.moveCenter(screen()->geometry().center());
    } else {
        geo.moveCenter(center);
    }
    m_geo = geo;
    update();
//    m_scaled = m_image.scaled(newSize);
//    qDebug() << geometry() << geo << center;
//    setGeometry(geo);
//    resize(geo.size());
//    move(geo.topLeft());
//    m_updating = true;
    if (!m_geometryTimer.isActive()) {
        m_geometryTimer.start();
    }
//    m_updating = false;
//    update();
}

//QSize Viewer::preferredSize() const
//{
////    QSize s = size();
////    s.scale(height(), height(), Qt::KeepAspectRatioByExpanding);
////    return s;
//}
