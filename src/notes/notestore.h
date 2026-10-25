/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>

#include <memory>
#include <vector>

namespace InfinitDesktop
{

/// One note: Markdown text and how its window looks. Where it lies is the canvas' business.
class Note : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(QString title READ title NOTIFY textChanged)
    Q_PROPERTY(int color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(int width READ width WRITE setWidth NOTIFY sizeChanged)
    Q_PROPERTY(int height READ height WRITE setHeight NOTIFY sizeChanged)
    Q_PROPERTY(bool hidden READ isHidden WRITE setHidden NOTIFY hiddenChanged)
    /// Shows the formatted text instead of the Markdown source (which is the default).
    Q_PROPERTY(bool formatted READ isFormatted WRITE setFormatted NOTIFY formattedChanged)
    Q_PROPERTY(bool pinned READ isPinned NOTIFY pinnedChanged)
    Q_PROPERTY(bool keepAbove READ keepAbove NOTIFY pinnedChanged)

public:
    static constexpr int MinimumWidth = 180;
    static constexpr int MinimumHeight = 120;
    static constexpr int Colors = 5;

    Note(const QString &id, QObject *parent);

    QString id() const
    {
        return m_id;
    }
    QString text() const
    {
        return m_text;
    }
    void setText(const QString &text);
    /// First line without Markdown heading marks, or empty.
    QString title() const;
    int color() const
    {
        return m_color;
    }
    void setColor(int color);
    int width() const
    {
        return m_width;
    }
    void setWidth(int width);
    int height() const
    {
        return m_height;
    }
    void setHeight(int height);
    bool isHidden() const
    {
        return m_hidden;
    }
    void setHidden(bool hidden);
    bool isFormatted() const
    {
        return m_formatted;
    }
    void setFormatted(bool formatted);
    bool isPinned() const
    {
        return m_pinned;
    }
    bool keepAbove() const
    {
        return m_keepAbove;
    }
    /// From the canvas (it owns the pin state).
    void setPinned(bool pinned, bool keepAbove);

Q_SIGNALS:
    void textChanged();
    void colorChanged();
    void sizeChanged();
    void hiddenChanged();
    void formattedChanged();
    void pinnedChanged();
    /// Something that is saved changed.
    void modified();

private:
    friend class NoteStore;
    QString m_id;
    QString m_text;
    int m_color = 0;
    int m_width = 300;
    int m_height = 260;
    bool m_hidden = false;
    bool m_formatted = false;
    bool m_pinned = false;
    bool m_keepAbove = false;
};

/**
 * All notes, stored in %APPDATA%\WIN Infinit Desktop\notes: the text of each note in
 * <id>.md (readable and editable outside the app), everything else in index.json. Saving is
 * delayed a little and atomic (QSaveFile), so typing never waits for the disk and a crash
 * never leaves half a file.
 */
class NoteStore : public QObject
{
    Q_OBJECT

public:
    static constexpr int MaxNotes = 1000;
    static constexpr qint64 MaxTextSize = 1024 * 1024;

    explicit NoteStore(QObject *parent = nullptr);
    ~NoteStore() override;

    /// Reads all notes from disk (replacing the ones in memory).
    void load();
    /// Writes everything that changed now.
    void flush();

    const std::vector<std::unique_ptr<Note>> &notes() const
    {
        return m_notes;
    }
    Note *find(const QString &id) const;
    /// A new, empty note; nullptr when there are too many.
    Note *create(int color);
    /// Deletes a note and its file. The Note object is deleted later, so QML can let go of it.
    void remove(const QString &id);
    QStringList ids() const;

    /// Whether notes are shown at all (the "show / hide notes" toggle), stored in the index.
    bool notesVisible() const
    {
        return m_visible;
    }
    void setNotesVisible(bool visible);

    static QString directory();
    static bool isValidId(const QString &id);

Q_SIGNALS:
    void noteAdded(InfinitDesktop::Note *note);
    void noteRemoved(const QString &id);

private:
    void watch(Note *note);
    void scheduleSave(const QString &id);
    void saveIndex();

    std::vector<std::unique_ptr<Note>> m_notes;
    QSet<QString> m_dirtyTexts;
    bool m_indexDirty = false;
    bool m_visible = true;
    QTimer m_saveTimer;
};

} // namespace InfinitDesktop
