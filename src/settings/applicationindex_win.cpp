/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later

    The Start menu (the shell's AppsFolder) for ApplicationIndex, and application icons
    for the image provider.
*/
#include "applicationindex.h"
#include "searchutil.h"
#include "shellitem_win.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

#include <knownfolders.h>
#include <shlobj.h>
#include <shobjidl.h>

namespace InfinitDesktop
{

namespace
{

/// "{GUID}\Mozilla Firefox\firefox.exe" -> "C:\Program Files\Mozilla Firefox\firefox.exe".
QString resolveKnownFolder(const QString &parsingName)
{
    if (!parsingName.startsWith(QLatin1Char('{'))) {
        return parsingName;
    }
    const int close = parsingName.indexOf(QLatin1Char('}'));
    if (close < 0) {
        return QString();
    }
    const QString guidText = parsingName.left(close + 1);
    GUID guid = {};
    if (FAILED(CLSIDFromString(reinterpret_cast<LPCOLESTR>(guidText.utf16()), &guid))) {
        return QString();
    }
    PWSTR base = nullptr;
    if (FAILED(SHGetKnownFolderPath(guid, KF_FLAG_DEFAULT, nullptr, &base)) || !base) {
        if (base) {
            CoTaskMemFree(base);
        }
        return QString();
    }
    const QString folder = QString::fromWCharArray(base);
    CoTaskMemFree(base);
    return QDir::toNativeSeparators(folder + parsingName.mid(close + 1));
}

} // namespace

QList<ApplicationIndex::Entry> ApplicationIndex::readStartMenu()
{
    QList<Entry> entries;
    const ComApartment apartment;
    if (!apartment.ok()) {
        return entries;
    }
    ComPtr<IShellItem> folder;
    if (FAILED(SHGetKnownFolderItem(FOLDERID_AppsFolder, KF_FLAG_DEFAULT, nullptr, IID_PPV_ARGS(folder.put())))) {
        return entries;
    }
    ComPtr<IEnumShellItems> items;
    if (FAILED(folder->BindToHandler(nullptr, BHID_EnumItems, IID_PPV_ARGS(items.put())))) {
        return entries;
    }
    static const QRegularExpression uninstaller(QStringLiteral("uninstall|deinstall|entfernen"), QRegularExpression::CaseInsensitiveOption);
    QSet<QString> seen;
    // A Start menu with thousands of entries is broken; stop there.
    for (int count = 0; count < 4000; ++count) {
        ComPtr<IShellItem> item;
        if (items->Next(1, item.put(), nullptr) != S_OK) {
            break;
        }
        const QString name = displayName(item.get(), SIGDN_NORMALDISPLAY);
        const QString parsing = displayName(item.get(), SIGDN_PARSINGNAME);
        if (name.isEmpty() || parsing.isEmpty()) {
            continue;
        }
        Entry entry;
        entry.name = name;
        entry.iconName = shellIconName(parsing);
        entry.hasIcon = true;
        if (parsing.contains(QLatin1Char('\\')) || parsing.contains(QLatin1Char('/'))) {
            // A classic program without its own AppUserModelID: the service knows its
            // windows by the program path.
            const QString path = resolveKnownFolder(parsing);
            if (!path.endsWith(QLatin1String(".exe"), Qt::CaseInsensitive)) {
                continue; // documents, web links, help files
            }
            entry.id = path;
            entry.windowClass = QFileInfo(path).completeBaseName().toLower();
        } else {
            entry.id = parsing;
        }
        entry.key = entry.id.toLower();
        if (seen.contains(entry.key)) {
            continue;
        }
        seen.insert(entry.key);
        entry.noDisplay = uninstaller.match(name).hasMatch();
        entry.searchText = SearchUtil::normalize(QStringList{entry.name, entry.windowClass, entry.id}.join(QLatin1Char(' ')));
        entries.append(entry);
    }
    return entries;
}

} // namespace InfinitDesktop
