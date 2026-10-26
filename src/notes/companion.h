/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <QColor>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QPointer>
#include <QString>

#include <memory>

class QFileSystemWatcher;
class QQmlEngine;
class QQuickWindow;

namespace Wid
{
class IpcClient;
class JsonLineServer;
}

namespace InfiniteDesktop
{

class Note;
class NoteStore;

/**
 * The notes app wid-notes.exe: note windows and the pipe the canvas service calls (new note,
 * show / hide notes, announce the note windows again). The tray icon belongs to the service.
 *
 * Every note window is announced to the service (expectNote with its window handle) right
 * before it is shown, so the canvas knows which note it is and places it where it was. A
 * window exists only while its note is shown; hiding a note destroys its window.
 */
class Companion : public QObject
{
    Q_OBJECT

public:
    Companion(QQmlEngine *engine, NoteStore *store, QObject *parent = nullptr);
    ~Companion() override;

    /// Serves the notes pipe; false if another notes app has it already.
    bool registerService();
    /// Shows the notes that are not hidden.
    void start();

    /// Requests on the notes pipe; public for the unit tests.
    QJsonValue handleRequest(const QString &method, const QJsonObject &params, QString *error);

public Q_SLOTS:
    void newNote();
    void toggleNotes();
    void setNotesVisible(bool visible);
    /// The canvas (re)started and does not know the open note windows: show them again.
    void reannounceNotes();
    /// {id, title, color, hidden} of every note.
    QJsonArray notes() const;
    void quit();

    // From the note windows
    Q_INVOKABLE void togglePin(const QString &id, bool keepAbove);
    Q_INVOKABLE void hideNote(const QString &id);
    Q_INVOKABLE void deleteNote(const QString &id);
    Q_INVOKABLE void showMap();
    Q_INVOKABLE void openSettings();
    /// Paper color of note color index `color`.
    Q_INVOKABLE QColor paperColor(int color) const;
    Q_INVOKABLE QString colorName(int color) const;

private:
    void showWindow(Note *note);
    void destroyWindow(const QString &id);
    void announce(const QString &id, QQuickWindow *window);
    void callService(const QString &method, const QJsonObject &params = {});
    void serviceEvent(const QString &name, const QJsonObject &params);
    void applyNoteState(const QJsonValue &state);
    void loadSettings();
    void settingsFileChanged();
    void refreshPinStates();

    QQmlEngine *const m_engine;
    NoteStore *const m_store;
    QHash<QString, QPointer<QQuickWindow>> m_windows;
    std::unique_ptr<Wid::IpcClient> m_service;
    std::unique_ptr<Wid::JsonLineServer> m_server;
    QFileSystemWatcher *m_settingsWatcher = nullptr;
    bool m_enabled = true;
    int m_defaultColor = 0;
    /// Windows hidden by us (not by the user closing them).
    bool m_closing = false;
};

} // namespace InfiniteDesktop
