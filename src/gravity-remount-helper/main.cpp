#include <QtCore/QCommandLineParser>
#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QLoggingCategory>
#include <QtCore/QTimer>

#include <HemeraCore/Operation>

#include "gravityremounthelpercore.h"

#include <systemd/sd-daemon.h>

// Static plugins
//#include "plugins/config-static-plugins.h"

Q_LOGGING_CATEGORY(LOG_GRH_MAIN, "GravityRemountHelper::main")

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    app.setApplicationName(QStringLiteral("Gravity Remount Helper"));
    app.setApplicationVersion(QStringLiteral(HEMERA_GRAVITY_VERSION));
    app.setOrganizationDomain(QStringLiteral("com.ispirata.hemera"));
    app.setOrganizationName(QStringLiteral("Ispirata"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Gravity Remount Helper"));
    parser.addHelpOption();
    parser.addVersionOption();

    // A boolean option with multiple names (-f, --force)
    QCommandLineOption modeOption(QStringList() << QStringLiteral("m") << QStringLiteral("mode"),
                                  QStringLiteral("Operation mode. Can be either rw or ro. Required"), QStringLiteral("mode"));
    parser.addOption(modeOption);
    QCommandLineOption hooksOnlyOption(QStringList() << QStringLiteral("hooks-only"),
                                       QStringLiteral("Executes only relevant hooks without remounting"));
    parser.addOption(hooksOnlyOption);

    // Process the actual command line arguments given by the user
    parser.process(app);

    QString mode = parser.value(modeOption);
    if (mode != QStringLiteral("rw") && mode != QStringLiteral("ro")) {
        qCCritical(LOG_GRH_MAIN) << "--mode should be specified, and be either ro or rw.";
        return 1;
    }
    bool hooksOnly = parser.isSet(hooksOnlyOption);
    if (hooksOnly) {
        qCInfo(LOG_GRH_MAIN) << "Selected hooks only, will skip remounting phase.";
    }

    GravityRemountHelperCore core(mode, hooksOnly);
    Hemera::Operation *op = core.init();

    QObject::connect(op, &Hemera::Operation::finished, [op] {
        if (op->isError()) {
            // Notify failure to systemd
            sd_notifyf(0, "STATUS=Failed to start up: %s\n"
                        "BUSERROR=%s",
                        qstrdup(op->errorMessage().toLatin1().constData()),
                        qstrdup(op->errorName().toLatin1().constData()));

            qFatal("Initialization of the core failed. Error reported: %s - %s", op->errorName().toLatin1().data(),
                                                                                 op->errorMessage().toLatin1().data());
        } else {
            // Load static plugins here


            // Notify startup to systemd
            sd_notify(0, "READY=1");
        }
    });

    return app.exec();
}
