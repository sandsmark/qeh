#include "Viewer.h"

#include "imgeffects.h"

#include <QKeyEvent>
#include <QPainter>
#include <QScreen>
#include <QDebug>
#include <QX11Info>
#include <QImageReader>
#include <QGuiApplication>
#include <QBuffer>
#include <QMetaEnum>
#include <QColorSpace>
#include <QElapsedTimer>

#ifdef DEBUG_LOAD_TIME
#include <QElapsedTimer>
#endif//DEBUG_LOAD_TIME

extern "C" {
#include <xcb/xcb_icccm.h>
#include <unistd.h>
}

Viewer::Viewer()
{
    qRegisterMetaType<QImageReader::ImageReaderError>("QImageReader::ImageReaderError");
    setFlag(Qt::Dialog);
}

bool Viewer::load(const QString &filename)
{
#ifdef DEBUG_LOAD_TIME
    QElapsedTimer t; t.start();
#endif
    if (filename == "-") {
        // QMovie requires seeking, so we have to read in and store everything
        // in a qbuffer.
        QBuffer *buffer = new QBuffer(this);
        m_input = buffer;

        QFile stdinFile;
        stdinFile.open(STDIN_FILENO, QIODevice::ReadOnly, QFileDevice::DontCloseHandle);
        buffer->setData(stdinFile.readAll());
    } else {
        m_input = new QFile(filename, this);
    }

    if (!m_input->open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open" << filename << m_input->errorString();
        return false;
    }

    QImageReader reader(m_input);

    if (!reader.canRead()) {
        m_error = reader.error();
        qWarning().noquote() << "Error trying to read image from" << filename << ":" << reader.errorString();
    }
    m_format = reader.format();
    if (!reader.subType().isEmpty()) {
        m_format += "/" + reader.subType();
    }
    m_imageSize = reader.size();

    if (reader.supportsAnimation()) {
        static const QSet<QByteArray> brokenFormats = {
            "mng"
        };

        resetMovie();
        m_brokenFormat = brokenFormats.contains(m_movie->format());
        if (m_brokenFormat) {
            qWarning() << m_movie->format() << "has issues, playback might get janky";
        }

        if (!m_imageSize.isValid()) {
            m_movie->start();
            m_imageSize = m_movie->currentImage().size();
            if (!m_imageSize.isValid()) {
                m_imageSize = QSize(10, 10);
            }
        }
        if (m_imageSize.isValid()) {
            m_movie->setScaledSize(m_imageSize);
        } else {
            qWarning() << "Failed to get size from animated image";
            return false;
        }

        QMetaObject::invokeMethod(m_movie.get(), &QMovie::start);
    } else {
        reader.read(&m_image);
        if (m_image.isNull()) {
            qWarning() << "Image reader error:" << reader.errorString();
            return false;
        }
        m_imageSize = m_image.size();
    }
    m_scaledSize = m_imageSize;
#ifdef DEBUG_LOAD_TIME
    qDebug() << "Image loaded in" << t.elapsed() << "ms";
#endif//DEBUG_LOAD_TIME
    QSize minSize = m_imageSize;
    minSize.scale(100, 100, Qt::KeepAspectRatio);
    setMinimumSize(minSize);

    QSize maxSize = m_imageSize;
    maxSize.scale(screen()->availableSize() * 2, Qt::KeepAspectRatioByExpanding);
    setMaximumSize(maxSize);

    updateSize(m_imageSize, true);

    return true;
}

void Viewer::resetMovie()
{
    m_movie.reset();
    m_input->seek(0);
    m_movie.reset(new QMovie(m_input));

    m_movie->setCacheMode(QMovie::CacheAll);

    if (m_imageSize.isValid()) {
        m_movie->setScaledSize(m_imageSize);
    }

    connect(m_movie.get(), &QMovie::error, this, [this]() { qWarning() << "ERROR" << m_movie->lastErrorString(); });

    connect(m_movie.get(), &QMovie::frameChanged, this, [this](int frameNum) {
        update();

        if (!m_brokenFormat) {
            return;
        }
        if (m_movie->frameCount() > 0 && frameNum >= m_movie->frameCount() - 1) {
            QMetaObject::invokeMethod(this, &Viewer::onMovieFinished, Qt::QueuedConnection);
            return;
        }
        if (frameNum > 0) {
            m_failed = false;
        }
    });

    connect(m_movie.get(), &QMovie::finished, this, &Viewer::onMovieFinished);
}

void Viewer::onMovieFinished()
{
    if (m_failed) {
        qWarning() << "Finished but failed, not reseting";
        return;
    }
    if (m_brokenFormat) {
        m_failed = true;
        resetMovie();
    }
    QMetaObject::invokeMethod(m_movie.get(), &QMovie::start);
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
    const QPoint center = geo.center();
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

template <typename ENUM> static const QString enumToString(const ENUM val)
{
    const QMetaEnum metaEnum = QMetaEnum::fromType<ENUM>();
    return metaEnum.valueToKey(static_cast<typename std::underlying_type<ENUM>::type>(val));
}

void Viewer::paintEvent(QPaintEvent *event)
{
    QPainter p(this);
    p.setClipRegion(event->region());

    const QRect rect(QPoint(0, 0), size());
    p.setCompositionMode(QPainter::CompositionMode_Source);

    QRect imageRect;
    QImage image;
    if (m_movie) {
        imageRect = QRect(QPoint(0, 0), m_scaledSize);
        imageRect.moveCenter(rect.center());
        if (m_movie->state() != QMovie::Running || m_brokenFormat) {
            image = m_movie->currentImage().scaled(m_scaledSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        } else {
            image =  m_movie->currentImage();
        }
        if (m_effect == Equalize) {
            equalize(image);
        } else if (m_effect == Normalize) {
            normalize(image);
        }
    } else {
        imageRect = m_scaled.rect();
        imageRect.moveCenter(rect.center());

        image = m_scaled;
    }
    p.drawImage(imageRect.topLeft(), image);

    // This _should_ always be empty
    QRegion background = rect;
    background -= imageRect;
    background &= event->region();
    for (const QRect &r : background) {
        p.fillRect(r, Qt::black);
    }
    if (m_showInfo) {
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        QString infoText;
        infoText += enumToString(image.format());
        infoText += "\nSize: " + QString::asprintf("%dx%d", m_imageSize.width(), m_imageSize.height());
        infoText += "\nFormat: " + m_format;

        QColorSpace colors = image.colorSpace();
        if (colors.isValid() || image.colorCount()) {
            infoText += "\nColors:";
            if (colors.gamma()) {
                infoText += "\n  Gamma: " + QString::number(colors.gamma());
            }
            if (colors.isValid()) {
                infoText += "\n  Primaries: " + enumToString(colors.primaries());
                infoText += "\n  Transfer function: " + enumToString(colors.transferFunction());
            }
            if (image.colorCount()) {
                infoText += "\n  Color count: " + QString::number(image.colorCount());
            }
        }

        if (!image.textKeys().isEmpty()) {
            infoText += "\nMetadata:";
            for (const QString &key : image.textKeys()) {
                QString text = image.text(key).simplified();
                //text.remove("\n");
                if (text.length() > 25) {
                    text = text.mid(0, 22) + "...";
                }
                infoText += "\n - " + key + ": " + text;
            }
        }

        if (m_movie) {
            infoText += "\nFrame: " + QString::number(m_movie->currentFrameNumber());
            if (m_movie->frameCount()) {
                infoText += "/" + QString::number(m_movie->frameCount());
            }
            if (m_movie->speed() != 100) {
                infoText += "\nSpeed: " + QString::number(m_movie->speed()) + "%";
            }
            infoText += "\n" + enumToString(m_movie->state());
        }
        QRect textRect = p.boundingRect(rect, Qt::AlignLeft | Qt::AlignTop, infoText);
        textRect += QMargins(2, 2, 2, 2);
        textRect.moveTo(0, 0);
        p.fillRect(textRect, QColor(0, 0, 0, 128));
        p.setPen(Qt::white);
        p.drawText(textRect, infoText);
    }
}


void Viewer::wheelEvent(QWheelEvent *event)
{
    const qreal numDegrees = event->angleDelta().y();
    if (!numDegrees) {
        return;
    }
    const qreal delta = 1. + qAbs(numDegrees / 800.);
    if (numDegrees > 0) {
        updateSize(m_scaledSize * delta);
    } else {
        updateSize(m_scaledSize / delta);
    }
}

void Viewer::keyPressEvent(QKeyEvent *event)
{
    QSize fullSize = m_imageSize.scaled(screen()->availableSize(), Qt::KeepAspectRatio);
    const QSize screenSize = screen()->availableSize();
    switch(event->key()) {
    case Qt::Key_1: updateSize(fullSize  * 0.1); return;
    case Qt::Key_2: updateSize(fullSize  * 0.2); return;
    case Qt::Key_3: updateSize(fullSize  * 0.3); return;
    case Qt::Key_4: updateSize(fullSize  * 0.4); return;
    case Qt::Key_5: updateSize(fullSize  * 0.5); return;
    case Qt::Key_6: updateSize(fullSize  * 0.6); return;
    case Qt::Key_7: updateSize(fullSize  * 0.7); return;
    case Qt::Key_8: updateSize(fullSize  * 0.8); return;
    case Qt::Key_9: updateSize(fullSize  * 0.9); return;
    case Qt::Key_0: updateSize(m_imageSize); return;
    case Qt::Key_Backspace:
        if (m_movie) {
            m_movie->setSpeed(100);
        }
        updateSize(m_imageSize);
        return;
    case Qt::Key_Space:
        if (!m_movie) {
            return;
        }
        if (m_movie->state() == QMovie::Running) {
            m_movie->setPaused(true);
        } else {
            m_movie->setPaused(false);
        }
        return;
    case Qt::Key_W:
        if (!m_movie) {
            return;
        }
        m_movie->setSpeed(qMin<int>(m_movie->speed() * 1.1, 1000));
        return;
    case Qt::Key_S:
        if (!m_movie) {
            return;
        }
        m_movie->setSpeed(qMax<int>(m_movie->speed() / 1.1, 10));
        return;
    case Qt::Key_A:
        if (!m_movie) {
            return;
        }
        m_movie->setPaused(true);
        if (m_movie->currentFrameNumber() == 0) {
            m_movie->jumpToFrame(m_movie->frameCount() - 1);
        } else {
            m_movie->jumpToFrame(m_movie->currentFrameNumber() - 1);
        }
        return;
    case Qt::Key_N:
        if (m_effect == Normalize) {
            m_effect = None;
        } else {
            m_effect = Normalize;
        }
        updateScaled();
        update();
        break;
    case Qt::Key_E:
        if (m_effect == Equalize) {
            m_effect = None;
        } else {
            m_effect = Equalize;
        }
        updateScaled();
        update();
        break;

    case Qt::Key_D:
        if (!m_movie) {
            return;
        }
        m_movie->setPaused(true);
        if (m_movie->currentFrameNumber() >= m_movie->frameCount()) {
            m_movie->jumpToFrame(0);
        } else {
            m_movie->jumpToFrame(m_movie->currentFrameNumber() + 1);
        }
        return;
    case Qt::Key_Equal:
    case Qt::Key_Plus:
    case Qt::Key_Up:
        updateSize(m_scaledSize * 1.1);
        return;
    case Qt::Key_PageUp:
        updateSize(m_scaledSize * 2);
        return;
    case Qt::Key_Minus:
    case Qt::Key_Down:
        updateSize(m_scaledSize / 1.1);
        return;
    case Qt::Key_PageDown:
        updateSize(m_scaledSize / 2);
        return;
    case Qt::Key_F: {
        if (width() >= screenSize.width() || height() >= screenSize.height()) {
            updateSize(m_imageSize);
        } else {
            updateSize(fullSize);
        }
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
    case Qt::Key_I:
        m_showInfo = !m_showInfo;
        update();
        break;
    default:
        break;
    }
    QRasterWindow::keyPressEvent(event);
}

void Viewer::updateScaled()
{
    if (m_movie) {
        return;
    }
    QElapsedTimer t; t.start();
    const Qt::TransformationMode mode = Qt::SmoothTransformation;
    const bool scalingUp = m_image.width() < width();

    m_scaled = m_image;
    if (scalingUp) {
        if (m_effect == Equalize) {
            equalize(m_scaled);
        } else if (m_effect == Normalize) {
            normalize(m_scaled);
        }
    }
    m_scaled = m_scaled.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (!scalingUp) {
        if (m_effect == Equalize) {
            equalize(m_scaled);
        } else if (m_effect == Normalize) {
            normalize(m_scaled);
        }
    }
    qDebug() << "Effect applied in" << t.elapsed() << "ms";
}

void Viewer::resizeEvent(QResizeEvent *event)
{
    m_scaledSize = m_imageSize.scaled(size(), Qt::KeepAspectRatio);
    if (m_movie) {
        if (!m_brokenFormat) {
            m_movie->setScaledSize(m_scaledSize);
        }
    } else {
        updateScaled();
    }
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

void Viewer::mousePressEvent(QMouseEvent *ev)
{
    m_lastMousePos = ev->globalPos();
    QGuiApplication::setOverrideCursor(QCursor(Qt::ClosedHandCursor));
}

void Viewer::mouseReleaseEvent(QMouseEvent *)
{
    QGuiApplication::restoreOverrideCursor();
}

void Viewer::mouseMoveEvent(QMouseEvent *ev)
{
    if (!ev->buttons()) {
        return;
    }
    const QPoint delta = ev->globalPos() - m_lastMousePos;
    setPosition(position() + delta);
    m_lastMousePos = ev->globalPos();
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
