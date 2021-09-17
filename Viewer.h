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

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void paintEvent(QPaintEvent *e) override;
//    void resizeEvent(QResizeEvent *event) override;

private slots:
    void updateGeometry();

private:
    void setSize(QSize newSize, bool centerOnScreen = false);
    const QImage m_image;
    QImage m_scaled;
    bool m_updating = false;
    QTimer m_geometryTimer;
    QRect m_geo;
};
