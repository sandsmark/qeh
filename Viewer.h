#pragma once

#include <QRasterWindow>
#include <QImage>

class Viewer : public QRasterWindow
{
    Q_OBJECT

public:
    Viewer(const QImage &image);

private slots:
    void setAspectRatio();

protected:
    void paintEvent(QPaintEvent*) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void moveEvent(QMoveEvent *event) override;

private:
    void updateSize(QSize newSize, bool centerOnScreen = false);
    void ensureVisible();
    const QImage m_image;
    QImage m_scaled;
};


