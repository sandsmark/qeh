#pragma once

#include <QRasterWindow>
#include <QImage>
#include <QScopedPointer>
#include <QElapsedTimer>
#include <QMovie>
#include <QPointer>

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
    bool event(QEvent *event) override;

private:
    void updateSize(QSize newSize, bool initial = false);
    void ensureVisible();
    QImage m_image;
    QImage m_scaled;
    QSize m_imageSize;
    QSize m_scaledSize;
    QScopedPointer<QMovie> m_movie;
    QImageReader::ImageReaderError m_error = QImageReader::UnknownError;

    bool m_failed = false;

    QPoint m_lastMousePos;
    bool m_brokenFormat = false;
    QElapsedTimer m_timer;

    QPointer<QIODevice> m_input;

    bool m_showInfo = false;
    QString m_format;
};

