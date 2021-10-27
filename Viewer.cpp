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
#include <QTimer>
#include <QString>

#ifdef DEBUG_LOAD_TIME
#include <QElapsedTimer>
#endif//DEBUG_LOAD_TIME

extern "C" {
#include <xcb/xcb_icccm.h>
#include <unistd.h>
}

static const QString s_helpText = QStringLiteral(
        "Equals/Plus/Up: Zooms in\n"
        "Minus/Down: Zooms out\n"
        "Right: Move right\n"
        "Left: Move left\n"
        "Escape/Q: Quit\n"
        "F: Maximize\n"
        "I: Show/hide image info\n"
        "1-0: Zoom 10-100%\n"
        "Space: Toggle animation\n"
        "W: Speed up animation\n"
        "S: Slow down animation\n"
        "D: Step animation forward\n"
        "A: Step animation backward\n"
        "E: Equalize\n"
        "N: Normalize\n"
        "Backspace: Reset\n"
        "?: Show/hide this message"
    );


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
    QIODevice *device = nullptr;
    if (filename == "-") {
        QFile stdinFile;
        stdinFile.open(STDIN_FILENO, QIODevice::ReadOnly, QFileDevice::DontCloseHandle);

        m_buffer = stdinFile.readAll();
        device = new QBuffer(&m_buffer, this);
    } else {
        m_fileName = filename;
        device = new QFile(filename, this);
    }

    if (!device->open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open" << filename << device->errorString();
        return false;
    }

    QImageReader reader(device);

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
        reader.setDevice(nullptr);
        device->deleteLater();

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
    } else {
        reader.read(&m_image);
        reader.setDevice(nullptr);
        device->deleteLater();
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
    m_decodeSuccess = false;
    m_needReset = false;

    int speed = -1;
    QSize scaledSize;

    if (m_movie) {
        speed = m_movie->speed();
        scaledSize = m_movie->scaledSize();
        m_movie->device()->deleteLater();
    }

    QIODevice *device = nullptr;
    if (!m_fileName.isEmpty()) {
        device = new QFile(m_fileName, this);
    } else {
        device = new QBuffer(&m_buffer, this);
    }
    device->open(QIODevice::ReadOnly);
    m_movie.reset(new QMovie(device));

    if (speed > 0 && speed <= 1000) {
        m_movie->setSpeed(speed);
    }
    if (scaledSize.isValid()) {
        m_movie->setScaledSize(scaledSize);
    } else if (m_imageSize.isValid()) {
        m_movie->setScaledSize(m_imageSize);
    }

    // Completely arbitrary
    const bool bigImage = m_scaledSize.width()/1000 * m_scaledSize.height() / 1000 > 16;
    m_movie->setCacheMode(bigImage ? QMovie::CacheNone : QMovie::CacheAll);

    QMetaObject::invokeMethod(m_movie.get(), &QMovie::start);

    connect(m_movie.get(), &QMovie::error, this, [this]() {
            qWarning() << "QMovie error" << m_movie->lastErrorString();
            m_needReset = true;
        });

#ifdef DEBUG_MNG
    m_timer.restart();
    qDebug() << " - Next delay" << m_movie->nextFrameDelay();
#endif

    connect(m_movie.get(), &QMovie::frameChanged, this, [this](int frameNum) {
#ifdef DEBUG_MNG
        qDebug() << " - Now at frame" << frameNum << m_timer.restart() << "elapsed since last";
        qDebug() << " - Next delay" << m_movie->nextFrameDelay();
#endif
        update();

        // Basically just the mng reader not providing a delay, we cap at 60fps
        const int nextFrameDelay = m_movie->nextFrameDelay();
        if (nextFrameDelay < 16) {
            m_movie->setPaused(true);
            QTimer::singleShot(qMin(16 - nextFrameDelay, 16), m_movie.data(), &QMovie::start);
        }

        if (!m_movie->currentPixmap().isNull()) {
            m_decodeSuccess = true;
        }

        if (!m_brokenFormat) {
            return;
        }
        if (m_movie->frameCount() > 0 && frameNum >= m_movie->frameCount() - 1) {
            QMetaObject::invokeMethod(this, &Viewer::onMovieFinished, Qt::QueuedConnection);
            return;
        }
    });

    connect(m_movie.get(), &QMovie::finished, this, &Viewer::onMovieFinished, Qt::QueuedConnection);
}

void Viewer::onMovieFinished()
{
    if (!m_decodeSuccess) {
        qWarning() << "Animation decode failed";
        return;
    }
    if (m_needReset) {
        QMetaObject::invokeMethod(this, &Viewer::resetMovie);
    } else {
        m_movie->jumpToFrame(0);
    }
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
    if (image.isNull()) {
        qWarning() << "Decode failure";
        return;
    }
    p.drawImage(imageRect.topLeft(), image);

    // This _should_ always be empty
    QRegion background = rect;
    background -= imageRect;
    background &= event->region();
    for (const QRect &r : background) {
        p.fillRect(r, Qt::black);
    }

    QString text;
    if (m_showHelp) {
        text = s_helpText;
    } else if (m_showInfo) {
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        text += enumToString(image.format());
        text += "\nSize: " + QString::asprintf("%dx%d", m_imageSize.width(), m_imageSize.height());
        text += "\nFormat: " + m_format;

        QColorSpace colors = image.colorSpace();
        if (colors.isValid() || image.colorCount()) {
            text += "\nColors:";
            if (colors.gamma()) {
                text += "\n  Gamma: " + QString::number(colors.gamma());
            }
            if (colors.isValid()) {
                text += "\n  Primaries: " + enumToString(colors.primaries());
                text += "\n  Transfer function: " + enumToString(colors.transferFunction());
            }
            if (image.colorCount()) {
                text += "\n  Color count: " + QString::number(image.colorCount());
            }
        }

        if (!image.textKeys().isEmpty()) {
            text += "\nMetadata:";
            for (const QString &key : image.textKeys()) {
                QString text = image.text(key).simplified();
                //text.remove("\n");
                if (text.length() > 25) {
                    text = text.mid(0, 22) + "...";
                }
                text += "\n - " + key + ": " + text;
            }
        }

        if (m_movie) {
            text += "\nFrame: " + QString::number(m_movie->currentFrameNumber());
            if (m_movie->frameCount()) {
                text += "/" + QString::number(m_movie->frameCount());
            }
            if (m_movie->speed() != 100) {
                text += "\nSpeed: " + QString::number(m_movie->speed()) + "%";
            }
            text += "\n" + enumToString(m_movie->state());
        }
    }
    if (!text.isEmpty()) {
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        QRect textRect = p.boundingRect(rect, Qt::AlignLeft | Qt::AlignTop, text);
        textRect += QMargins(2, 2, 2, 2);
        textRect.moveTo(0, 0);
        p.fillRect(textRect, QColor(0, 0, 0, 128));
        p.setPen(Qt::white);
        p.drawText(textRect, text);
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
    const QSize screenSize = screen()->availableSize();
    QSize fullSize = m_imageSize.scaled(screenSize, Qt::KeepAspectRatio);
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
    case Qt::Key_Backspace: {
        if (m_movie) {
            m_movie->setSpeed(100);
        }
        updateSize(m_imageSize);
        const QRect screenGeo = screen()->availableGeometry();
        QRect geo = geometry();
        geo.moveCenter(screenGeo.center());
        setGeometry(geo);
        return;
    }
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
        if (m_movie->frameCount() && m_movie->currentFrameNumber() >= m_movie->frameCount()) {
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
    case Qt::Key_Question:
        m_showHelp = !m_showHelp;
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
#ifdef DEBUG_LOAD_TIME
    QElapsedTimer t; t.start();
#endif
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
#ifdef DEBUG_LOAD_TIME
    qDebug() << "Effect applied in" << t.elapsed() << "ms";
#endif
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
