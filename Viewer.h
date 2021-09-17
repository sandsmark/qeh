#pragma once

#include <QWidget>
#include <QImage>
#include <QTimer>

class Viewer : public QWidget
{
    Q_OBJECT

public:
    Viewer(const QString &file);
    const bool isValid() { return !m_image.isNull(); }
    ~Viewer();

public:
    void keyPressEvent(QKeyEvent *event) override;
    void paintEvent(QPaintEvent *e) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateSize(QSize newSize, bool centerOnScreen = false);
    const QImage m_image;
    bool m_resizing = false;
    QImage m_scaled;
};


