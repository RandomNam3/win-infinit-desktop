/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "notestore.h"

#include "apppaths.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>

#include <algorithm>

using namespace std::chrono_literals;

namespace InfinitDesktop
{

namespace
{

bool writeFile(const QString &path, const QByteArray &data)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(data);
    return file.commit();
}

} // namespace

Note::Note(const QString &id, QObject *parent)
    : QObject(parent)
    , m_id(id)
{
}

void Note::setText(const QString &text)
{
    if (text == m_text) {
        return;
    }
    m_text = text.left(NoteStore::MaxTextSize / 2);
    Q_EMIT textChanged();
    Q_EMIT modified();
}

QString Note::title() const
{
    const QStringList lines = m_text.split(QLatin1Char('\n'));
    for (QString line : lines) {
        line = line.trimmed();
        while (line.startsWith(QLatin1Char('#'))) {
            line.remove(0, 1);
        }
        line = line.trimmed();
        if (!line.isEmpty()) {
            return line.left(80);
        }
    }
    return QString();
}

void Note::setColor(int color)
{
    color = std::clamp(color, 0, Colors - 1);
    if (color != m_color) {
        m_color = color;
        Q_EMIT colorChanged();
        Q_EMIT modified();
    }
}

void Note::setWidth(int width)
{
    width = std::clamp(width, MinimumWidth, 4000);
    if (width != m_width) {
        m_width = width;
        Q_EMIT sizeChanged();
        Q_EMIT modified();
    }
}

void Note::setHeight(int height)
{
    height = std::clamp(height, MinimumHeight, 4000);
    if (height != m_height) {
        m_height = height;
        Q_EMIT sizeChanged();
        Q_EMIT modified();
    }
}

void Note::setHidden(bool hidden)
{
    if (hidden != m_hidden) {
        m_hidden = hidden;
        Q_EMIT hiddenChanged();
        Q_EMIT modified();
    }
}

void Note::setFormatted(bool formatted)
{
    if (formatted != m_formatted) {
        m_formatted = formatted;
        Q_EMIT formattedChanged();
        Q_EMIT modified();
    }
}

void Note::setPinned(bool pinned, bool keepAbove)
{
    keepAbove = pinned && keepAbove;
    if (pinned != m_pinned || keepAbove != m_keepAbove) {
        m_pinned = pinned;
        m_keepAbove = keepAbove;
        Q_EMIT pinnedChanged();
    }
}

NoteStore::NoteStore(QObject *parent)
    : QObject(parent)
{
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(600ms);
    connect(&m_saveTimer, &QTimer::timeout, this, &NoteStore::flush);
}

NoteStore::~NoteStore()
{
    flush();
}

QString NoteStore::directory()
{
    // %APPDATA%\WIN Infinit Desktop\notes: roams with the user profile like the settings.
    return QDir(Wid::roamingDataDirectory()).filePath(QStringLiteral("notes"));
}

bool NoteStore::isValidId(const QString &id)
{
    // Ids end up in file names: only what QUuid produces.
    static const QRegularExpression pattern(QStringLiteral("^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$"));
    return pattern.match(id).hasMatch();
}

void NoteStore::load()
{
    m_notes.clear();
    QFile index(directory() + QStringLiteral("/index.json"));
    if (!index.open(QIODevice::ReadOnly) || index.size() > 4 * 1024 * 1024) {
        return;
    }
    const QJsonObject root = QJsonDocument::fromJson(index.readAll()).object();
    m_visible = root.value(QStringLiteral("visible")).toBool(true);
    const QJsonArray array = root.value(QStringLiteral("notes")).toArray();
    QSet<QString> seen;
    for (const QJsonValue &value : array) {
        if (m_notes.size() >= std::size_t(MaxNotes)) {
            break;
        }
        const QJsonObject object = value.toObject();
        const QString id = object.value(QStringLiteral("id")).toString();
        if (!isValidId(id) || seen.contains(id)) {
            continue;
        }
        seen.insert(id);
        auto note = std::make_unique<Note>(id, this);
        note->m_color = std::clamp(object.value(QStringLiteral("color")).toInt(), 0, Note::Colors - 1);
        note->m_width = std::clamp(object.value(QStringLiteral("width")).toInt(300), Note::MinimumWidth, 4000);
        note->m_height = std::clamp(object.value(QStringLiteral("height")).toInt(260), Note::MinimumHeight, 4000);
        note->m_hidden = object.value(QStringLiteral("hidden")).toBool();
        note->m_formatted = object.value(QStringLiteral("formatted")).toBool();
        QFile text(directory() + QLatin1Char('/') + id + QStringLiteral(".md"));
        if (text.open(QIODevice::ReadOnly) && text.size() <= MaxTextSize) {
            note->m_text = QString::fromUtf8(text.readAll());
        }
        watch(note.get());
        m_notes.push_back(std::move(note));
    }
}

void NoteStore::watch(Note *note)
{
    connect(note, &Note::textChanged, this, [this, note]() {
        scheduleSave(note->id());
    });
    connect(note, &Note::modified, this, [this]() {
        m_indexDirty = true;
        m_saveTimer.start();
    });
}

void NoteStore::scheduleSave(const QString &id)
{
    m_dirtyTexts.insert(id);
    m_indexDirty = true;
    m_saveTimer.start();
}

void NoteStore::flush()
{
    m_saveTimer.stop();
    if (m_dirtyTexts.isEmpty() && !m_indexDirty) {
        return;
    }
    QDir().mkpath(directory());
    for (const QString &id : std::as_const(m_dirtyTexts)) {
        if (const Note *note = find(id)) {
            writeFile(directory() + QLatin1Char('/') + id + QStringLiteral(".md"), note->text().toUtf8());
        }
    }
    m_dirtyTexts.clear();
    if (m_indexDirty) {
        saveIndex();
    }
}

void NoteStore::saveIndex()
{
    QJsonArray array;
    for (const auto &note : m_notes) {
        array.append(QJsonObject{
            {QStringLiteral("id"), note->id()},
            {QStringLiteral("color"), note->color()},
            {QStringLiteral("width"), note->width()},
            {QStringLiteral("height"), note->height()},
            {QStringLiteral("hidden"), note->isHidden()},
            {QStringLiteral("formatted"), note->isFormatted()},
        });
    }
    const QJsonObject root{
        {QStringLiteral("version"), 1},
        {QStringLiteral("visible"), m_visible},
        {QStringLiteral("notes"), array},
    };
    if (writeFile(directory() + QStringLiteral("/index.json"), QJsonDocument(root).toJson(QJsonDocument::Indented))) {
        m_indexDirty = false;
    }
}

Note *NoteStore::find(const QString &id) const
{
    for (const auto &note : m_notes) {
        if (note->id() == id) {
            return note.get();
        }
    }
    return nullptr;
}

Note *NoteStore::create(int color)
{
    if (m_notes.size() >= std::size_t(MaxNotes)) {
        return nullptr;
    }
    auto note = std::make_unique<Note>(QUuid::createUuid().toString(QUuid::WithoutBraces), this);
    note->m_color = std::clamp(color, 0, Note::Colors - 1);
    Note *raw = note.get();
    watch(raw);
    m_notes.push_back(std::move(note));
    scheduleSave(raw->id());
    Q_EMIT noteAdded(raw);
    return raw;
}

void NoteStore::remove(const QString &id)
{
    auto it = std::find_if(m_notes.begin(), m_notes.end(), [&id](const std::unique_ptr<Note> &note) {
        return note->id() == id;
    });
    if (it == m_notes.end()) {
        return;
    }
    // Windows and QML may still refer to it until the event loop runs again.
    Note *note = it->release();
    m_notes.erase(it);
    note->disconnect(this);
    note->deleteLater();
    m_dirtyTexts.remove(id);
    QFile::remove(directory() + QLatin1Char('/') + id + QStringLiteral(".md"));
    m_indexDirty = true;
    m_saveTimer.start();
    Q_EMIT noteRemoved(id);
}

QStringList NoteStore::ids() const
{
    QStringList result;
    result.reserve(int(m_notes.size()));
    for (const auto &note : m_notes) {
        result.append(note->id());
    }
    return result;
}

void NoteStore::setNotesVisible(bool visible)
{
    if (visible != m_visible) {
        m_visible = visible;
        m_indexDirty = true;
        m_saveTimer.start();
    }
}

} // namespace InfinitDesktop
