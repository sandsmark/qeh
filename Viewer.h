#pragma once

#include <QRasterWindow>
#include <QImage>
#include <QScopedPointer>
#include <QElapsedTimer>
#include <QMovie>
#include <QPointer>

//#define DEBUG_MNG

class QMovie;
class QIODevice;

class Viewer : public QRasterWindow
{
    Q_OBJECT

public:
    Viewer();

    bool load(const QString &filename);

    bool isValid() {
        return (m_movie && m_movie->isValid()) || !m_image.isNull();
    }
    QImageReader::ImageReaderError error() const { return m_error; }

    enum Effect {
        None,
        Normalize,
        Equalize
    };

private slots:
    void setAspectRatio();
    void resetMovie();
    void onMovieFinished();

protected:
    void paintEvent(QPaintEvent*) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void mousePressEvent(QMouseEvent *evt) override;
    void mouseReleaseEvent(QMouseEvent *evt) override;
    void mouseMoveEvent(QMouseEvent *evt) override;
    void wheelEvent(QWheelEvent *event) override;
    bool event(QEvent *event) override;

private:
    void updateSize(QSize newSize, bool initial = false);
    void ensureVisible();
    void updateScaled();

    QImage m_image;
    QImage m_scaled;
    QSize m_imageSize;
    QSize m_scaledSize;
    QImageReader::ImageReaderError m_error = QImageReader::UnknownError;

    QScopedPointer<QMovie> m_movie;
    bool m_decodeSuccess = false;
    bool m_needReset = false;

    QPoint m_lastMousePos;
    bool m_brokenFormat = false;

    QString m_fileName;
    QByteArray m_buffer;

    bool m_showInfo = false;
    QString m_format;

    Effect m_effect = None;

    bool m_showHelp = false;

#ifdef DEBUG_MNG
    QElapsedTimer m_timer;
#endif
};

