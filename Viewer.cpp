#include "Viewer.h"
#include <QKeyEvent>
#include <QPainter>
#include <QScreen>
#include <QDebug>

Viewer::Viewer(const QString &file) : QWidget(nullptr),
    m_image(file)

{
    if (m_image.isNull()) {
        return;
    }
    setWindowFlag(Qt::Dialog);
    setWindowFlag(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_NoBackground);
    setAttribute(Qt::WA_OpaquePaintEvent);
    m_scaled = m_image;
    updateSize(m_image.size(), true);

//    m_resizeTimer.setInterval(0);
//    m_resizeTimer.setSingleShot(true);
//    connect(&m_resizeTimer, &QTimer::timeout, this, &Viewer::onResizeTimer);
//    m_resizeTimer2.setInterval(0);
//    m_resizeTimer2.setSingleShot(true);
//    connect(&m_resizeTimer2, &QTimer::timeout, this, &Viewer::onResizeTimer2);
}

Viewer::~Viewer()
{
}


void Viewer::keyPressEvent(QKeyEvent *event)
{
    switch(event->key()) {
    case Qt::Key_Equal:
    case Qt::Key_Plus:
    case Qt::Key_Up:
        updateSize(size()  * 1.1);
        event->accept();
        return;
    case Qt::Key_Minus:
    case Qt::Key_Down:
        updateSize(size()  / 1.1);
        event->accept();
        return;
    case Qt::Key_Q:
    case Qt::Key_Escape:
        close();
        event->accept();
        return;
    default:
        break;
    }

    QWidget::keyPressEvent(event);
}

void Viewer::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    if (height() != m_scaled.height() && size().width() != m_scaled.width()) {
        qWarning() << "REsize event not fired!";
//        m_scaled = m_image.scaled(size(), Qt::KeepAspectRatio);
    }
    QRect imageRect = m_scaled.rect();
    imageRect.moveCenter(rect().center());
    QRegion background = rect();
    background -= imageRect;
    for (const QRect &r : background) {
        p.fillRect(r, Qt::black);
    }
    p.drawImage(imageRect, m_scaled);
}

void Viewer::resizeEvent(QResizeEvent *event)
{
    if (event->size().height() == m_scaled.height() || event->size().width() == m_scaled.width() || m_resizing) {
        m_resizing = false;
        QWidget::resizeEvent(event);
        return;
    }

    qDebug() << "Resized";
    m_scaled = m_image.scaled(event->size(), Qt::KeepAspectRatio);
//    m_resizeTimer.start();
////    QWidget::resizeEvent(event);
//    m_resizing = false;
}

//bool Viewer::event(QEvent *e)
//{
//    qDebug() << e;
//    return QWidget::event(e);
//}
//void Viewer::onResizeTimer2()
//{
//    setFixedSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
//    setMinimumSize(10, 10);
//    resize(m_scaled.size());
//}

//void Viewer::onResizeTimer()
//{
////    QSize newSize = m_image.size();
////    float aspect = float(m_image.size().width()) / m_image.size().height();
////    newSize = QSize(height() * aspect, height());
////    qDebug() << newSize << size();
////    if (m_image.width() < newSize.width()) {
////        newSize.setWidth(m_image.width());
////    } else if (m_image.height() < newSize.height()) {
////        newSize.setHeight(m_image.height());
////    }
//    setFixedSize(m_scaled.size());
//    m_resizeTimer2.start();
////    newSize.scale(size(), Qt::KeepAspectRatio);
////    resize(m_scaled.size());
////    setFixedSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
////    setMinimumSize(10, 10);
//}

void Viewer::updateSize(QSize newSize, bool centerOnScreen)
{
    const QSize maxSize(screen()->availableSize());
    if (newSize.width() > maxSize.width()|| newSize.height() >= maxSize.height()) {
        newSize.scale(maxSize.width(), maxSize.height(), Qt::KeepAspectRatio);
    }
    if (newSize.width() < 10 || newSize.height() < 10 || newSize == size()) {
        qDebug() << "new size" << newSize << size();
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
//    setFixedWidth(newSize.width());
//    setFixedSize(newSize);
//    m_resizeTimer.start();
    m_scaled = m_image.scaled(newSize);

    m_resizing = true;
    setGeometry(geo);
}
