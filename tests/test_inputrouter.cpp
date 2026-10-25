/*
    SPDX-FileCopyrightText: 2026 WIN Infinit Desktop contributors
    SPDX-License-Identifier: GPL-3.0-or-later
*/
#include "inputrouter.h"

#include <QTest>

using namespace Wid;
using Kind = InputCommand::Kind;

class TestInputRouter : public QObject
{
    Q_OBJECT

    QList<InputCommand> m_commands;
    bool m_desktop = true;
    int m_releases = 0;
    std::unique_ptr<InputRouter> m_router;

    std::shared_ptr<InputSnapshot> snapshot()
    {
        auto snapshot = std::make_shared<InputSnapshot>();
        snapshot->enabled = true;
        snapshot->shortcutsEnabled = true;
        snapshot->panModifiers = MOD_WIN | MOD_ALT;
        snapshot->zoomModifiers = MOD_WIN;
        snapshot->monitors = {RECT{0, 0, 1920, 1080}, RECT{1920, 0, 3840, 1080}};
        snapshot->locked = {false, false};
        snapshot->shortcuts = ShortcutMap::defaults();
        return snapshot;
    }

    static MouseEvent mouse(WPARAM message, int x, int y, DWORD time = 0, int wheel = 0, DWORD flags = 0)
    {
        MouseEvent event;
        event.message = message;
        event.position = POINT{x, y};
        event.time = time;
        event.wheelDelta = wheel;
        event.flags = flags;
        return event;
    }

    static KeyEvent key(WPARAM message, DWORD vk)
    {
        KeyEvent event;
        event.message = message;
        event.vk = vk;
        return event;
    }

    QList<Kind> kinds() const
    {
        QList<Kind> result;
        for (const InputCommand &command : m_commands) {
            result.append(command.kind);
        }
        return result;
    }

    // Like Windows: a swallowed press never counts as down, and the desktop's press is up once
    // its release was sent. Pans must go on regardless.
    void useRealButtonStates(bool *leftDown)
    {
        m_router = std::make_unique<InputRouter>(
            [this](const InputCommand &command) {
                m_commands.append(command);
            },
            [this](POINT) {
                return m_desktop;
            },
            [this, leftDown]() {
                ++m_releases;
                *leftDown = false;
            },
            [leftDown](int vk) {
                return vk == VK_LBUTTON && *leftDown;
            });
        m_router->setSnapshot(snapshot());
    }

private Q_SLOTS:
    void init()
    {
        m_commands.clear();
        m_desktop = true;
        m_releases = 0;
        m_router = std::make_unique<InputRouter>(
            [this](const InputCommand &command) {
                m_commands.append(command);
            },
            [this](POINT) {
                return m_desktop;
            },
            [this]() {
                ++m_releases;
            },
            [](int) {
                return true;
            });
        m_router->setSnapshot(snapshot());
    }

    void clickOnDesktopPasses()
    {
        QCOMPARE(m_router->mouse(mouse(WM_LBUTTONDOWN, 100, 100), 0), HookAction::Pass);
        QCOMPARE(m_router->mouse(mouse(WM_MOUSEMOVE, 101, 101), 0), HookAction::Pass);
        QCOMPARE(m_router->mouse(mouse(WM_LBUTTONUP, 101, 101), 0), HookAction::Pass);
        QVERIFY(m_commands.isEmpty());
        QCOMPARE(m_releases, 0);
    }

    void dragOnDesktopPans()
    {
        QCOMPARE(m_router->mouse(mouse(WM_LBUTTONDOWN, 100, 100, 1000), 0), HookAction::Pass);
        // The first move beyond the threshold: the desktop gets its release, the move is swallowed.
        QCOMPARE(m_router->mouse(mouse(WM_MOUSEMOVE, 110, 100, 1010), 0), HookAction::Swallow);
        QCOMPARE(m_releases, 1);
        QCOMPARE(kinds(), (QList<Kind>{Kind::PanBegin, Kind::PanDelta}));
        QCOMPARE(m_commands[1].delta, QPointF(10, 0));
        // Later moves pass (the pointer keeps moving) and report deltas.
        QCOMPARE(m_router->mouse(mouse(WM_MOUSEMOVE, 130, 90, 1020), 0), HookAction::Pass);
        QCOMPARE(m_commands.last().delta, QPointF(20, -10));
        // The release was sent already: swallow the real one.
        QCOMPARE(m_router->mouse(mouse(WM_LBUTTONUP, 130, 90, 1030), 0), HookAction::Swallow);
        QCOMPARE(m_commands.last().kind, Kind::PanEnd);
    }

    void pressOnWindowDoesNothing()
    {
        m_desktop = false;
        QCOMPARE(m_router->mouse(mouse(WM_LBUTTONDOWN, 100, 100), 0), HookAction::Pass);
        QCOMPARE(m_router->mouse(mouse(WM_MOUSEMOVE, 200, 100), 0), HookAction::Pass);
        QVERIFY(m_commands.isEmpty());
    }

    void modifierPansAnywhere()
    {
        m_desktop = false;
        QCOMPARE(m_router->mouse(mouse(WM_LBUTTONDOWN, 100, 100), MOD_WIN | MOD_ALT), HookAction::Swallow);
        QCOMPARE(m_router->mouse(mouse(WM_MOUSEMOVE, 150, 100), MOD_WIN | MOD_ALT), HookAction::Pass);
        QCOMPARE(m_router->mouse(mouse(WM_LBUTTONUP, 150, 100), MOD_WIN | MOD_ALT), HookAction::Swallow);
        QCOMPARE(kinds(), (QList<Kind>{Kind::PanBegin, Kind::PanDelta, Kind::PanEnd}));
        QCOMPARE(m_releases, 0);
    }

    void middleButtonPansOnDesktop()
    {
        QCOMPARE(m_router->mouse(mouse(WM_MBUTTONDOWN, 100, 100), 0), HookAction::Swallow);
        QCOMPARE(m_router->mouse(mouse(WM_MBUTTONUP, 100, 100), 0), HookAction::Swallow);
        QCOMPARE(kinds(), (QList<Kind>{Kind::PanBegin, Kind::PanEnd}));
    }

    void modifierPanGoesOnWhileWindowsSeesNoButton()
    {
        bool leftDown = false; // swallowed: Windows never saw it
        useRealButtonStates(&leftDown);
        QCOMPARE(m_router->mouse(mouse(WM_LBUTTONDOWN, 100, 100), MOD_WIN | MOD_ALT), HookAction::Swallow);
        for (int x = 110; x <= 150; x += 10) {
            m_router->mouse(mouse(WM_MOUSEMOVE, x, 100), MOD_WIN | MOD_ALT);
        }
        QCOMPARE(kinds().count(Kind::PanDelta), 5);
        QCOMPARE(m_router->mouse(mouse(WM_LBUTTONUP, 150, 100), MOD_WIN | MOD_ALT), HookAction::Swallow);
        QCOMPARE(m_commands.last().kind, Kind::PanEnd);
    }

    void desktopPanGoesOnAfterItsReleaseWasSent()
    {
        bool leftDown = true; // the desktop got the press
        useRealButtonStates(&leftDown);
        QCOMPARE(m_router->mouse(mouse(WM_LBUTTONDOWN, 100, 100), 0), HookAction::Pass);
        QCOMPARE(m_router->mouse(mouse(WM_MOUSEMOVE, 110, 100), 0), HookAction::Swallow);
        QCOMPARE(m_releases, 1);
        QVERIFY(!leftDown);
        for (int x = 120; x <= 160; x += 10) {
            QCOMPARE(m_router->mouse(mouse(WM_MOUSEMOVE, x, 100), 0), HookAction::Pass);
        }
        QCOMPARE(kinds().count(Kind::PanDelta), 6);
        QCOMPARE(m_router->mouse(mouse(WM_LBUTTONUP, 160, 100), 0), HookAction::Swallow);
        QCOMPARE(m_commands.last().kind, Kind::PanEnd);
    }

    void pendingPressWithoutButtonIsForgotten()
    {
        bool leftDown = true;
        useRealButtonStates(&leftDown);
        QCOMPARE(m_router->mouse(mouse(WM_LBUTTONDOWN, 100, 100), 0), HookAction::Pass);
        leftDown = false; // released, but the hook never saw it
        QCOMPARE(m_router->mouse(mouse(WM_MOUSEMOVE, 200, 100), 0), HookAction::Pass);
        QVERIFY(m_commands.isEmpty());
    }

    void lostReleaseEndsThePanWithTheNextPress()
    {
        QCOMPARE(m_router->mouse(mouse(WM_LBUTTONDOWN, 100, 100), MOD_WIN | MOD_ALT), HookAction::Swallow);
        m_router->mouse(mouse(WM_MOUSEMOVE, 120, 100), MOD_WIN | MOD_ALT);
        // The release never came; the next press ends that pan and is a press of its own.
        m_desktop = false;
        QCOMPARE(m_router->mouse(mouse(WM_LBUTTONDOWN, 300, 100), 0), HookAction::Pass);
        QCOMPARE(m_commands.last().kind, Kind::PanEnd);
        const int count = m_commands.size();
        m_router->mouse(mouse(WM_MOUSEMOVE, 400, 100), 0);
        QCOMPARE(m_router->mouse(mouse(WM_LBUTTONUP, 400, 100), 0), HookAction::Pass);
        QCOMPARE(m_commands.size(), count);
    }

    void escapeLeavesTheOverview()
    {
        QCOMPARE(m_router->key(key(WM_KEYDOWN, VK_ESCAPE), 0), HookAction::Pass);
        auto overview = snapshot();
        overview->overviewOpen = true;
        m_router->setSnapshot(overview);
        QCOMPARE(m_router->key(key(WM_KEYDOWN, VK_ESCAPE), 0), HookAction::Swallow);
        QCOMPARE(m_commands.last().action, ShortcutAction::ZoomReset);
        QCOMPARE(m_router->key(key(WM_KEYUP, VK_ESCAPE), 0), HookAction::Swallow);
    }

    void escapeClosesTheOverlayFirst()
    {
        auto open = snapshot();
        open->overviewOpen = true;
        open->overlayOpen = true;
        m_router->setSnapshot(open);
        QCOMPARE(m_router->key(key(WM_KEYDOWN, VK_ESCAPE), 0), HookAction::Swallow);
        QCOMPARE(kinds(), QList<Kind>{Kind::CancelOverlay});
        QCOMPARE(m_router->key(key(WM_KEYUP, VK_ESCAPE), 0), HookAction::Swallow);
        // With a modifier it is not ours.
        QCOMPARE(m_router->key(key(WM_KEYDOWN, VK_ESCAPE), MOD_CONTROL), HookAction::Pass);
    }

    void anOpenMenuGetsAllInput()
    {
        auto open = snapshot();
        open->overviewOpen = true;
        open->menuOpen = true;
        m_router->setSnapshot(open);
        // Esc closes the menu, not the overview; clicks and drags reach the menu too.
        QCOMPARE(m_router->key(key(WM_KEYDOWN, VK_ESCAPE), 0), HookAction::Pass);
        QCOMPARE(m_router->key(key(WM_KEYUP, VK_ESCAPE), 0), HookAction::Pass);
        QCOMPARE(m_router->mouse(mouse(WM_LBUTTONDOWN, 100, 100), MOD_WIN | MOD_ALT), HookAction::Pass);
        QCOMPARE(m_router->mouse(mouse(WM_MOUSEMOVE, 200, 100), MOD_WIN | MOD_ALT), HookAction::Pass);
        QCOMPARE(m_router->mouse(mouse(WM_LBUTTONUP, 200, 100), MOD_WIN | MOD_ALT), HookAction::Pass);
        QVERIFY(m_commands.isEmpty());
    }

    void canvasMenu()
    {
        auto menu = snapshot();
        menu->canvasMenu = true;
        m_router->setSnapshot(menu);
        // A plain right click is the desktop's own menu.
        QCOMPARE(m_router->mouse(mouse(WM_RBUTTONDOWN, 500, 400), 0), HookAction::Pass);
        QCOMPARE(m_router->mouse(mouse(WM_RBUTTONUP, 500, 400), 0), HookAction::Pass);
        QVERIFY(m_commands.isEmpty());
        // Win + right click on the desktop: ours, release included.
        QCOMPARE(m_router->mouse(mouse(WM_RBUTTONDOWN, 500, 400), MOD_WIN), HookAction::Swallow);
        QCOMPARE(m_router->mouse(mouse(WM_RBUTTONUP, 500, 400), MOD_WIN), HookAction::Swallow);
        QCOMPARE(kinds(), QList<Kind>{Kind::CanvasMenu});
        QCOMPARE(m_commands.last().point, QPointF(500, 400));
        // Other modifiers, a window under the pointer, or a locked monitor: passed on.
        QCOMPARE(m_router->mouse(mouse(WM_RBUTTONDOWN, 500, 400), MOD_WIN | MOD_SHIFT), HookAction::Pass);
        QCOMPARE(m_router->mouse(mouse(WM_RBUTTONUP, 500, 400), MOD_WIN | MOD_SHIFT), HookAction::Pass);
        m_desktop = false;
        QCOMPARE(m_router->mouse(mouse(WM_RBUTTONDOWN, 500, 400), MOD_WIN), HookAction::Pass);
        QCOMPARE(m_router->mouse(mouse(WM_RBUTTONUP, 500, 400), MOD_WIN), HookAction::Pass);
        m_desktop = true;
        menu = snapshot();
        menu->canvasMenu = true;
        menu->locked = {true, false};
        m_router->setSnapshot(menu);
        QCOMPARE(m_router->mouse(mouse(WM_RBUTTONDOWN, 500, 400), MOD_WIN), HookAction::Pass);
        QCOMPARE(m_router->mouse(mouse(WM_RBUTTONUP, 500, 400), MOD_WIN), HookAction::Pass);
        QCOMPARE(m_commands.size(), 1);
    }

    void clickWhileGlidingStops()
    {
        auto gliding = snapshot();
        gliding->gliding = true;
        m_router->setSnapshot(gliding);
        m_desktop = false;
        QCOMPARE(m_router->mouse(mouse(WM_LBUTTONDOWN, 10, 10), 0), HookAction::Swallow);
        QCOMPARE(m_router->mouse(mouse(WM_LBUTTONUP, 10, 10), 0), HookAction::Swallow);
        QCOMPARE(kinds(), QList<Kind>{Kind::StopMotion});
    }

    void wheel()
    {
        // Plain wheel on the desktop zooms, over a window it goes to the window.
        QCOMPARE(m_router->mouse(mouse(WM_MOUSEWHEEL, 10, 10, 0, 120), 0), HookAction::Swallow);
        QCOMPARE(m_commands.last().kind, Kind::Zoom);
        QCOMPARE(m_commands.last().steps, 1.0);
        m_desktop = false;
        m_commands.clear();
        QCOMPARE(m_router->mouse(mouse(WM_MOUSEWHEEL, 10, 10, 0, -120), 0), HookAction::Pass);
        QVERIFY(m_commands.isEmpty());
        // Win + wheel zooms anywhere.
        QCOMPARE(m_router->mouse(mouse(WM_MOUSEWHEEL, 10, 10, 0, -240), MOD_WIN), HookAction::Swallow);
        QCOMPARE(m_commands.last().steps, -2.0);
    }

    void wheelOnDesktopVariants()
    {
        // Shift: horizontal pan; Ctrl: vertical pan.
        QCOMPARE(m_router->mouse(mouse(WM_MOUSEWHEEL, 10, 10, 0, 120), MOD_SHIFT), HookAction::Swallow);
        QCOMPARE(m_commands.last().kind, Kind::ScrollPan);
        QCOMPARE(m_commands.last().delta, QPointF(120, 0));
        QCOMPARE(m_router->mouse(mouse(WM_MOUSEWHEEL, 10, 10, 0, -120), MOD_CONTROL), HookAction::Swallow);
        QCOMPARE(m_commands.last().delta, QPointF(0, -120));
        // Pinch: injected Ctrl + wheel zooms.
        QCOMPARE(m_router->mouse(mouse(WM_MOUSEWHEEL, 10, 10, 0, 60, LLMHF_INJECTED), MOD_CONTROL), HookAction::Swallow);
        QCOMPARE(m_commands.last().kind, Kind::Zoom);
        QCOMPARE(m_commands.last().steps, 0.5);
    }

    void touchpadPans()
    {
        m_router->setTouchpadTick(5000);
        QCOMPARE(m_router->mouse(mouse(WM_MOUSEWHEEL, 10, 10, 5020, 8), 0), HookAction::Swallow);
        QCOMPARE(m_commands.last().kind, Kind::ScrollPan);
        QCOMPARE(m_commands.last().delta, QPointF(0, 8));
        QCOMPARE(m_router->mouse(mouse(WM_MOUSEHWHEEL, 10, 10, 5030, 5), 0), HookAction::Swallow);
        QCOMPARE(m_commands.last().delta, QPointF(-5, 0));
        // Fingers lifted long ago: the wheel zooms again.
        QCOMPARE(m_router->mouse(mouse(WM_MOUSEWHEEL, 10, 10, 9000, 120), 0), HookAction::Swallow);
        QCOMPARE(m_commands.last().kind, Kind::Zoom);
    }

    void pausedAndLocked()
    {
        auto paused = snapshot();
        paused->enabled = false;
        m_router->setSnapshot(paused);
        QCOMPARE(m_router->mouse(mouse(WM_MOUSEWHEEL, 10, 10, 0, 120), 0), HookAction::Pass);
        QCOMPARE(m_router->mouse(mouse(WM_LBUTTONDOWN, 10, 10), MOD_WIN | MOD_ALT), HookAction::Pass);
        m_router->mouse(mouse(WM_LBUTTONUP, 10, 10), 0);
        // Shortcuts keep working while paused.
        QCOMPARE(m_router->key(key(WM_KEYDOWN, 'H'), MOD_WIN | MOD_ALT), HookAction::Swallow);
        m_router->key(key(WM_KEYUP, 'H'), MOD_WIN | MOD_ALT);

        auto locked = snapshot();
        locked->locked = {true, false};
        m_router->setSnapshot(locked);
        m_commands.clear();
        QCOMPARE(m_router->mouse(mouse(WM_MOUSEWHEEL, 10, 10, 0, 120), 0), HookAction::Pass);
        QCOMPARE(m_router->mouse(mouse(WM_MOUSEWHEEL, 2000, 10, 0, 120), 0), HookAction::Swallow);
        QCOMPARE(m_commands.size(), 1);
    }

    void shortcuts()
    {
        QCOMPARE(m_router->key(key(WM_SYSKEYDOWN, 'H'), MOD_WIN | MOD_ALT), HookAction::Swallow);
        QCOMPARE(m_commands.last().action, ShortcutAction::Home);
        QVERIFY(!m_commands.last().repeat);
        // Auto repeat.
        QCOMPARE(m_router->key(key(WM_SYSKEYDOWN, 'H'), MOD_WIN | MOD_ALT), HookAction::Swallow);
        QVERIFY(m_commands.last().repeat);
        // The release of a swallowed key is swallowed too, others pass.
        QCOMPARE(m_router->key(key(WM_KEYUP, 'H'), MOD_WIN | MOD_ALT), HookAction::Swallow);
        QCOMPARE(m_router->key(key(WM_KEYUP, 'H'), 0), HookAction::Pass);
        QCOMPARE(m_router->key(key(WM_KEYDOWN, 'H'), MOD_WIN), HookAction::Pass);
        QCOMPARE(m_router->key(key(WM_KEYDOWN, VK_MENU), MOD_WIN), HookAction::Pass);
        auto safe = snapshot();
        safe->shortcutsEnabled = false;
        m_router->setSnapshot(safe);
        QCOMPARE(m_router->key(key(WM_KEYDOWN, 'W'), MOD_WIN | MOD_ALT), HookAction::Pass);
    }

    void draggingAWindow()
    {
        auto moving = snapshot();
        moving->userMove = true;
        moving->userMoveMonitor = 0;
        moving->flingThreshold = 1600;
        m_router->setSnapshot(moving);
        // Slowly towards the right monitor: held back at the edge, which scrolls the canvas.
        QCOMPARE(m_router->mouse(mouse(WM_MOUSEMOVE, 1900, 500, 1000), 0), HookAction::Pass);
        QCOMPARE(m_commands.last().kind, Kind::EdgeChanged);
        QCOMPARE(m_commands.last().delta, QPointF(1, 0));
        QCOMPARE(m_router->mouse(mouse(WM_MOUSEMOVE, 1925, 500, 1100), 0), HookAction::Swallow);
        // Fast: thrown onto the other monitor.
        m_commands.clear();
        QCOMPARE(m_router->mouse(mouse(WM_MOUSEMOVE, 1980, 500, 1110), 0), HookAction::Pass);
        QVERIFY(kinds().contains(Kind::MonitorCrossed));
        // Releasing ends edge scrolling.
        m_router->mouse(mouse(WM_LBUTTONUP, 1980, 500, 1120), 0);
    }
};

QTEST_GUILESS_MAIN(TestInputRouter)

#include "test_inputrouter.moc"
