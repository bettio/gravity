#include <QtCore/QCommandLineParser>
#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QStringList>

#include <HemeraCore/Operation>

#include "gravityusermanager.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    app.setApplicationName(QStringLiteral("Gravity User Manager"));
    app.setOrganizationDomain(QStringLiteral("com.ispirata.Hemera"));
    app.setOrganizationName(QStringLiteral("Ispirata"));
    app.setApplicationVersion(QStringLiteral(GRAVITY_VERSION));

    // Usage: gravity-user-manager <files ...>
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Gravity User Manager"));
    parser.addHelpOption();
    parser.addVersionOption();

    parser.addPositionalArgument(QStringLiteral("userlists"), QStringLiteral("Userlist files to compile."));

    QCommandLineOption createOption(QStringList() << QStringLiteral("create"),
                                    QStringLiteral("Create or update users"));
    parser.addOption(createOption);
    QCommandLineOption deleteOption(QStringList() << QStringLiteral("delete"),
                                    QStringLiteral("Delete users"));
    parser.addOption(deleteOption);
    QCommandLineOption debugSupportOption(QStringList() << QStringLiteral("debug-support"),
                                          QStringLiteral("Compiles support for debug targets and orbits. Makes sense only if compiling an Orbital Application."));
    parser.addOption(debugSupportOption);

    parser.process(app);

    // Check
    if (parser.positionalArguments().isEmpty()) {
        return EXIT_FAILURE;
    }
    if (!parser.isSet(createOption) && !parser.isSet(deleteOption)) {
        qDebug() << "Either --create or --delete must be given.";
        return EXIT_FAILURE;
    }
    if (parser.isSet(createOption) && parser.isSet(deleteOption)) {
        qDebug() << "Must give only one between --create or --delete.";
        return EXIT_FAILURE;
    }

    GravityUserManager gum(parser.positionalArguments(), parser.isSet(createOption), parser.isSet(debugSupportOption));

    Hemera::Operation *op = gum.init();

    QObject::connect(op, &Hemera::Operation::finished, [op] {
        if (op->isError()) {
            qWarning() << "Gravity User manager could not be initialized. Error:" << op->errorName() << op->errorMessage();
            QCoreApplication::instance()->exit(EXIT_FAILURE);
        }
    });

    return app.exec();
}
