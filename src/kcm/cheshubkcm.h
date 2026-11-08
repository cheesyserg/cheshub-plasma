// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "../devices/devicemanager.h"

#include <KQuickConfigModule>

namespace cheshub {

// System Settings → Input & Output → Input Devices → ChesHub Devices.
// Apply writes every changed device, Reset re-reads them from the hardware,
// Defaults loads factory values into the page (written on Apply).
class CheshubKcm : public KQuickConfigModule
{
    Q_OBJECT
    Q_PROPERTY(cheshub::DeviceManager *manager READ manager CONSTANT)

public:
    CheshubKcm(QObject *parent, const KPluginMetaData &data);

    DeviceManager *manager() const { return m_manager; }

    void load() override;
    void save() override;
    void defaults() override;

private:
    DeviceManager *m_manager;
    bool m_scanned = false;
};

} // namespace cheshub
