/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "settings.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>

#include <algorithm>

namespace Wid
{

namespace
{

constexpr unsigned ModAlt = 0x0001;
constexpr unsigned ModControl = 0x0002;
constexpr unsigned ModShift = 0x0004;
constexpr unsigned ModWin = 0x0008;

int readInt(QSettings &settings, const char *key, int fallback, int low, int high)
{
    bool ok = false;
    const int value = settings.value(QLatin1String(key), fallback).toInt(&ok);
    return std::clamp(ok ? value : fallback, low, high);
}

bool readBool(QSettings &settings, const char *key, bool fallback)
{
    const QVariant value = settings.value(QLatin1String(key));
    if (!value.isValid()) {
        return fallback;
    }
    const QString text = value.toString().trimmed().toLower();
    if (text == QLatin1String("true") || text == QLatin1String("1")) {
        return true;
    }
    if (text == QLatin1String("false") || text == QLatin1String("0")) {
        return false;
    }
    return fallback;
}

QStringList readList(QSettings &settings, const char *key, const QStringList &fallback)
{
    const QVariant value = settings.value(QLatin1String(key));
    if (!value.isValid()) {
        return fallback;
    }
    // KConfig writes lists comma separated; QSettings may already have split them.
    QStringList list = value.typeId() == QMetaType::QStringList ? value.toStringList() : value.toString().split(QLatin1Char(','));
    for (QString &entry : list) {
        entry = entry.trimmed().toLower();
    }
    list.removeAll(QString());
    return list;
}

QColor readColor(QSettings &settings, const char *key, const QColor &fallback)
{
    const QVariant value = settings.value(QLatin1String(key));
    if (!value.isValid()) {
        return fallback;
    }
    // "r,g,b" as KConfig stores colors; QSettings may have split it into a list.
    const QStringList parts = value.typeId() == QMetaType::QStringList ? value.toStringList() : value.toString().split(QLatin1Char(','));
    if (parts.size() < 3) {
        const QColor named(value.toString());
        return named.isValid() ? named : fallback;
    }
    const auto component = [](const QString &text) {
        return std::clamp(text.trimmed().toInt(), 0, 255);
    };
    return QColor(component(parts[0]), component(parts[1]), component(parts[2]));
}

} // namespace

unsigned modifierBits(GestureModifier modifier)
{
    switch (modifier) {
    case GestureModifier::Disabled:
        return 0;
    case GestureModifier::Meta:
        return ModWin;
    case GestureModifier::MetaAlt:
        return ModWin | ModAlt;
    case GestureModifier::MetaCtrl:
        return ModWin | ModControl;
    case GestureModifier::MetaShift:
        return ModWin | ModShift;
    case GestureModifier::CtrlAlt:
        return ModControl | ModAlt;
    }
    return 0;
}

GestureModifier parseGestureModifier(const QString &value, GestureModifier fallback)
{
    const QString text = value.trimmed().toLower();
    if (text == QLatin1String("disabled")) {
        return GestureModifier::Disabled;
    }
    if (text == QLatin1String("meta")) {
        return GestureModifier::Meta;
    }
    if (text == QLatin1String("metaalt")) {
        return GestureModifier::MetaAlt;
    }
    if (text == QLatin1String("metactrl")) {
        return GestureModifier::MetaCtrl;
    }
    if (text == QLatin1String("metashift")) {
        return GestureModifier::MetaShift;
    }
    if (text == QLatin1String("ctrlalt")) {
        return GestureModifier::CtrlAlt;
    }
    return fallback;
}

ShowMode parseShowMode(const QString &value, ShowMode fallback)
{
    const QString text = value.trimmed().toLower();
    if (text == QLatin1String("never")) {
        return ShowMode::Never;
    }
    if (text == QLatin1String("whilenavigating")) {
        return ShowMode::WhileNavigating;
    }
    if (text == QLatin1String("always")) {
        return ShowMode::Always;
    }
    return fallback;
}

NoteColor parseNoteColor(const QString &value, NoteColor fallback)
{
    const QString text = value.trimmed().toLower();
    if (text == QLatin1String("yellow")) {
        return NoteColor::Yellow;
    }
    if (text == QLatin1String("blue")) {
        return NoteColor::Blue;
    }
    if (text == QLatin1String("green")) {
        return NoteColor::Green;
    }
    if (text == QLatin1String("pink")) {
        return NoteColor::Pink;
    }
    if (text == QLatin1String("gray")) {
        return NoteColor::Gray;
    }
    return fallback;
}

Settings Settings::load(QSettings &settings)
{
    Settings s;
    settings.beginGroup(QLatin1String(SettingsGroup));
    s.dragDesktopToPan = readBool(settings, "DragDesktopToPan", s.dragDesktopToPan);
    s.middleButtonPan = readBool(settings, "MiddleButtonPan", s.middleButtonPan);
    s.panModifier = parseGestureModifier(settings.value(QStringLiteral("PanModifier")).toString(), s.panModifier);
    s.wheelZoomOnDesktop = readBool(settings, "WheelZoomOnDesktop", s.wheelZoomOnDesktop);
    s.zoomModifier = parseGestureModifier(settings.value(QStringLiteral("ZoomModifier")).toString(), s.zoomModifier);
    s.invertWheelZoom = readBool(settings, "InvertWheelZoom", s.invertWheelZoom);
    s.shiftWheelPans = readBool(settings, "ShiftWheelPans", s.shiftWheelPans);
    s.touchpadScrollPans = readBool(settings, "TouchpadScrollPans", s.touchpadScrollPans);
    s.pinchZoom = readBool(settings, "PinchZoom", s.pinchZoom);
    s.invertScrollPan = readBool(settings, "InvertScrollPan", s.invertScrollPan);
    s.panSpeed = readInt(settings, "PanSpeed", 100, 20, 400) / 100.0;
    s.zoomStep = 1.0 + readInt(settings, "ZoomStep", 15, 2, 100) / 100.0;
    s.minZoom = readInt(settings, "MinZoom", 10, 2, 100) / 100.0;
    s.maxZoom = readInt(settings, "MaxZoom", 100, 100, 400) / 100.0;
    s.snapZoomToOne = readBool(settings, "SnapZoomToOne", s.snapZoomToOne);
    s.momentum = readBool(settings, "Momentum", s.momentum);
    s.friction = readInt(settings, "Friction", 5, 1, 20);
    s.animationDuration = std::chrono::milliseconds(readInt(settings, "AnimationDuration", 280, 0, 2000));
    s.keyboardPanStep = readInt(settings, "KeyboardPanStep", 25, 5, 100) / 100.0;
    s.revealOnActivate = readBool(settings, "RevealOnActivate", s.revealOnActivate);
    s.revealMode = settings.value(QStringLiteral("RevealMode")).toString().trimmed().toLower() == QLatin1String("center")
        ? InfinitDesktop::RevealMode::Center
        : InfinitDesktop::RevealMode::Minimal;
    s.newWindowsExitZoom = readBool(settings, "NewWindowsExitZoom", s.newWindowsExitZoom);
    s.pinKeepAboveWindows = readBool(settings, "PinKeepAboveWindows", s.pinKeepAboveWindows);
    s.excludedClasses = readList(settings, "ExcludedWindowClasses", {});
    s.rememberWindowPlacement = readBool(settings, "RememberWindowPlacement", s.rememberWindowPlacement);
    s.restoreOnCanvas = settings.value(QStringLiteral("RestorePlacementMode")).toString().trimmed().toLower() != QLatin1String("screen");
    s.restoreWindowSize = readBool(settings, "RestoreWindowSize", s.restoreWindowSize);
    s.placementExcludedClasses = readList(settings, "PlacementExcludedClasses", {});
    s.flingToOtherScreen = readBool(settings, "FlingToOtherScreen", s.flingToOtherScreen);
    s.flingThreshold = readInt(settings, "FlingThreshold", 1600, 100, 10000);
    s.edgePan = readBool(settings, "EdgePan", s.edgePan);
    s.edgePanMargin = readInt(settings, "EdgePanMargin", 24, 2, 200);
    s.edgePanSpeed = readInt(settings, "EdgePanSpeed", 1400, 100, 8000);
    s.lockFullscreen = readBool(settings, "LockFullscreen", s.lockFullscreen);
    s.gridSpacing = readInt(settings, "GridSpacing", 64, 16, 512);
    s.gridOpacity = readInt(settings, "GridOpacity", 35, 0, 100) / 100.0;
    s.gridColor = readColor(settings, "GridColor", s.gridColor);
    s.offscreenMarkers = parseShowMode(settings.value(QStringLiteral("OffscreenMarkers")).toString(), s.offscreenMarkers);
    s.notesEnabled = readBool(settings, "NotesEnabled", s.notesEnabled);
    s.noteColor = parseNoteColor(settings.value(QStringLiteral("NoteColor")).toString(), s.noteColor);
    s.groupLayoutAutomatic = settings.value(QStringLiteral("GroupLayoutMode")).toString().trimmed().toLower() == QLatin1String("automatic");
    s.groupStartsApplications = readBool(settings, "GroupStartsApplications", s.groupStartsApplications);
    s.groupZoomToFit = readBool(settings, "GroupZoomToFit", s.groupZoomToFit);
    s.showAreas = parseShowMode(settings.value(QStringLiteral("ShowAreas")).toString(), s.showAreas);
    s.areaPadding = readInt(settings, "AreaPadding", 48, 0, 400);
    s.mapSize = readInt(settings, "MapSize", 70, 30, 95);
    s.crashGuard = readBool(settings, "CrashGuard", s.crashGuard);
    s.rememberCanvas = readBool(settings, "RememberCanvas", s.rememberCanvas);
    s.debugLogging = readBool(settings, "DebugLogging", s.debugLogging);
    settings.endGroup();
    return s;
}

Settings Settings::loadDefaultFile()
{
    QSettings file(settingsFilePath(), QSettings::IniFormat);
    return load(file);
}

QString settingsFilePath()
{
    // %APPDATA% (roaming), like the settings of other desktop programs.
    QString base = qEnvironmentVariable("APPDATA");
    if (base.isEmpty()) {
        base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    }
    return QDir(base).filePath(QStringLiteral("WIN Infinit Desktop/settings.ini"));
}

QString stateDirectory()
{
    QString base = qEnvironmentVariable("LOCALAPPDATA");
    if (base.isEmpty()) {
        base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    }
    const QString path = QDir(base).filePath(QStringLiteral("WIN Infinit Desktop"));
    QDir().mkpath(path);
    return path;
}

} // namespace Wid
