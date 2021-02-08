#include "genericfingerprintprovider.h"

#include "fingerprintsservice.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QDebug>
#include <QtCore/QFile>

#include <libudev.h>

GenericFingerprintProvider::GenericFingerprintProvider(QObject* parent): FingerprintProviderPlugin(parent)
{

}

GenericFingerprintProvider::~GenericFingerprintProvider()
{

}

QByteArray GenericFingerprintProvider::initHardwareSerialNumber()
{
    QStringList uidsList;
    QStringList removableUidsList;

    udev *ud = udev_new();
    udev_enumerate *uenum = udev_enumerate_new(ud);
    udev_enumerate_add_match_subsystem(uenum, "net");
    udev_enumerate_add_match_subsystem(uenum, "bluetooth");
    udev_enumerate_scan_devices(uenum);
    udev_list_entry *dev_list_entry;
    udev_list_entry *devices = udev_enumerate_get_list_entry(uenum);

    udev_list_entry_foreach(dev_list_entry, devices) {
        const char *path = udev_list_entry_get_name(dev_list_entry);
        udev_device *dev = udev_device_new_from_syspath(ud, path);
        //check if it is virtual: it has no ..\device path
        udev_device *parentDev = udev_device_get_parent(dev);
        if (!parentDev){
            continue;
        }

        const char *parentSubsystems = udev_device_get_subsystem(parentDev);

        if (!parentSubsystems) {
            qWarning() << "Found device without subsystem: " << udev_device_get_syspath(parentDev);
        }

        const char *addrSysAttr = udev_device_get_sysattr_value(dev, "address");
        if (addrSysAttr) {
            QString hwAddr = QString::fromLatin1(addrSysAttr);
            if (parentSubsystems && QString::fromLatin1(parentSubsystems).contains(QStringLiteral("usb"))) {
                removableUidsList.append(hwAddr);
            } else {
                uidsList.append(hwAddr);
            }
        }
    }

    //We need to free udev context

    QCryptographicHash hash(QCryptographicHash::Md5);
    uidsList.sort();
    if (uidsList.isEmpty()) {
        removableUidsList.sort();

        for (int i = 0; i < removableUidsList.size(); i++){
            qDebug() << removableUidsList.at(i);
            hash.addData(removableUidsList.at(i).toLatin1());
        }
    } else {
        for (int i = 0; i < uidsList.size(); i++){
            qDebug() << uidsList.at(i);
            hash.addData(uidsList.at(i).toLatin1());
        }
    }

    hash.addData(cpuValues().join(QStringLiteral(" ")).toLatin1());

    qDebug() << "Key: " << hash.result().toHex();

    return hash.result();
}

QStringList GenericFingerprintProvider::cpuValues() const
{
    QStringList list;

    QFile cpuinfoFile(QStringLiteral("/proc/cpuinfo"));
    if (!cpuinfoFile.open(QIODevice::ReadOnly)) {
        qWarning() << "Could not open cpuinfo!";
        return QStringList();
    }

    QTextStream cpuinfo(&cpuinfoFile);

    QStringList interstingKeys = { QStringLiteral("serial"), QStringLiteral("CPU implementer"), QStringLiteral("CPU architecture"),
                                   QStringLiteral("CPU variant"), QStringLiteral("CPU part"), QStringLiteral("CPU revision"), QStringLiteral("cpu family"),
                                   QStringLiteral("model") };

    QString line;
    do {
        line = cpuinfo.readLine();

        for (const QString &checkKey : interstingKeys) {
            if (line.startsWith(checkKey)) {
                QStringList sl = line.split(QLatin1Char(':'));
                if (sl.count() != 2) {
                    continue;
                }
                QString cpuValue = sl.at(1).simplified();
                qDebug() << "CPU info: " << checkKey << ": " << cpuValue;
                list.append(cpuValue);
            }
        }
    } while (!line.isNull());

    if (list.isEmpty()) {
        qWarning() << "no available CPU info";
    }

    list.sort();
    list.removeDuplicates();

    return list;
}
