#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>

#include <HemeraCore/Operation>
#include <HemeraCore/CommonOperations>

#include "appliancecryptoservice.h"
#include "fingerprintsservice.h"

#include <systemd/sd-daemon.h>
#include <systemd/sd-journal.h>

#include <functional>

/* 60 seconds */
constexpr int killerInterval() { return 60 * 1000; }

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    FingerprintsService *fingerprintsService = new FingerprintsService;
    ApplianceCryptoService *applianceCryptoService = new ApplianceCryptoService;
    Hemera::CompositeOperation *sop = new Hemera::CompositeOperation(QList<Hemera::Operation*>() << fingerprintsService->init()
                                                                                                 << applianceCryptoService->init());
    QObject::connect(sop, &Hemera::Operation::finished, [fingerprintsService, applianceCryptoService] (Hemera::Operation *op) {
        if (op->isError()) {
            sd_notifyf(0, "STATUS=%s.\nERRNO=15", op->errorMessage().toLatin1().constData());
            QCoreApplication::instance()->exit(15);
            return;
        }

        // Activity monitoring
        QTimer *killerTimer = new QTimer;
        QObject::connect(killerTimer, &QTimer::timeout, [killerTimer] {
            sd_notify(0, "STATUS=hemera-fingerprints is shutting down due to inactivity.\n");
            killerTimer->deleteLater();
            QCoreApplication::instance()->quit();
        });
        killerTimer->setInterval(killerInterval());
        killerTimer->setSingleShot(true);

        auto timerStart = (void (QTimer::*)())&QTimer::start;
        QObject::connect(fingerprintsService, &FingerprintsService::activity, killerTimer, timerStart);
        QObject::connect(applianceCryptoService, &ApplianceCryptoService::activity, killerTimer, timerStart);

        killerTimer->start();

        sd_notify(0, "STATUS=hemera-fingerprints is active.\n");
    });

    return app.exec();
}
