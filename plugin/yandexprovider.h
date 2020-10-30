/*
    Copyright (C) 2016 Jolla Ltd.
    Contact: Chris Adams <chris.adams@jollamobile.com>
    Copyright (C) 2020 Chupligin Sergey <neochapay@gmail.com>

    This file is part of geoclue-yandex based on geoclue-mlsdb.

    Geoclue-yandex is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License.
*/

#ifndef MLSDBPROVIDER_H
#define MLSDBPROVIDER_H

#include <QtCore/QFileSystemWatcher>
#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtCore/QBasicTimer>
#include <QtCore/QList>
#include <QtCore/QSet>
#include <QtCore/QMap>
#include <QtCore/QDateTime>
#include <QtCore/QVariantMap>
#include <QtDBus/QDBusContext>

#include "locationtypes.h"
#include "mlsdbserialisation.h"

/*
// TODO: use RIL to perform RIL_REQUEST_GET_NEIGHBORING_CELL_IDS
// add the following to spec:
BuildRequires: pkgconfig(libhardware)
BuildRequires: pkgconfig(android-headers)
// and uncomment these lines, and add the callbacks etc.
#include <android-version.h>
#include <telephony/ril.h>
// Define versions of the Android RIL interface supported.
#if ANDROID_VERSION_MAJOR == 5 && ANDROID_VERSION_MINOR >= 1
    #define GEOCLUE_ANDROID_RIL_INTERFACE 51
#elif ANDROID_VERSION_MAJOR == 4 && ANDROID_VERSION_MINOR >= 2
    #define GEOCLUE_ANDROID_RIL_INTERFACE 42
#else
    // By default expects Android 4.1
#endif
*/

QT_FORWARD_DECLARE_CLASS(QDBusServiceWatcher)
class QOfonoExtCellWatcher;
class YandexOnlineLocator;

/*
 * The geoclue-mlsdb provider provides position information
 * by determining approximate device position from cell id
 * and signal strength information available from the RIL.
 *
 * The geographic location of the given cell id is looked up
 * from a Mozilla Location Service database.
 */

class YandexProvider : public QObject, public QDBusContext
{
    Q_OBJECT

public:
    struct CellPositioningData {
        MlsdbUniqueCellId uniqueCellId;
        quint32 signalStrength;
    };

    explicit YandexProvider(QObject *parent = 0);
    ~YandexProvider();

    // org.freedesktop.Geoclue
    void AddReference();
    void RemoveReference();

    // org.freedesktop.Geoclue
    QString GetProviderInfo(QString &description);

    // Must match GeoclueStatus enum
    enum Status {
        StatusError,
        StatusUnavailable,
        StatusAcquiring,
        StatusAvailable
    };

    // org.freedesktop.Geoclue
    int GetStatus();

    // org.freedesktop.Geoclue
    void SetOptions(const QVariantMap &options);

    // Must match GeocluePositionFields enum
    enum PositionField {
        NoPositionFields = 0x00,
        LatitudePresent = 0x01,
        LongitudePresent = 0x02,
        AltitudePresent = 0x04
    };
    Q_DECLARE_FLAGS(PositionFields, PositionField)

    // org.freedesktop.Geoclue.Position
    int GetPosition(int &timestamp, double &latitude, double &longitude, double &altitude, Accuracy &accuracy);

signals:
    // org.freedesktop.Geoclue
    void StatusChanged(int status);

    // org.freedesktop.Geoclue.Position
    void PositionChanged(int fields, int timestamp, double latitude, double longitude, double altitude, const Accuracy &accuracy);

private Q_SLOTS:
    void setLocation(const Location &location);
    void serviceUnregistered(const QString &service);
    void updatePositioningEnabled();
    void cellularNetworkRegistrationChanged();
    void onlineLocationFound(double latitude, double longitude, double accuracy);
    void onlineLocationError(const QString &errorString);
    void onlineWlanChanged();

protected:
    void timerEvent(QTimerEvent *event) Q_DECL_OVERRIDE; // QObject

private:
    void emitLocationChanged();
    void startPositioningIfNeeded();
    void stopPositioningIfNeeded();
    void setStatus(Status status);
    void getEnabled(bool *positioningEnabled, bool *cellPositioningEnabled,
                    bool *onlinePositioningEnabled, bool *onlineDataAllowed,
                    bool *cellDataAllowed, bool *wlanDataAllowed);
    quint32 minimumRequestedUpdateInterval() const;
    void calculatePositionAndEmitLocation();

    QList<CellPositioningData> seenCellIds() const;
    void updateLocationFromCells(const QList<CellPositioningData> &cells);
    bool searchForCellIdLocation(const MlsdbUniqueCellId &uniqueCellId, MlsdbCoords *coords);

    QFileSystemWatcher m_locationSettingsWatcher;
    bool m_positioningEnabled;
    bool m_cellDataAllowed;
    bool m_positioningStarted;
    Status m_status;
    Location m_currentLocation;
    Location m_lastLocation;

    YandexOnlineLocator *m_mlsdbOnlineLocator;
    bool m_onlinePositioningEnabled;
    bool m_onlineDataAllowed;
    bool m_wlanDataAllowed;
    QPair<QDateTime, QVariantMap> m_previousQuery;

    QOfonoExtCellWatcher *m_cellWatcher;
    QMap<MlsdbUniqueCellId, MlsdbCoords> m_uniqueCellIdToLocation; // cache
    QSet<MlsdbUniqueCellId> m_knownCellIdsWithUnknownLocations;

    QDBusServiceWatcher *m_watcher;
    struct ServiceData {
        ServiceData()
        :   referenceCount(0), updateInterval(0)
        {
        }

        int referenceCount;
        quint32 updateInterval;
    };
    QMap<QString, ServiceData> m_watchedServices;

    QBasicTimer m_idleTimer;    // qApp->quit() if positioning is off for long enough.
    QBasicTimer m_fixLostTimer; // after fix timeout, status set to Acquiring.  timer is reset when a position is calculated.
    QBasicTimer m_recalculatePositionTimer;

    bool m_signalUpdateCell;
    bool m_signalUpdateWlan;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(YandexProvider::PositionFields)

#endif // MLSDBPROVIDER_H
