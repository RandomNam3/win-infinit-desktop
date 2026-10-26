/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <QKeySequence>
#include <QList>
#include <QString>

namespace InfiniteDesktop
{

/**
 * Key combinations in the format of the service's settings file ("Win+Alt+H",
 * "Win+Alt+Num+", "Win+Ctrl+Alt+Left"; several separated by ';'), see shortcuts.cpp of the
 * service. Qt's Meta modifier is the Windows key.
 */
namespace KeyCombo
{

/// The service's text for the first chord of `sequence`; empty if the service cannot bind it.
QString toText(const QKeySequence &sequence);
/// Empty for anything the service would not understand either.
QKeySequence fromText(const QString &text);

QString toList(const QList<QKeySequence> &sequences);
QList<QKeySequence> fromList(const QString &text);

/// True if the service can bind `sequence` (one chord, a supported key).
bool isSupported(const QKeySequence &sequence);

} // namespace KeyCombo

} // namespace InfiniteDesktop
