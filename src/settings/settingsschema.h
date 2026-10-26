/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <QColor>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <memory>
#include <optional>
#include <vector>

namespace InfiniteDesktop
{

class SettingsFile;

/**
 * One entry of win-infinite-desktop.kcfg with its current value: the part of
 * KConfigSkeletonItem the settings model needs, without KConfig.
 *
 * Values in memory: Bool as bool, Int as int, Enum as the index of the choice, Color as
 * QColor, StringList as QStringList. In the file: true/false, the number, the choice name,
 * "r,g,b" and a comma separated list; an entry that has its default value is removed from
 * the file, like KConfig does.
 */
class SchemaItem
{
public:
    enum class Type {
        Bool,
        Int,
        Enum,
        Color,
        StringList,
    };
    struct Choice
    {
        QString name;
        QString label;
    };

    QString name() const
    {
        return m_name;
    }
    Type type() const
    {
        return m_type;
    }
    /// Translated label and explanation.
    QString label() const;
    QString whatsThis() const;
    /// Choices of an Enum entry, labels translated.
    QList<Choice> choices() const;
    std::optional<int> minValue() const
    {
        return m_min;
    }
    std::optional<int> maxValue() const
    {
        return m_max;
    }

    QVariant property() const
    {
        return m_value;
    }
    QVariant getDefault() const
    {
        return m_default;
    }
    bool isDefault() const
    {
        return isEqual(m_default);
    }
    bool isEqual(const QVariant &value) const;
    void setProperty(const QVariant &value);
    void setDefault()
    {
        m_value = m_default;
    }

    /// Reads the value from the file (the default if missing or invalid).
    void readConfig(const SettingsFile &file);
    /// Writes the value (or removes the entry when it has the default value).
    void writeConfig(SettingsFile &file) const;

    /// Text of a value as stored in the file; public for the unit tests.
    QVariant toStored(const QVariant &value) const;
    QVariant fromStored(const QVariant &stored) const;

private:
    friend class SettingsSchema;

    QString m_name;
    Type m_type = Type::Bool;
    QString m_label;
    QString m_whatsThis;
    QList<Choice> m_choices;
    std::optional<int> m_min;
    std::optional<int> m_max;
    QVariant m_default;
    QVariant m_value;
};

/// The entries of the schema file, in its order.
class SettingsSchema
{
public:
    /// The schema compiled into the program (:/settings/win-infinite-desktop.kcfg).
    static SettingsSchema &instance();

    /// Parses kcfg XML; false on errors (the entries read so far are kept).
    bool parse(const QByteArray &xml, QString *error = nullptr);
    /// The group of the entries ("Effect-infinitedesktop").
    QString group() const
    {
        return m_group;
    }
    const std::vector<std::unique_ptr<SchemaItem>> &items() const
    {
        return m_items;
    }
    SchemaItem *find(const QString &name) const;

private:
    QString m_group;
    std::vector<std::unique_ptr<SchemaItem>> m_items;
};

} // namespace InfiniteDesktop
