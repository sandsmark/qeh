#include "Viewer.h"

#include <QGuiApplication>
#include <QDebug>
#include <QImageReader>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QAccessible>
#include <QTimer>
#include <QSurfaceFormat>

//#define DEBUG_LAUNCH_TIME

#ifdef DEBUG_LAUNCH_TIME
#include <QTimer>
#include <QElapsedTimer>
#endif

void dummyAccessibilityRootHandler(QObject*) {  }

static void printHelp(const char *app, bool verbose)
{
    qDebug() << "Usage:" << app << "(filename)";
    qDebug() << "Filename can be - to read data from stdin instead, for example:";
    qDebug() << "   base64 -d foo | qeh -";
    if (!verbose) {
        return;
    }
    qDebug().noquote() << "Supported formats:";
    QMimeDatabase db;
    for (const QByteArray &format : QImageReader::supportedMimeTypes()) {
        QMimeType mime = db.mimeTypeForName(format);
        if (!mime.isValid() || mime.comment().isEmpty()) {
            qDebug().noquote() << " - " << QImageReader::imageFormatsForMimeType(format).join(", ");
            continue;
        }
        qDebug().noquote() << " - " << mime.filterString() << mime.name();
    }
}


int main(int argc, char *argv[])
{
    qunsetenv("QT_QPA_PLATFORMTHEME");
    qputenv("QT_XCB_GL_INTEGRATION", "none");
    qputenv("QT_QPA_PLATFORMTHEME", "generic");
    qputenv("QT_XCB_NO_XI2_MOUSE", "1");

    // Stole this from the qdbus unit tests, should hopefully kill all attempts
    // at using dbus and slowing is down.
    qputenv("DBUS_SESSION_BUS_ADDRESS", "unix:abstract=/tmp/does_not_exist");
    qputenv("DBUS_SYSTEM_BUS_ADDRESS", "unix:abstract=/tmp/does_not_exist");

    // Otherwise it tries to contact SPI and stuff
    // But we don't really have any GUI, so we don't need this
    QAccessible::installRootObjectHandler(dummyAccessibilityRootHandler);
#ifdef DEBUG_LAUNCH_TIME
    QElapsedTimer t; t.start();
#endif

    QGuiApplication a(argc, argv);
    if (argc != 2) {
        printHelp(argv[0], false);
        return 1;
    }

    for (const QString &arg : a.arguments()) {
        if (arg == "-h" || arg == "-v" || arg == "--help" || arg == "--version") {
            printHelp(argv[0], true);
            return 1;
        }
    }
    QSurfaceFormat defaultFormat = QSurfaceFormat::defaultFormat();
    if (!defaultFormat.hasAlpha()) {
        defaultFormat.setAlphaBufferSize(8);
    }
    QSurfaceFormat::setDefaultFormat(defaultFormat);


    QFileInfo info(argv[1]);

    a.setApplicationDisplayName(info.fileName());

    Viewer w;
    if (!w.load(argv[1])) {
        printHelp(argv[0], w.error() == QImageReader::UnsupportedFormatError);
        return 1;
    }
    w.show();
#ifdef DEBUG_LAUNCH_TIME
    QTimer::singleShot(0, &a, &QGuiApplication::quit);
    int ret = a.exec();
    qDebug() << "Startup done in" << t.elapsed() << "ms";
    return ret;
#else
    return a.exec();
#endif
}
