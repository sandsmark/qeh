#pragma once

#include <QWidget>
#include <QImage>
#include <QTimer>

class Viewer : public QWidget
{
    Q_OBJECT

public:
    Viewer(const QString &file);
    bool isValid() const { return !m_image.isNull(); }

public:
    void keyPressEvent(QKeyEvent *event) override;
    void paintEvent(QPaintEvent *e) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void updateSize(QSize newSize, bool centerOnScreen = false);
    const QImage m_image;
    QImage m_scaled;
    bool m_initialized = false;
};


