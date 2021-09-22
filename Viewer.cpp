#include "Viewer.h"
#include <QKeyEvent>
#include <QPainter>
#include <QScreen>
#include <QDebug>
#include <QX11Info>
#include <QImageReader>

#ifdef DEBUG_LOAD_TIME
#include <QElapsedTimer>
#endif//DEBUG_LOAD_TIME

#include <xcb/xcb_icccm.h>

Viewer::Viewer(const QString &file)
{
#ifdef DEBUG_LOAD_TIME
    QElapsedTimer t; t.start();
#endif

    QImageReader reader(file);
    reader.setQuality(0);
    if (!reader.canRead()) {
        m_error = reader.error();
        qWarning().noquote() << "Can't read image from" << file << ":" << reader.errorString();
        return;
    }
    m_imageSize = reader.size();
    qDebug() << m_imageSize;
    if (reader.supportsAnimation()) {
        m_movie.reset(new QMovie(file));
        m_movie->setScaledSize(m_imageSize);
        m_movie->setCacheMode(QMovie::CacheAll);
        connect(m_movie.get(), &QMovie::frameChanged, this, [this]() { update(); }); // QRasterWindow has broken support for QEvent::UpdateRequest...
        connect(m_movie.get(), &QMovie::finished, m_movie.get(), &QMovie::start);
        QMetaObject::invokeMethod(m_movie.get(), &QMovie::start);
    } else {
        m_image = reader.read();
    }
#ifdef DEBUG_LOAD_TIME
    qDebug() << "Image loaded in" << t.elapsed() << "ms";
#endif//DEBUG_LOAD_TIME
    QSize minSize = m_imageSize;
    minSize.scale(100, 100, Qt::KeepAspectRatio);
    setMinimumSize(minSize);

    QSize maxSize = m_imageSize;
    maxSize.scale(screen()->availableSize() * 2, Qt::KeepAspectRatioByExpanding);
    setMaximumSize(maxSize);

    setFlag(Qt::Dialog);
    updateSize(m_imageSize, true);
}


void Viewer::updateSize(QSize newSize, bool initial)
{
    QSize maxSize(screen()->availableSize());
    if (!initial) {
        maxSize *= 2; // let the user go wild
    }
    if (newSize.width() > maxSize.width() || newSize.height() >= maxSize.height()) {
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
    if (initial) {
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
    xcb_icccm_size_hints_set_aspect(&hints, m_imageSize.width(), m_imageSize.height(), m_imageSize.width(), m_imageSize.height());
    xcb_icccm_set_wm_normal_hints(QX11Info::connection(), winId(), &hints);
}

void Viewer::paintEvent(QPaintEvent *event)
{
    QPainter p(this);
    p.setClipRegion(event->region());

    QRect rect(QPoint(0, 0), size());
    QRect imageRect;
    if (m_movie) {
        imageRect = QRect(QPoint(0, 0), m_movie->scaledSize());
        imageRect.moveCenter(rect.center());
        p.drawImage(imageRect.topLeft(), m_movie->currentImage());
    } else {
        imageRect = m_scaled.rect();
        imageRect.moveCenter(rect.center());
        p.drawImage(imageRect.topLeft(), m_scaled);
    }

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
    case Qt::Key_1: updateSize(m_imageSize  * 0.1); return;
    case Qt::Key_2: updateSize(m_imageSize  * 0.2); return;
    case Qt::Key_3: updateSize(m_imageSize  * 0.3); return;
    case Qt::Key_4: updateSize(m_imageSize  * 0.4); return;
    case Qt::Key_5: updateSize(m_imageSize  * 0.5); return;
    case Qt::Key_6: updateSize(m_imageSize  * 0.6); return;
    case Qt::Key_7: updateSize(m_imageSize  * 0.7); return;
    case Qt::Key_8: updateSize(m_imageSize  * 0.8); return;
    case Qt::Key_9: updateSize(m_imageSize  * 0.9); return;
    case Qt::Key_0: updateSize(m_imageSize); return;
    case Qt::Key_Equal:
    case Qt::Key_Plus:
    case Qt::Key_Up:
        if (m_movie) {
            updateSize(m_movie->scaledSize() * 1.1);
        } else {
            updateSize(m_scaled.size() * 1.1);
        }
        return;
    case Qt::Key_Minus:
    case Qt::Key_Down:
        if (m_movie) {
            updateSize(m_movie->scaledSize() / 1.1);
        } else {
            updateSize(m_scaled.size() / 1.1);
        }
        return;
    case Qt::Key_F: {
        QSize newSize = m_imageSize;
        newSize.scale(screen()->availableSize(), Qt::KeepAspectRatio);
        updateSize(newSize);
        return;
    }
    case Qt::Key_Q:
    case Qt::Key_Escape:
        close();
        return;
    case Qt::Key_Right: {
        QRect geom = geometry();
        const QRect screenGeo = screen()->availableGeometry();
        if (geom.left() == screenGeo.left()) {
            geom.moveCenter(screenGeo.center());
        } else {
            geom.moveRight(screenGeo.right());
        }
        setGeometry(geom);
        break;
    }
    case Qt::Key_Left: {
        QRect geom = geometry();
        const QRect screenGeo = screen()->availableGeometry();
        if (geom.right() == screenGeo.right()) {
            geom.moveCenter(screenGeo.center());
        } else {
            geom.moveLeft(screenGeo.left());
        }
        setGeometry(geom);
        break;
    }
    default:
        break;
    }
    QRasterWindow::keyPressEvent(event);
}

void Viewer::resizeEvent(QResizeEvent *event)
{
    if (m_movie) {
        m_movie->setScaledSize(m_movie->scaledSize().scaled(size(), Qt::KeepAspectRatio));
    } else {
        const Qt::TransformationMode mode = m_image.width() / width() > 2 ? Qt::FastTransformation : Qt::SmoothTransformation;
        m_scaled = m_image.scaled(size(), Qt::KeepAspectRatio, mode);
    }
    //if (m_image.width() / width() > 2) {
    //    m_scaled = m_image.scaled(size(), Qt::KeepAspectRatio, Qt::FastTransformation);
    //} else {
    //    m_scaled = m_image.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    //}
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

bool Viewer::event(QEvent *ev)
{
    switch(ev->type()) {
    case QEvent::UpdateRequest:
        return QRasterWindow::event(ev);
    case QEvent::FocusIn:
    case QEvent::FocusOut:
        // Avoid QWindow connecting to dbus
        ev->accept();
        return true;
    default:
        return QRasterWindow::event(ev);
    }
}
