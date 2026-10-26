/*
    WIN Infinite Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinite Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <QJsonObject>
#include <QObject>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <functional>

class QJSEngine;
class QQmlEngine;

namespace InfiniteDesktop
{

/**
 * Groups, areas and notes of the canvas, for the "Notes & Groups" page
 * (groups/setGroups/areas/setAreas on the service's pipe, notes on the notes app's).
 *
 * @c groups is a list of maps: id, name, slot (1-based shortcut slot), automatic,
 * openWindows and members (list of maps: appKey, launch, name, iconName, hasIcon).
 * @c areas is a list of maps: id, name, dynamic, color.
 * Edits change the JSON documents of the effect and send them back as a whole.
 */
class WorkspaceModel : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Workspace)
    QML_SINGLETON
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(QVariantList groups READ groups NOTIFY changed)
    Q_PROPERTY(QVariantList areas READ areas NOTIFY changed)
    Q_PROPERTY(int noteCount READ noteCount NOTIFY changed)

public:
    explicit WorkspaceModel(QObject *parent);

    static WorkspaceModel *instance();
    static WorkspaceModel *create(QQmlEngine *qmlEngine, QJSEngine *jsEngine);
    static void setDemoMode(bool demo);
    /// "Microsoft.WindowsTerminal_8wekyb3d8bbwe!App" -> lower case, like the service's application keys.
    static QString applicationKey(const QString &appId);

    bool available() const
    {
        return m_available;
    }
    QVariantList groups() const
    {
        return m_groups;
    }
    QVariantList areas() const
    {
        return m_areas;
    }
    int noteCount() const
    {
        return m_noteCount;
    }

    Q_INVOKABLE void refresh();

    Q_INVOKABLE void addGroup();
    Q_INVOKABLE void renameGroup(const QString &id, const QString &name);
    Q_INVOKABLE void setGroupAutomatic(const QString &id, bool automatic);
    Q_INVOKABLE void addGroupApplication(const QString &id, const QString &appId);
    Q_INVOKABLE void removeGroupApplication(const QString &id, int index);
    Q_INVOKABLE void moveGroup(const QString &id, int delta);
    Q_INVOKABLE void removeGroup(const QString &id);
    Q_INVOKABLE void openGroup(const QString &id);
    Q_INVOKABLE void saveGroupLayout(const QString &id);
    Q_INVOKABLE void newGroupFromSelection();

    Q_INVOKABLE void renameArea(const QString &id, const QString &name);
    Q_INVOKABLE void setAreaDynamic(const QString &id, bool dynamic);
    Q_INVOKABLE void removeArea(const QString &id);
    Q_INVOKABLE void newAreaFromSelection();

    Q_INVOKABLE void newNote();
    Q_INVOKABLE void showMap();

    /// Parse the documents of groups() / areas(); public for testing.
    void parseGroups(const QByteArray &json);
    void parseAreas(const QByteArray &json);

Q_SIGNALS:
    void changed();
    void failed(const QString &message);

private Q_SLOTS:
    void remoteChanged();

private:
    void callEffect(const QString &method, const QJsonObject &params, std::function<void(const QJsonValue &)> handler = {});
    void pushGroups();
    void pushAreas();
    void rebuildGroups();
    void rebuildAreas();

    bool m_available = false;
    QJsonObject m_groupsDocument;
    QJsonObject m_areasDocument;
    QVariantList m_groups;
    QVariantList m_areas;
    int m_noteCount = 0;
};

} // namespace InfiniteDesktop
