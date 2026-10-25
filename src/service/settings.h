/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include "navigation.h"

#include <QColor>
#include <QString>
#include <QStringList>

#include <chrono>

class QSettings;

namespace Wid
{

/// Modifier combinations for gestures, as in the KDE schema (Meta is the Windows key).
enum class GestureModifier {
    Disabled,
    Meta,
    MetaAlt,
    MetaCtrl,
    MetaShift,
    CtrlAlt,
};

/// MOD_* bits of a gesture modifier, 0 for Disabled.
unsigned modifierBits(GestureModifier modifier);

enum class ShowMode {
    Never,
    WhileNavigating,
    Always,
};

/// Paper color of new notes (NoteColor in the KDE schema); the notes app uses it.
enum class NoteColor {
    Yellow,
    Blue,
    Green,
    Pink,
    Gray,
};

/**
 * The settings of the canvas service, read from
 * %APPDATA%\WIN Infinit Desktop\settings.ini, group [Effect-infinitdesktop].
 *
 * Keys, types and defaults are those of KDE Infinit Desktop's infinitdesktop.kcfg, so both
 * settings apps share one schema; values are clamped like the KWin effect does. Settings
 * that have no meaning on Windows are not read.
 */
struct Settings
{
    bool dragDesktopToPan = true;
    bool middleButtonPan = true;
    GestureModifier panModifier = GestureModifier::MetaAlt;
    bool wheelZoomOnDesktop = true;
    GestureModifier zoomModifier = GestureModifier::Meta;
    bool invertWheelZoom = false;
    bool shiftWheelPans = true;
    bool touchpadScrollPans = true;
    bool pinchZoom = true;
    bool invertScrollPan = false;
    qreal panSpeed = 1.0;
    qreal zoomStep = 1.15; ///< factor per wheel notch
    qreal minZoom = 0.10;
    qreal maxZoom = 1.0;
    bool snapZoomToOne = true;
    bool momentum = true;
    qreal friction = 5;
    std::chrono::milliseconds animationDuration{280};
    qreal keyboardPanStep = 0.25;
    bool revealOnActivate = true;
    InfinitDesktop::RevealMode revealMode = InfinitDesktop::RevealMode::Minimal;
    bool newWindowsExitZoom = true;
    bool pinKeepAboveWindows = false;
    QStringList excludedClasses;
    bool rememberWindowPlacement = true;
    bool restoreOnCanvas = true;
    bool restoreWindowSize = true;
    QStringList placementExcludedClasses;
    bool flingToOtherScreen = true;
    qreal flingThreshold = 1600;
    bool edgePan = true;
    int edgePanMargin = 24;
    qreal edgePanSpeed = 1400;
    bool lockFullscreen = true;
    int gridSpacing = 64;
    qreal gridOpacity = 0.35;
    QColor gridColor = QColor(255, 255, 255);
    ShowMode offscreenMarkers = ShowMode::WhileNavigating;
    // Notes, groups, areas and the map
    bool notesEnabled = true;
    NoteColor noteColor = NoteColor::Yellow;
    bool groupLayoutAutomatic = false; ///< GroupLayoutMode: Manual / Automatic
    bool groupStartsApplications = true;
    bool groupZoomToFit = true;
    ShowMode showAreas = ShowMode::Always;
    int areaPadding = 48;
    int mapSize = 70; ///< percent of the work area
    bool crashGuard = true;
    bool rememberCanvas = true;
    bool debugLogging = false;

    /// Reads all values from `settings` (current group: none; the group is entered here).
    static Settings load(QSettings &settings);
    /// Reads from the settings file of the current user.
    static Settings loadDefaultFile();
};

inline constexpr const char *SettingsGroup = "Effect-infinitdesktop";

/// %APPDATA%\WIN Infinit Desktop\settings.ini
QString settingsFilePath();
/// %LOCALAPPDATA%\WIN Infinit Desktop (created if missing)
QString stateDirectory();

GestureModifier parseGestureModifier(const QString &value, GestureModifier fallback);
ShowMode parseShowMode(const QString &value, ShowMode fallback);
NoteColor parseNoteColor(const QString &value, NoteColor fallback);

} // namespace Wid
