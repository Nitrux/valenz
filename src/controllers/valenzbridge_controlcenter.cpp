// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2026 Nitrux Latinoamericana S.C. <hello@nxos.org>

#include "valenzbridge.h"
#include "valenzbridge_p.h"
#include "mauikit_system_control.h"

QVariantList ValenzBridge::controlCenterDiskUsageOptions() const
{
    return MauiKitSystem::diskUsagePartitionOptions(m_controlCenterDiskUsagePath);
}
