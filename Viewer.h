#pragma once

#include <QWindow>
#include <QImage>

class Viewer : public QWindow
{
    Q_OBJECT

public:
    Viewer(const QString &file);
    bool isValid() const { return !m_image.isNull(); }

public:
    bool event(QEvent *event) override;

private slots:
    void setAspectRatio();
    void render(const QRegion &region);

private:
    void updateSize(QSize newSize, bool centerOnScreen = false);
    const QImage m_image;
    QImage m_scaled;
    QBackingStore *m_backingStore;
};


