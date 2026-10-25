/*
    WIN Infinit Desktop - infinite canvas for Windows 11
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#pragma once

#include <QImage>
#include <QSize>
#include <QString>

#include <objbase.h>
#include <shobjidl.h>

namespace InfinitDesktop
{

/// Owns one COM reference.
template<typename T>
class ComPtr
{
public:
    ComPtr() = default;
    ~ComPtr()
    {
        reset();
    }
    ComPtr(const ComPtr &) = delete;
    ComPtr &operator=(const ComPtr &) = delete;

    T *get() const
    {
        return m_pointer;
    }
    T *operator->() const
    {
        return m_pointer;
    }
    explicit operator bool() const
    {
        return m_pointer != nullptr;
    }
    /// Releases the current pointer and returns the slot for a new one.
    T **put()
    {
        reset();
        return &m_pointer;
    }
    void reset()
    {
        if (m_pointer) {
            m_pointer->Release();
            m_pointer = nullptr;
        }
    }

private:
    T *m_pointer = nullptr;
};

/// COM on this thread for the lifetime of the object (single threaded apartment).
class ComApartment
{
public:
    ComApartment()
    {
        const HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        m_ok = SUCCEEDED(result) || result == RPC_E_CHANGED_MODE;
        m_owned = SUCCEEDED(result);
    }
    ~ComApartment()
    {
        if (m_owned) {
            CoUninitialize();
        }
    }
    ComApartment(const ComApartment &) = delete;
    ComApartment &operator=(const ComApartment &) = delete;

    bool ok() const
    {
        return m_ok;
    }

private:
    bool m_ok = false;
    bool m_owned = false;
};

/// Display name of a shell item, empty on failure.
QString displayName(IShellItem *item, SIGDN form);

/**
 * The icon of a shell item at `size`: "shell:AppsFolder\<parsingName>" for a Start menu
 * entry, or a file path. A null image on failure. Must run on a thread with COM.
 */
QImage shellItemIcon(const QString &parsingName, bool appsFolder, QSize size);

} // namespace InfinitDesktop
