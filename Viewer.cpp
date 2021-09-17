#include "Viewer.h"
#include <QKeyEvent>
#include <QPainter>
#include <QScreen>
#include <QDebug>
#include <QX11Info>
#include <QFileInfo>
#include <QCoreApplication>

#include <xcb/xcb_icccm.h>

Viewer::Viewer(const QString &file) :
    m_image(file)

{
    if (m_image.isNull()) {
        qApp->quit();
        return;
    }

    QSize minSize = m_image.size();
    minSize.scale(100, 100, Qt::KeepAspectRatio);
    setMinimumSize(minSize);

    QSize maxSize = m_image.size();
    maxSize.scale(screen()->availableSize(), Qt::KeepAspectRatioByExpanding);
    setMaximumSize(maxSize);

    setTitle(QFileInfo(file).fileName());
    setFlag(Qt::Dialog);
    updateSize(m_image.size(), true);
}


void Viewer::updateSize(QSize newSize, bool centerOnScreen)
{
    const QSize maxSize(screen()->availableSize());
    if (newSize.width() > maxSize.width()|| newSize.height() >= maxSize.height()) {
        newSize.scale(maxSize.width(), maxSize.height(), Qt::KeepAspectRatio);
    }
    if (newSize.width() < minimumWidth()) {
        newSize.setWidth(minimumWidth());
    }
    if (newSize.height() < minimumHeight()) {
        newSize.setHeight(minimumHeight());
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

    setGeometry(geo);
}

void Viewer::ensureVisible()
{
    const QRect screenGeometry = screen()->availableGeometry();
    QRect newGeometry = geometry();
    if (geometry().left() > screenGeometry.right() - 50) {
        newGeometry.translate(-50, 0);
    } else if (geometry().right() < screenGeometry.left() + 50) {
        newGeometry.translate(50, 0);
    }
    if (geometry().top() > screenGeometry.bottom() - 50) {
        newGeometry.translate(0, -50);
    } else if (geometry().bottom() < screenGeometry.top() + 50) {
        newGeometry.translate(0, 50);
    }
    setPosition(newGeometry.topLeft());
}

void Viewer::setAspectRatio()
{
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_aspect(&hints, m_image.width(), m_image.height(), m_image.width(), m_image.height());
    xcb_icccm_set_wm_normal_hints(QX11Info::connection(), winId(), &hints);
}

void Viewer::paintEvent(QPaintEvent *event)
{
    QPainter p(this);
    p.setClipRegion(event->region());


    QRect rect(QPoint(0, 0), size());
    QRect imageRect = m_scaled.rect();
    imageRect.moveCenter(rect.center());
    p.drawImage(imageRect.topLeft(), m_scaled);

    // This _should_ always be empty
    QRegion background = rect;
    background -= imageRect;
    background &= event->region();
    for (const QRect &r : background) {
        p.fillRect(r, Qt::black);
    }

}

void Viewer::keyPressEvent(QKeyEvent *event)
{
    switch(event->key()) {
    case Qt::Key_Equal:
    case Qt::Key_Plus:
    case Qt::Key_Up:
        updateSize(m_scaled.size()  * 1.1);
        return;
    case Qt::Key_Minus:
    case Qt::Key_Down:
        updateSize(m_scaled.size()  / 1.1);
        return;
    case Qt::Key_Q:
    case Qt::Key_Escape:
        close();
        return;
    default:
        break;
    }
    QRasterWindow::keyPressEvent(event);
}

void Viewer::resizeEvent(QResizeEvent *event)
{
    m_scaled = m_image.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QMetaObject::invokeMethod(this, &Viewer::setAspectRatio, Qt::QueuedConnection);
    QRasterWindow::resizeEvent(event);
    QMetaObject::invokeMethod(this, &Viewer::ensureVisible, Qt::QueuedConnection);
    update();
}

void Viewer::moveEvent(QMoveEvent *event)
{
    QRasterWindow::moveEvent(event);
    QMetaObject::invokeMethod(this, &Viewer::ensureVisible, Qt::QueuedConnection);
}
