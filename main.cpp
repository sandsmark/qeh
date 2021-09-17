#include "Viewer.h"

#include <QGuiApplication>
#include <QDebug>
#include <QImageReader>
#include <QFileInfo>

int main(int argc, char *argv[])
{
    QGuiApplication a(argc, argv);
    if (argc < 2) {
        qWarning() << "Pass an image";
        return 1;
    }
    for (const QString &arg : a.arguments()) {
        if (arg == "-h" || arg == "-v" || arg == "--help" || arg == "--version") {
            qDebug() << "Usage:" << argv[0] << "(filename)";
            qDebug() << "Supported formats:" << QImageReader::supportedImageFormats();
            return 0;
        }
    }
    QImageReader reader(QString::fromLocal8Bit(argv[1]));
    if (!reader.canRead()) {
        qWarning() << "Can't read image" << reader.errorString();
        return 1;
    }
    const QImage image = reader.read();
    if (image.isNull()) {
        qWarning() << "Invalid image read";
        if (reader.error() != QImageReader::UnknownError) {
            qWarning() << reader.errorString();
        }
        return 1;
    }
    a.setApplicationDisplayName(QFileInfo(reader.fileName()).fileName());

    Viewer w(image);
    w.show();
    return a.exec();
}
