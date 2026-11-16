#pragma once

// appupdate::SerialStore — where an app keeps lastSerial (contract V1,
// 2026-11-16, docs/SHARED.md „Update-Härtung“).
//
// The hub stays QSettings-free ("Hub liefert Mechanik, Produkt Bedeutung"):
// UpdateChecker decides, the app persists. `load` answers the highest serial
// accepted so far for a product slug (0 = none, also for an unreadable
// value); `store` receives a HIGHER one after a manifest passed signature,
// product and replay checks — also when the user then installs nothing.
// Both are called on the checker's thread, inside checkForUpdate's flow.
//
// Key spelling for QSettings-based apps: policy::lastSerialSettingsKey()
// (UpdatePolicy.hpp) — "update/last_serial/<slug>", value the number.
//
// Its own header so DeviceUpdateService and other hosts can take one
// without pulling QtNetwork in through UpdateChecker.hpp.

#include <QString>
#include <QtGlobal>

#include <functional>

namespace appupdate {

struct SerialStore {
    std::function<quint32(const QString& productSlug)> load;
    std::function<void(const QString& productSlug, quint32 serial)> store;

    [[nodiscard]] bool isSet() const
    {
        return static_cast<bool>(load) && static_cast<bool>(store);
    }
};

}  // namespace appupdate
