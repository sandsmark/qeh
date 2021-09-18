#pragma once

#include <QRasterWindow>
#include <QImage>
#include <QScopedPointer>
#include <QMovie>

class QMovie;

class Viewer : public QRasterWindow
{
    Q_OBJECT

public:
    Viewer(const QString &file);

    bool isValid() {
        return (m_movie && m_movie->isValid()) || !m_image.isNull();
    }

private slots:
    void setAspectRatio();

protected:
    void paintEvent(QPaintEvent*) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    bool event(QEvent *event) override;

private:
    void updateSize(QSize newSize, bool initial = false);
    void ensureVisible();
    QImage m_image;
    QImage m_scaled;
    QSize m_imageSize;
    QScopedPointer<QMovie> m_movie;
};


