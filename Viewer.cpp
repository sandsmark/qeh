#include "Viewer.h"
#include <QKeyEvent>
#include <QPainter>
#include <QScreen>
#include <QDebug>
#include <QX11Info>
#include <QFileInfo>
#include <QBackingStore>
#include <QCoreApplication>

#include <xcb/xcb_icccm.h>

Viewer::Viewer(const QString &file) :
    m_image(file)

{
    if (m_image.isNull()) {
        qApp->quit();
        return;
    }
    m_backingStore = new QBackingStore(this);
    setTitle(QFileInfo(file).fileName());
    setFlag(Qt::Dialog);
    updateSize(m_image.size(), true);
    setMinimumSize(QSize(10, 10));
}


void Viewer::updateSize(QSize newSize, bool centerOnScreen)
{
    const QSize maxSize(screen()->availableSize());
    if (newSize.width() > maxSize.width()|| newSize.height() >= maxSize.height()) {
        newSize.scale(maxSize.width(), maxSize.height(), Qt::KeepAspectRatio);
    }
    if (newSize.width() < 10) {
        newSize.setWidth(10);
    }
    if (newSize.height() < 10) {
        newSize.setHeight(10);
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

    setGeometry(geo);
}

void Viewer::setAspectRatio()
{
    xcb_size_hints_t hints;
    memset(&hints, 0, sizeof(hints));
    xcb_icccm_size_hints_set_aspect(&hints, m_image.width(), m_image.height(), m_image.width(), m_image.height());
    xcb_icccm_set_wm_normal_hints(QX11Info::connection(), winId(), &hints);
}

void Viewer::render(const QRegion &region)
{
    if (!isExposed()) {
        return;
    }

    if (m_backingStore->size() != size()) {
        // Resize event pending
        return;
    }
    QRect rect(QPoint(0, 0), m_backingStore->size());
    m_backingStore->beginPaint(region.boundingRect());
    QPaintDevice *device = m_backingStore->paintDevice();
    if (!device) {
        qWarning() << "No paintdevice!";
        return;
    }
    QPainter p(device);
    p.setClipRegion(region);


    QRect imageRect = m_scaled.rect();
    imageRect.moveCenter(rect.center());
    p.drawImage(imageRect.topLeft(), m_scaled);

    // This _should_ always be empty
    QRegion background = rect;
    background -= imageRect;
    background &= region;
    for (const QRect &r : background) {
        p.fillRect(r, Qt::black);
    }

    p.end();
    m_backingStore->endPaint();
    m_backingStore->flush(rect);
}

bool Viewer::event(QEvent *event)
{
    switch(event->type()) {
    case QEvent::UpdateRequest: // I don't think we ever get this?
        render(QRect(0, 0, width(), height()));
        return true;
    case QEvent::Expose:
        render(static_cast<QExposeEvent*>(event)->region());
        return true;
    case QEvent::KeyPress:
        switch(static_cast<QKeyEvent*>(event)->key()) {
        case Qt::Key_Equal:
        case Qt::Key_Plus:
        case Qt::Key_Up:
            updateSize(m_scaled.size()  * 1.1);
            return true;
        case Qt::Key_Minus:
        case Qt::Key_Down:
            updateSize(m_scaled.size()  / 1.1);
            return true;
        case Qt::Key_Q:
        case Qt::Key_Escape:
            close();
            return true;
        default:
            break;
        }
        break;
    case QEvent::Resize:
        m_backingStore->resize(size());
        m_scaled = m_image.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);

        // Qt helpfully resets this when resizing, and has no option for us to set it
        QMetaObject::invokeMethod(this, &Viewer::setAspectRatio, Qt::QueuedConnection);
        render(QRect(0, 0, width(), height()));
        return true;
    default:
        break;
    }

    return QWindow::event(event);
}
