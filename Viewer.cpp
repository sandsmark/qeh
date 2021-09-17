#include "Viewer.h"
#include <QKeyEvent>
#include <QPainter>
#include <QScreen>
#include <QDebug>
#include <QX11Info>
#include <QFileInfo>

#include <xcb/xcb_icccm.h>

Viewer::Viewer(const QString &file) : QWidget(nullptr),
    m_image(file)

{
    if (m_image.isNull()) {
        return;
    }
    setWindowTitle(QFileInfo(file).fileName());
    setWindowFlag(Qt::Dialog);
//    setWindowFlag(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_NoBackground);
    setAttribute(Qt::WA_OpaquePaintEvent);
    updateSize(m_image.size(), true);
}


void Viewer::keyPressEvent(QKeyEvent *event)
{
    switch(event->key()) {
    case Qt::Key_Equal:
    case Qt::Key_Plus:
    case Qt::Key_Up:
        updateSize(m_scaled.size()  * 1.1);
        event->accept();
        return;
    case Qt::Key_Minus:
    case Qt::Key_Down:
        updateSize(m_scaled.size()  / 1.1);
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
    if (event->size().height() == m_scaled.height() || event->size().width() == m_scaled.width()) {
        QWidget::resizeEvent(event);
        return;
    }

    m_scaled = m_image.scaled(event->size(), Qt::KeepAspectRatio);
}

void Viewer::updateSize(QSize newSize, bool centerOnScreen)
{
    const QSize maxSize(screen()->availableSize());
    if (newSize.width() > maxSize.width()|| newSize.height() >= maxSize.height()) {
        newSize.scale(maxSize.width(), maxSize.height(), Qt::KeepAspectRatio);
    }
    if (newSize.width() < 10 || newSize.height() < 10 || newSize == size()) {
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

    m_scaled = m_image.scaled(newSize);

    setGeometry(geo);
}


void Viewer::showEvent(QShowEvent *event)
{
    if (m_initialized) {
        QWidget::showEvent(event);
        return;
    }
    m_initialized = true;
    QWidget::showEvent(event);

    QTimer::singleShot(0, this, [this]() {
        xcb_size_hints_t hints;
        memset(&hints, 0, sizeof(hints));
        xcb_icccm_size_hints_set_aspect(&hints, m_image.width(), m_image.height(), m_image.width(), m_image.height());
        xcb_icccm_set_wm_normal_hints(QX11Info::connection(), winId(), &hints);
    });
}
