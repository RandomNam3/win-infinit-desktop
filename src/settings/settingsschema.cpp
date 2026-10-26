/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "settingsschema.h"

#include "localizer.h"
#include "settingsfile.h"

#include <QFile>
#include <QXmlStreamReader>

#include <algorithm>

namespace InfiniteDesktop
{

namespace
{

QString translated(const QString &text)
{
    return text.isEmpty() ? text : Wid::Localizer::instance().translate(QString(), text);
}

QColor parseColor(const QVariant &value)
{
    if (value.metaType() == QMetaType::fromType<QColor>()) {
        return value.value<QColor>();
    }
    // "r,g,b" as KConfig stores colors; QSettings may have split it into a list.
    const QStringList parts = value.typeId() == QMetaType::QStringList ? value.toStringList() : value.toString().split(QLatin1Char(','));
    if (parts.size() >= 3) {
        bool ok[3] = {false, false, false};
        const int r = parts[0].trimmed().toInt(&ok[0]);
        const int g = parts[1].trimmed().toInt(&ok[1]);
        const int b = parts[2].trimmed().toInt(&ok[2]);
        if (ok[0] && ok[1] && ok[2]) {
            return QColor(std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255));
        }
        return QColor();
    }
    return QColor::fromString(value.toString().trimmed());
}

QStringList parseList(const QVariant &value)
{
    QStringList list = value.typeId() == QMetaType::QStringList ? value.toStringList() : value.toString().split(QLatin1Char(','));
    QStringList result;
    for (const QString &entry : std::as_const(list)) {
        const QString trimmed = entry.trimmed();
        if (!trimmed.isEmpty() && !result.contains(trimmed)) {
            result.append(trimmed);
        }
    }
    return result;
}

} // namespace

QString SchemaItem::label() const
{
    return translated(m_label);
}

QString SchemaItem::whatsThis() const
{
    return translated(m_whatsThis);
}

QList<SchemaItem::Choice> SchemaItem::choices() const
{
    QList<Choice> result = m_choices;
    for (Choice &choice : result) {
        choice.label = translated(choice.label);
    }
    return result;
}

bool SchemaItem::isEqual(const QVariant &value) const
{
    switch (m_type) {
    case Type::Bool:
        return m_value.toBool() == value.toBool();
    case Type::Int:
    case Type::Enum:
        return m_value.toInt() == value.toInt();
    case Type::Color:
        return m_value.value<QColor>().rgb() == parseColor(value).rgb();
    case Type::StringList:
        return m_value.toStringList() == value.toStringList();
    }
    return false;
}

void SchemaItem::setProperty(const QVariant &value)
{
    switch (m_type) {
    case Type::Bool:
        m_value = value.toBool();
        break;
    case Type::Int: {
        int number = value.toInt();
        if (m_min) {
            number = std::max(number, *m_min);
        }
        if (m_max) {
            number = std::min(number, *m_max);
        }
        m_value = number;
        break;
    }
    case Type::Enum:
        m_value = std::clamp(value.toInt(), 0, std::max(0, int(m_choices.size()) - 1));
        break;
    case Type::Color: {
        const QColor color = parseColor(value);
        if (color.isValid()) {
            m_value = color;
        }
        break;
    }
    case Type::StringList:
        m_value = parseList(value);
        break;
    }
}

QVariant SchemaItem::toStored(const QVariant &value) const
{
    switch (m_type) {
    case Type::Bool:
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    case Type::Int:
        return QString::number(value.toInt());
    case Type::Enum: {
        const int index = value.toInt();
        return index >= 0 && index < m_choices.size() ? m_choices.at(index).name : QString::number(index);
    }
    case Type::Color: {
        const QColor color = parseColor(value);
        return QStringLiteral("%1,%2,%3").arg(color.red()).arg(color.green()).arg(color.blue());
    }
    case Type::StringList:
        return value.toStringList().join(QLatin1Char(','));
    }
    return QVariant();
}

QVariant SchemaItem::fromStored(const QVariant &stored) const
{
    if (!stored.isValid()) {
        return QVariant();
    }
    switch (m_type) {
    case Type::Bool: {
        const QString text = stored.toString().trimmed().toLower();
        if (text == QLatin1String("true") || text == QLatin1String("1")) {
            return true;
        }
        if (text == QLatin1String("false") || text == QLatin1String("0")) {
            return false;
        }
        return QVariant();
    }
    case Type::Int: {
        bool ok = false;
        int number = stored.toString().trimmed().toInt(&ok);
        if (!ok) {
            return QVariant();
        }
        if (m_min) {
            number = std::max(number, *m_min);
        }
        if (m_max) {
            number = std::min(number, *m_max);
        }
        return number;
    }
    case Type::Enum: {
        const QString text = stored.toString().trimmed();
        for (int i = 0; i < m_choices.size(); ++i) {
            if (text.compare(m_choices.at(i).name, Qt::CaseInsensitive) == 0) {
                return i;
            }
        }
        bool ok = false;
        const int index = text.toInt(&ok);
        return ok && index >= 0 && index < m_choices.size() ? QVariant(index) : QVariant();
    }
    case Type::Color: {
        const QColor color = parseColor(stored);
        return color.isValid() ? QVariant(color) : QVariant();
    }
    case Type::StringList:
        return parseList(stored);
    }
    return QVariant();
}

void SchemaItem::readConfig(const SettingsFile &file)
{
    const QVariant value = fromStored(file.value(SettingsSchema::instance().group(), m_name));
    m_value = value.isValid() ? value : m_default;
}

void SchemaItem::writeConfig(SettingsFile &file) const
{
    const QString group = SettingsSchema::instance().group();
    if (isDefault()) {
        file.remove(group, m_name);
    } else {
        file.setValue(group, m_name, toStored(m_value));
    }
}

// ---------------------------------------------------------------------------------------

SettingsSchema &SettingsSchema::instance()
{
    static SettingsSchema schema = []() {
        SettingsSchema result;
        QFile file(QStringLiteral(":/settings/win-infinite-desktop.kcfg"));
        QString error;
        if (!file.open(QIODevice::ReadOnly) || !result.parse(file.readAll(), &error)) {
            qWarning("SettingsSchema: cannot read the schema: %s", qPrintable(error));
        }
        return result;
    }();
    return schema;
}

SchemaItem *SettingsSchema::find(const QString &name) const
{
    for (const auto &item : m_items) {
        if (item->name() == name) {
            return item.get();
        }
    }
    return nullptr;
}

bool SettingsSchema::parse(const QByteArray &xml, QString *error)
{
    QXmlStreamReader reader(xml);
    std::unique_ptr<SchemaItem> item;
    SchemaItem::Choice choice;
    bool inChoice = false;
    QString defaultText;

    const auto finishItem = [&]() {
        if (!item) {
            return;
        }
        const QString text = defaultText.trimmed();
        switch (item->m_type) {
        case SchemaItem::Type::Bool:
            item->m_default = text.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;
            break;
        case SchemaItem::Type::Int:
            item->m_default = text.toInt();
            break;
        case SchemaItem::Type::Enum: {
            int index = 0;
            for (int i = 0; i < item->m_choices.size(); ++i) {
                if (item->m_choices.at(i).name == text) {
                    index = i;
                }
            }
            item->m_default = index;
            break;
        }
        case SchemaItem::Type::Color:
            item->m_default = parseColor(text);
            break;
        case SchemaItem::Type::StringList:
            item->m_default = parseList(text);
            break;
        }
        item->m_value = item->m_default;
        m_items.push_back(std::move(item));
        defaultText.clear();
    };

    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement()) {
            const auto name = reader.name();
            if (name == QLatin1String("group")) {
                m_group = reader.attributes().value(QLatin1String("name")).toString();
            } else if (name == QLatin1String("entry")) {
                item = std::make_unique<SchemaItem>();
                item->m_name = reader.attributes().value(QLatin1String("name")).toString();
                const auto type = reader.attributes().value(QLatin1String("type"));
                if (type == QLatin1String("Bool")) {
                    item->m_type = SchemaItem::Type::Bool;
                } else if (type == QLatin1String("Int")) {
                    item->m_type = SchemaItem::Type::Int;
                } else if (type == QLatin1String("Enum")) {
                    item->m_type = SchemaItem::Type::Enum;
                } else if (type == QLatin1String("Color")) {
                    item->m_type = SchemaItem::Type::Color;
                } else if (type == QLatin1String("StringList")) {
                    item->m_type = SchemaItem::Type::StringList;
                } else {
                    if (error) {
                        *error = QStringLiteral("unsupported type %1 of %2").arg(type.toString(), item->m_name);
                    }
                    return false;
                }
            } else if (name == QLatin1String("choice") && item) {
                inChoice = true;
                choice = SchemaItem::Choice{reader.attributes().value(QLatin1String("name")).toString(), QString()};
            } else if (name == QLatin1String("label") && item) {
                const QString text = reader.readElementText().simplified();
                if (inChoice) {
                    choice.label = text;
                } else {
                    item->m_label = text;
                }
            } else if (name == QLatin1String("whatsthis") && item) {
                item->m_whatsThis = reader.readElementText().simplified();
            } else if (name == QLatin1String("default") && item) {
                defaultText = reader.readElementText();
            } else if (name == QLatin1String("min") && item) {
                item->m_min = reader.readElementText().trimmed().toInt();
            } else if (name == QLatin1String("max") && item) {
                item->m_max = reader.readElementText().trimmed().toInt();
            }
        } else if (reader.isEndElement()) {
            const auto name = reader.name();
            if (name == QLatin1String("choice") && item) {
                if (choice.label.isEmpty()) {
                    choice.label = choice.name;
                }
                item->m_choices.append(choice);
                inChoice = false;
            } else if (name == QLatin1String("entry")) {
                finishItem();
            }
        }
    }
    if (reader.hasError()) {
        if (error) {
            *error = reader.errorString();
        }
        return false;
    }
    return !m_items.empty();
}

} // namespace InfiniteDesktop
