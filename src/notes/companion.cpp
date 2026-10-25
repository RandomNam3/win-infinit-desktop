/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "companion.h"
#include "notestore.h"

#include "apppaths.h"
#include "i18n.h"
#include "ipcclient.h"
#include "jsonlineserver.h"
#include "pipenames.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QSettings>
#include <QTimer>

#include <algorithm>

using namespace std::chrono_literals;

namespace InfinitDesktop
{

namespace
{

// Paper colors (light, readable with dark text), in the order of the NoteColor setting.
const QColor PaperColors[] = {
    QColor(0xff, 0xf1, 0x9c), // yellow
    QColor(0xcf, 0xe6, 0xff), // blue
    QColor(0xd4, 0xf2, 0xd0), // green
    QColor(0xff, 0xd8, 0xe6), // pink
    QColor(0xe8, 0xe8, 0xe6), // gray
};

// NoteColor is stored by name, like KConfig stores enums.
int colorIndex(const QString &name)
{
    static const char *const names[] = {"yellow", "blue", "green", "pink", "gray"};
    const QString lower = name.trimmed().toLower();
    for (int i = 0; i < 5; ++i) {
        if (lower == QLatin1String(names[i])) {
            return i;
        }
    }
    bool ok = false;
    const int number = lower.toInt(&ok);
    return ok ? std::clamp(number, 0, 4) : 0;
}

bool readBool(QSettings &settings, const QString &key, bool fallback)
{
    const QVariant value = settings.value(key);
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

} // namespace

Companion::Companion(QQmlEngine *engine, NoteStore *store, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
    , m_store(store)
    , m_service(std::make_unique<Wid::IpcClient>(Wid::ipcServerName()))
{
    loadSettings();

    // The settings app writes the file atomically (a new file replaces the old one), so the
    // folder is watched as well and the file is added again after each change.
    m_settingsWatcher = new QFileSystemWatcher(this);
    const QString file = Wid::appSettingsFilePath();
    const QString folder = QFileInfo(file).absolutePath();
    if (QFileInfo::exists(folder)) {
        m_settingsWatcher->addPath(folder);
    }
    if (QFileInfo::exists(file)) {
        m_settingsWatcher->addPath(file);
    }
    connect(m_settingsWatcher, &QFileSystemWatcher::fileChanged, this, &Companion::settingsFileChanged);
    connect(m_settingsWatcher, &QFileSystemWatcher::directoryChanged, this, &Companion::settingsFileChanged);

    connect(m_store, &NoteStore::noteRemoved, this, [this](const QString &id) {
        destroyWindow(id);
    });
    connect(m_service.get(), &Wid::IpcClient::event, this, &Companion::serviceEvent);
}

Companion::~Companion()
{
    m_closing = true;
    for (QPointer<QQuickWindow> &window : m_windows) {
        delete window.data();
    }
    m_windows.clear();
    // The pipe server goes first: no request may reach a half destroyed companion.
    m_server.reset();
    m_service.reset();
}

void Companion::loadSettings()
{
    QSettings settings(Wid::appSettingsFilePath(), QSettings::IniFormat);
    settings.beginGroup(QLatin1String(Wid::AppSettingsGroup));
    m_enabled = readBool(settings, QStringLiteral("NotesEnabled"), true);
    m_defaultColor = colorIndex(settings.value(QStringLiteral("NoteColor")).toString());
    settings.endGroup();
}

void Companion::settingsFileChanged()
{
    const QString file = Wid::appSettingsFilePath();
    if (QFileInfo::exists(file) && !m_settingsWatcher->files().contains(file)) {
        m_settingsWatcher->addPath(file);
    }
    const bool wasEnabled = m_enabled;
    loadSettings();
    if (wasEnabled == m_enabled) {
        return;
    }
    if (m_enabled) {
        start();
    } else {
        const QStringList ids = m_windows.keys();
        for (const QString &id : ids) {
            destroyWindow(id);
        }
    }
}

bool Companion::registerService()
{
    m_server = std::make_unique<Wid::JsonLineServer>(Wid::notesServerName(), [this](const QString &method, const QJsonObject &params, QString *error) {
        return handleRequest(method, params, error);
    });
    if (!m_server->listen()) {
        m_server.reset();
        return false;
    }
    return true;
}

QJsonValue Companion::handleRequest(const QString &method, const QJsonObject &params, QString *error)
{
    // Requests that change windows run after the answer was sent, so the caller never waits
    // for QML (and a window announcing itself to the service cannot deadlock with it).
    const auto later = [this](auto function) {
        QTimer::singleShot(0, this, function);
        return QJsonValue(true);
    };
    if (method == QLatin1String("newNote")) {
        // The service remembers where the note goes (params x, y); nothing to do with them here.
        return later([this]() {
            newNote();
        });
    }
    if (method == QLatin1String("toggleNotes")) {
        return later([this]() {
            toggleNotes();
        });
    }
    if (method == QLatin1String("setNotesVisible")) {
        const bool visible = params.value(QStringLiteral("visible")).toBool(true);
        return later([this, visible]() {
            setNotesVisible(visible);
        });
    }
    if (method == QLatin1String("reannounceNotes")) {
        return later([this]() {
            reannounceNotes();
        });
    }
    if (method == QLatin1String("notes")) {
        return notes();
    }
    if (method == QLatin1String("version")) {
        return QStringLiteral(WID_VERSION_STRING);
    }
    if (method == QLatin1String("quit")) {
        return later([this]() {
            quit();
        });
    }
    *error = QStringLiteral("unknown method");
    return {};
}

void Companion::start()
{
    if (!m_enabled) {
        return;
    }
    // The canvas forgets places of notes deleted while it was not running.
    callService(QStringLiteral("retainNotes"), {{QStringLiteral("ids"), QJsonArray::fromStringList(m_store->ids())}});
    if (!m_store->notesVisible()) {
        return;
    }
    for (const auto &note : m_store->notes()) {
        if (!note->isHidden()) {
            showWindow(note.get());
        }
    }
    refreshPinStates();
}

void Companion::callService(const QString &method, const QJsonObject &params)
{
    // Fire and forget: when the service is not running, the call is dropped.
    m_service->call(method, params);
}

void Companion::announce(const QString &id, QQuickWindow *window)
{
    // Must reach the service before the window appears, so this one call waits (briefly).
    const quintptr handle = quintptr(window->winId());
    QString error;
    m_service->callBlocking(QStringLiteral("expectNote"),
                            {{QStringLiteral("id"), id}, {QStringLiteral("hwnd"), QString::number(handle)}},
                            500ms,
                            &error);
}

void Companion::showWindow(Note *note)
{
    if (!note || m_windows.value(note->id())) {
        return;
    }
    QQmlComponent component(m_engine);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    component.loadFromModule("InfinitDesktop.Notes", "NoteWindow");
#else
    component.loadUrl(QUrl(QStringLiteral("qrc:/InfinitDesktop/Notes/qml/NoteWindow.qml")));
#endif
    QObject *object = component.createWithInitialProperties({
        {QStringLiteral("note"), QVariant::fromValue(note)},
        {QStringLiteral("companion"), QVariant::fromValue(this)},
    });
    auto *window = qobject_cast<QQuickWindow *>(object);
    if (!window) {
        qWarning() << "cannot create a note window:" << component.errorString();
        delete object;
        return;
    }
    QQmlEngine::setObjectOwnership(window, QQmlEngine::CppOwnership);
    const QString id = note->id();
    m_windows.insert(id, window);
    // Closed by the user (Alt+F4, task manager): the note is hidden, not deleted.
    connect(window, &QWindow::visibleChanged, this, [this, id](bool visible) {
        if (visible || m_closing) {
            return;
        }
        if (Note *note = m_store->find(id)) {
            note->setHidden(true);
        }
        QTimer::singleShot(0, this, [this, id]() {
            destroyWindow(id);
        });
    });
    announce(id, window);
    window->show();
}

void Companion::destroyWindow(const QString &id)
{
    QPointer<QQuickWindow> window = m_windows.take(id);
    if (!window) {
        return;
    }
    m_closing = true;
    window->hide();
    m_closing = false;
    window->deleteLater();
}

void Companion::newNote()
{
    if (!m_enabled) {
        return;
    }
    if (!m_store->notesVisible()) {
        setNotesVisible(true);
    }
    if (Note *note = m_store->create(m_defaultColor)) {
        showWindow(note);
        if (QQuickWindow *window = m_windows.value(note->id())) {
            window->requestActivate();
        }
    }
}

void Companion::toggleNotes()
{
    setNotesVisible(!m_store->notesVisible() || m_windows.isEmpty());
}

void Companion::setNotesVisible(bool visible)
{
    m_store->setNotesVisible(visible);
    if (visible && m_enabled) {
        bool any = false;
        for (const auto &note : m_store->notes()) {
            // "Show notes" brings back the ones hidden one by one, too.
            note->setHidden(false);
            showWindow(note.get());
            any = true;
        }
        if (any) {
            refreshPinStates();
        }
    } else {
        const QStringList ids = m_windows.keys();
        for (const QString &id : ids) {
            destroyWindow(id);
        }
    }
}

void Companion::reannounceNotes()
{
    // The service may have been restarted: listen to its events again.
    m_service->connectToServer();
    if (!m_enabled || !m_store->notesVisible()) {
        return;
    }
    const QStringList ids = m_windows.keys();
    for (const QString &id : ids) {
        destroyWindow(id);
    }
    // Windows are destroyed later; show the new ones after that.
    QTimer::singleShot(0, this, [this]() {
        if (!m_enabled || !m_store->notesVisible()) {
            return;
        }
        for (const auto &note : m_store->notes()) {
            if (!note->isHidden()) {
                showWindow(note.get());
            }
        }
        refreshPinStates();
    });
}

QJsonArray Companion::notes() const
{
    QJsonArray array;
    for (const auto &note : m_store->notes()) {
        array.append(QJsonObject{
            {QStringLiteral("id"), note->id()},
            {QStringLiteral("title"), note->title()},
            {QStringLiteral("color"), note->color()},
            {QStringLiteral("hidden"), note->isHidden()},
        });
    }
    return array;
}

void Companion::quit()
{
    m_store->flush();
    QCoreApplication::quit();
}

void Companion::togglePin(const QString &id, bool keepAbove)
{
    if (Note *note = m_store->find(id)) {
        const bool pin = keepAbove ? !(note->isPinned() && note->keepAbove()) : !note->isPinned();
        callService(QStringLiteral("setNotePinned"),
                    {{QStringLiteral("id"), id}, {QStringLiteral("pinned"), pin}, {QStringLiteral("keepAbove"), keepAbove && pin}});
    }
}

void Companion::hideNote(const QString &id)
{
    if (Note *note = m_store->find(id)) {
        note->setHidden(true);
        destroyWindow(id);
    }
}

void Companion::deleteNote(const QString &id)
{
    m_store->remove(id);
    callService(QStringLiteral("forgetNote"), {{QStringLiteral("id"), id}});
}

void Companion::showMap()
{
    callService(QStringLiteral("showMap"));
}

void Companion::openSettings()
{
    QProcess::startDetached(Wid::siblingProgram(QLatin1String(Wid::SettingsExecutable)), {QStringLiteral("--page"), QStringLiteral("organize")});
}

QColor Companion::paperColor(int color) const
{
    return PaperColors[std::clamp(color, 0, 4)];
}

QString Companion::colorName(int color) const
{
    switch (color) {
    case 1:
        return i18nc("@item note color", "Blue");
    case 2:
        return i18nc("@item note color", "Green");
    case 3:
        return i18nc("@item note color", "Pink");
    case 4:
        return i18nc("@item note color", "Gray");
    default:
        return i18nc("@item note color", "Yellow");
    }
}

void Companion::serviceEvent(const QString &name, const QJsonObject &params)
{
    if (name == QLatin1String("notePinnedChanged")) {
        if (Note *note = m_store->find(params.value(QStringLiteral("id")).toString())) {
            note->setPinned(params.value(QStringLiteral("pinned")).toBool(), params.value(QStringLiteral("keepAbove")).toBool());
        }
    }
}

void Companion::applyNoteState(const QJsonValue &value)
{
    // The service answers with the object, or (like the KDE effect) with its JSON text.
    const QJsonObject state = value.isString() ? QJsonDocument::fromJson(value.toString().toUtf8()).object() : value.toObject();
    if (Note *note = m_store->find(state.value(QStringLiteral("id")).toString())) {
        note->setPinned(state.value(QStringLiteral("pinned")).toBool(), state.value(QStringLiteral("keepAbove")).toBool());
    }
}

void Companion::refreshPinStates()
{
    // The canvas owns the pin state; ask it once for the shown notes.
    for (auto it = m_windows.cbegin(); it != m_windows.cend(); ++it) {
        m_service->call(QStringLiteral("noteState"), {{QStringLiteral("id"), it.key()}}, this, [this](const QJsonValue &result, const QString &error) {
            if (error.isEmpty()) {
                applyNoteState(result);
            }
        });
    }
}

} // namespace InfinitDesktop
