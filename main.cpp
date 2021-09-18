#include "Viewer.h"

#include <QGuiApplication>
#include <QDebug>
#include <QImageReader>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QAccessible>

void dummyAccessibilityRootHandler(QObject*) {  }

static void printHelp(const char *app)
{
    qDebug() << "Usage:" << app << "(filename)";
    qDebug().noquote() << "Supported formats:";
    QMimeDatabase db;
    for (const QByteArray &format : QImageReader::supportedMimeTypes()) {
        QMimeType mime = db.mimeTypeForName(format);
        if (!mime.isValid() || mime.comment().isEmpty()) {
            qDebug().noquote() << " - " << QImageReader::imageFormatsForMimeType(format).join(", ");
            continue;
        }
        qDebug().noquote() << " - " << mime.filterString();
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

    QGuiApplication a(argc, argv);
    if (argc != 2) {
        printHelp(argv[0]);
        return 1;
    }
    for (const QString &arg : a.arguments()) {
        if (arg == "-h" || arg == "-v" || arg == "--help" || arg == "--version") {
            printHelp(argv[0]);
            return 0;
        }
    }
    a.setApplicationDisplayName(QFileInfo(argv[1]).fileName());

    Viewer w(argv[1]);
    if (!w.isValid()) {
        printHelp(argv[0]);
        return 1;
    }
    w.show();
    return a.exec();
}
