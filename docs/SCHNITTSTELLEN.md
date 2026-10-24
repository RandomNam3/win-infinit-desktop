# WIN Infinit Desktop – Windows-Schnittstellen

Welche Schnittstellen Windows 11 (25H2 und 26H2) für eine unendliche Arbeitsfläche bietet, was sie können
und wo die Grenzen liegen. Grundlage ist eine Recherche im Oktober 2026 (Microsoft Learn über die
Quelltexte in `MicrosoftDocs/win32` und `MicrosoftDocs/sdk-api`, Windows-Blogs, Projekte mit ähnlichem
Ziel). Was sich nur auf echter Hardware klären lässt, prüft das Werkzeug `wid-probe`
([unten](#offene-fragen-und-wid-probe)).

Kennzeichnung:
- **[D]** dokumentiert und unterstützt
- **[U]** undokumentiert oder nur von Dritten beschrieben, kann sich mit jedem Windows-Build ändern
- **[A]** Annahme, muss auf echter Hardware bestätigt werden

---

## Das Grundproblem

Unter KDE ist Infinit Desktop ein Plugin im Compositor (KWin). Es darf Fenster an beliebiger Stelle und
in beliebiger Größe **zeichnen** und Eingaben abfangen, **bevor** KWin sie verteilt. Unter Windows ist der
Compositor (DWM) geschlossen: Es gibt keine Plugin-Schnittstelle. Ein normales Programm kann nur

1. fremde Fenster **verschieben** und in der Größe ändern (`SetWindowPos`),
2. **Live-Vorschauen** fremder Fenster in eigene Fenster einblenden (DWM-Thumbnails, Windows.Graphics.Capture),
3. Maus und Tastatur **systemweit abfangen** (Low-Level-Hooks) und
4. **Ereignisse** über fremde Fenster empfangen (WinEvents).

Daraus folgt das Modell der Windows-Version (Details in [ARCHITEKTUR.md](ARCHITEKTUR.md)):
- **Bei 100 %** werden Fenster wirklich verschoben, wie unter KDE. Alles ist normal bedienbar.
- **Herausgezoomt** zeigt ein eigenes Fenster pro Bildschirm (die *Übersicht*) verkleinerte
  Live-Vorschauen. Die echten Fenster bleiben dahinter liegen.

## Versionen: Windows 11 25H2 und 26H2

| Version | Build | Erschienen | Plattform |
|---|---|---|---|
| 24H2 | 26100 | Okt. 2024 | „Germanium“, gemeinsame Basis |
| **25H2** | 26200 | 30. Sep. 2025 | Freischaltpaket auf 24H2 |
| **26H2** | 26300 | 29. Sep. 2026 | Freischaltpaket (KB5121794) auf 24H2/25H2 |
| 26H1 | 28000 | Feb. 2026 | eigene Plattform „Bromine“, nur vorinstalliert auf neuen ARM64-Geräten |

- 24H2, 25H2 und 26H2 bekommen dieselben monatlichen Updates (gleiche Revisionsnummer). Für das
  Programm sind sie eine Plattform. x64 und ARM64 werden beide unterstützt.
- 24H2 Home/Pro bekommt seit dem 13. Okt. 2026 keine Updates mehr.
- **Regel im Programm** (`src/platform/osversion.cpp`): Build < 26100 → Start verweigert, 26200 und
  26300 → unterstützt, alles andere ab 26100 (24H2, 26H1, neuere) → läuft, mit Hinweis „nicht getestet“.
  Die Version kommt von `RtlGetVersion`, die Anzeige („25H2“) aus `DisplayVersion` in der Registry.

Relevante Änderungen in 25H2/26H2:
- **Neues Startmenü** (seit KB5067036, Okt. 2025): scrollbar, Kategorien. Eine Schnittstelle, um es zu
  positionieren, gibt es weiterhin nicht.
- **Administratorschutz** (Just-in-time-Erhöhung): standardmäßig aus, in Microsofts Sicherheits-Baselines
  seit Okt. 2026 an. Bei eingeschaltetem Schutz laufen erhöhte Aufgaben der Aufgabenplanung bei der
  Anmeldung nicht mehr interaktiv [A].
- **Smart App Control** blockiert unsignierte Programme. Seit KB5083769 (Apr. 2026) lässt es sich ohne
  Neuinstallation aus- und wieder einschalten. Siehe [Verteilung](#verteilung-und-vertrauen).
- **Bildschirmaufnahme-Datenschutz**: Einstellungen → Datenschutz → „Screenshots und Bildschirmaufnahmen“
  steuert Windows.Graphics.Capture. DWM-Thumbnails fallen nicht darunter [A].

Quellen: [Release Preview 26H2](https://blogs.windows.com/windows-insider/2026/08/27/releasing-windows-11-version-26h2-to-the-release-preview-channel/),
[What's new 26H2](https://learn.microsoft.com/en-us/windows/whats-new/whats-new-windows-11-version-26h2),
[IT-Pro-Leitfaden 26H2](https://techcommunity.microsoft.com/blog/windows-itpro-blog/an-it-pro%E2%80%99s-guide-to-windows-11-version-26h2/4559760),
[Administratorschutz](https://learn.microsoft.com/en-us/windows/security/application-security/application-control/administrator-protection).

---

## 1. Fenster finden und verfolgen

| Zweck | Schnittstelle | Status |
|---|---|---|
| Alle Fenster in Stapelreihenfolge | `EnumWindows` | [D] |
| Neue, verschwundene, gezeigte, versteckte Fenster, Aktivierung, Minimieren, Verschieben durch den Benutzer, Positionsänderung, Titel, Verbergen durch DWM | `SetWinEventHook` mit `WINEVENT_OUTOFCONTEXT`: `EVENT_OBJECT_CREATE/DESTROY/SHOW/HIDE`, `EVENT_SYSTEM_FOREGROUND`, `EVENT_SYSTEM_MINIMIZESTART/END`, `EVENT_SYSTEM_MOVESIZESTART/END`, `EVENT_OBJECT_LOCATIONCHANGE`, `EVENT_OBJECT_NAMECHANGE`, `EVENT_OBJECT_CLOAKED/UNCLOAKED` | [D] |
| Sichtbarer Rahmen (ohne unsichtbare Ränder) | `DwmGetWindowAttribute(DWMWA_EXTENDED_FRAME_BOUNDS)` | [D] |
| Auf anderem virtuellen Desktop / von der App verborgen | `DWMWA_CLOAKED` (`DWM_CLOAKED_SHELL` = anderer Desktop) | [D] |
| Programm-Identität | `QueryFullProcessImageNameW`, AppUserModelID per `SHGetPropertyStoreForWindow` | [D] |
| Erhöhtes Programm? | `OpenProcessToken` + `TokenElevation` | [D] |

- Die Ereignisse kommen asynchron. Beim Empfang kann das Fenster schon weg sein, deshalb wird jedes HWND
  vor Gebrauch mit `IsWindow` geprüft (`src/platform/windowevents.h`).
- Nur so wenige Ereignisbereiche wie nötig abonnieren: Jedes Ereignis kostet systemweit einen
  Kontextwechsel in den Dienst.
- **Welche Fenster auf die Fläche gehören** (`classify()` in `src/platform/toplevel.cpp`), angelehnt an
  die Alt+Tab-Regeln:
  - sichtbar, nicht verborgen, nicht leer;
  - ohne Besitzer: Ein Fenster mit Besitzer (Dialog, Palette) folgt seinem Hauptfenster;
  - nicht `WS_EX_TOOLWINDOW` (außer mit `WS_EX_APPWINDOW`);
  - nicht oben fixiert und gleichzeitig nicht aktivierbar (Overlays von Spielen, Bildschirmanzeigen);
  - nie die Shell: `Progman`/`WorkerW` (Desktop), `Shell_TrayWnd`/`Shell_SecondaryTrayWnd`
    (Taskleisten), Start, Suche, Benachrichtigungen, Task View, Snap-Layouts.

  Raymond Chen beschreibt die Alt+Tab-Logik:
  [Which windows appear in the Alt+Tab list?](https://devblogs.microsoft.com/oldnewthing/20071008-00/?p=24863)
- `RegisterShellHookWindow` liefert ähnliche Ereignisse, ist laut Microsoft aber „nicht für den
  allgemeinen Gebrauch“ gedacht. Es wird nicht verwendet.

## 2. Fenster verschieben und parken

| Zweck | Schnittstelle | Status |
|---|---|---|
| Verschieben ohne zu aktivieren, ohne Stapeländerung | `SetWindowPos(…, SWP_NOSIZE \| SWP_NOZORDER \| SWP_NOACTIVATE)` | [D] |
| Ohne auf ein hängendes Programm zu warten | `SWP_ASYNCWINDOWPOS` (Auftrag geht an den Thread des Fensters) | [D] |
| Viele Fenster auf einmal | `BeginDeferWindowPos`/`DeferWindowPos` – wartet aber nacheinander auf jeden Thread | [D] |
| Immer im Vordergrund (Anpinnen) | `SetWindowPos(HWND_TOPMOST)` | [D] |

- **Grenzen:** `SetWindowPos` nimmt 32-Bit-Werte. `WM_MOVE` packt die Position aber in 16 Bit, und
  (−32000, −32000) ist der Platz minimierter Fenster. Geparkt wird deshalb innerhalb von ±30000
  ([WM_MOVE](https://learn.microsoft.com/windows/win32/winmsg/wm-move),
  [Raymond Chen zu −32000](https://devblogs.microsoft.com/oldnewthing/20040707-00/?p=38523)).
- **Unsichtbare Ränder:** Fenster haben in Windows 10/11 einige Pixel breite, unsichtbare Ränder zum
  Ziehen. Die Fläche rechnet mit dem sichtbaren Rahmen (`moveFrameTo()` gleicht den Versatz aus).
- **Erhöhte Programme** (Administrator) lassen sich von einem normalen Prozess nicht verschieben (UIPI,
  `ERROR_ACCESS_DENIED`) [A, `wid-probe --test-move`]. Abhilfe ist entweder ein erhöhter Dienst oder
  `uiAccess` (siehe [Abschnitt 9](#9-uiaccess-erhöhte-fenster-und-touchscreen)).
- **Windows holt Fenster zurück**, wenn Monitore wechseln („Fensterpositionen anhand der Monitorverbindung
  merken“). Die Fläche wendet nach `WM_DISPLAYCHANGE` ihr Layout neu an, wie unter KDE nach
  `checkWorkspacePosition`.
- **Schlafen geparkter Fenster:** Chromium-basierte Programme (Chrome, Edge, Electron, Teams, Discord, …)
  hören auf zu zeichnen, sobald ihr Fenster ganz verdeckt ist oder außerhalb des virtuellen Bildschirms
  liegt. Windows senkt außerdem die Priorität (QoS) solcher Fenster. Für geparkte Fenster ist das
  erwünscht und spart Strom, wie unter KDE
  ([Chromium: Native Window Occlusion](https://chromium.googlesource.com/chromium/src/+/main/docs/windows_native_window_occlusion_tracking.md),
  [Windows QoS](https://learn.microsoft.com/windows/win32/procthread/quality-of-service)).
- **Snap:** Windows maximiert oder kachelt ein Fenster, das an den oberen bzw. seitlichen Rand gezogen
  wird. Das stört das Kantenscrollen. Windows lässt die Funktion per `SystemParametersInfo`
  (`SPI_SETWINARRANGING`) abschalten [D]. Wie unter KDE wird sie nur abgeschaltet, solange ein Fenster
  der Fläche gezogen wird, und danach wiederhergestellt.

## 3. Herauszoomen: verkleinerte Live-Ansichten

| Technik | Was sie kann | Status |
|---|---|---|
| **DWM-Thumbnails** (`DwmRegisterThumbnail`, `DwmUpdateThumbnailProperties`) | Live-Abbild eines fremden Fensters, beliebig skaliert, in einem eigenen Fenster. DWM setzt es zusammen, kein Kopieren von Pixeln. | [D] |
| DComp-Thumbnail-Visual (`DwmpCreateSharedThumbnailVisual`, dwmapi Ordinal 147) | wie oben, aber als DirectComposition-Visual: alle Fenster in einem Commit, eigene Ebenen darüber | [U] |
| Windows.Graphics.Capture | Fensterinhalt als D3D-Textur; gelber Rahmen nur mit Zustimmung abschaltbar; kostet pro Fenster eine Kopie pro Bild (ca. 16 MB je 1080p-Fenster) | [D] |
| Bildschirmlupe (`MagSetFullscreenTransform`) | nur Vergrößern: „Werte unter 1,0 sind ungültig“ | [D] – ungeeignet |

**Entscheidung:** DWM-Thumbnails. Sie sind dokumentiert, brauchen keine Zustimmung und kosten am
wenigsten. Pro Bild wird nur ihr Zielrechteck aktualisiert, und nur wenn es sich geändert hat.

Was die Thumbnails nicht können:
- **Sie liegen immer über dem Inhalt des eigenen Fensters.** Markierungen, Punktraster und
  Gruppenrahmen *über* den Fenstern brauchen deshalb ein zweites, durchklickbares Fenster darüber
  (geschichtetes Fenster mit Alphakanal: Klicks gehen durch transparente Pixel).
- **Die Stapelreihenfolge ist die Reihenfolge der Registrierung.** Um sie zu ändern, muss man neu
  registrieren
  ([CanvasWindowComposer](https://github.com/schnepg1/CanvasWindowComposer), docs/canvas-performance.md).
- **Popups** (Menüs) sind eigene Fenster und brauchen eigene Thumbnails.
- **Geparkte Fenster:** Gemessen mit `wid-probe --test-thumbnail` auf dem CI-Runner mit Windows Server
  2025 (Build 26100, dieselbe Plattform wie 25H2/26H2):

  | Zustand der Quelle | Thumbnail |
  |---|---|
  | auf dem Bildschirm | live |
  | verdeckt von einem anderen Fenster | live |
  | verborgen (`DWMWA_CLOAK`) | live |
  | geparkt bei x = 29000 | **eingefroren** |
  | fast ganz außerhalb (40 px sichtbar) | **eingefroren** |
  | minimiert | **eingefroren** |

  Fenster außerhalb des Bildes zeigen in der Übersicht also ihr letztes Bild. Live bleiben würden sie
  nur verborgen statt geparkt. Fremde Fenster lassen sich aber nur über eine undokumentierte
  Schnittstelle verbergen (komorebi: `IApplicationView::SetCloak`). Auf echter Windows-11-Hardware ist
  das noch zu bestätigen. Auf dem ARM64-Runner (Windows 11 25H2) liefert der Test nichts, weil die
  Sitzung dort nicht zusammengesetzt wird. Auch DirectComposition hört bei
  Fenstern außerhalb des Bildschirms auf zusammenzusetzen
  ([CreateSurfaceFromHwnd](https://learn.microsoft.com/windows/win32/api/dcomp/nf-dcomp-idcompositiondevice-createsurfacefromhwnd)).
- **Die Übersicht darf Chromium-Fenster nicht verdecken**, sonst hören sie auf zu zeichnen und ihre
  Vorschau friert ein. `WS_EX_TOOLWINDOW`-Fenster zählt Chromium nicht als verdeckend. Die Übersicht ist
  deshalb ein Werkzeugfenster.

## 4. Herausgezoomt arbeiten

Unter KDE leitet das Plugin Klicks, Mausrad und Tastatur pixelgenau an das verkleinerte Fenster weiter,
und die Programme zeichnen für die kleinere Größe neu. **Unter Windows geht beides nicht vollständig:**

| Ansatz | Ergebnis |
|---|---|
| Fenster eines anderen Prozesses mit anderer DPI zeichnen lassen | **nicht möglich**: `SetThreadDpiAwarenessContext` wirkt nur im eigenen Thread; ein gefälschtes `WM_DPICHANGED` bringt das Programm durcheinander [D] |
| Bildschirmlupe mit Eingabe-Umrechnung (`MagSetInputTransform`) | **nicht möglich**: nur Vergrößern, braucht `uiAccess` [D] |
| Mausnachrichten mit umgerechneten Koordinaten posten (`PostMessage WM_LBUTTONDOWN …`) | geht für klassische Win32-Programme; bricht bei Spielen, Raw-Input, Drag & Drop, nativen Menüs, Programmen, die den echten Zeiger lesen, und bei allem, was `WM_POINTER` nutzt (UWP, WinUI, XAML) [D/U] |
| Echtes Fenster kurz unter den Zeiger schieben und `SendInput` | stimmt nur im Moment des Klicks; beim Ziehen läuft die Position auseinander, weil das Fenster in voller Größe reagiert [A] |
| UI Automation (Invoke, Scroll, Text) | für einzelne Aktionen, nicht als allgemeine Eingabe [A] |

**Entscheidung für die Windows-Version:**
- Titelleiste ziehen, Ränder ziehen (Größe ändern) und Doppelklick (zu 100 %) übernimmt die Übersicht
  selbst. Danach folgt `SetWindowPos`. Das verhält sich wie unter KDE.
- Die **Tastatur** geht an das aktive Fenster. Ein Klick auf ein Fenster aktiviert es, Tippen funktioniert
  also auch herausgezoomt.
- **Klick in den Fensterinhalt:** Die Kamera „taucht ein“, also 100 % mit diesem Fenster in der Mitte,
  und der Klick kommt an. Das ist die verlässliche Variante.
- **Mausrad** über einem Fenster zoomt (unter KDE scrollt es im Programm).
- Text wird nur verkleinert, nicht neu gezeichnet.
- Weitergabe per `PostMessage` an klassische Win32-Programme ist als spätere Option mit einer Liste
  erlaubter Programme vorgesehen.

Vergleichbare Projekte bieten keine vollständige Bedienung im verkleinerten Zustand: Spatial Canvas
tauscht bei 100 % das echte Fenster ein, CanvasWindowComposer postet Mausnachrichten, und The Infinity
zoomt beim Klick auf 100 % ([Übersicht der Projekte](#vergleichbare-projekte)).

## 5. Maus, Mausrad, Touchpad, Touchscreen

| Zweck | Schnittstelle | Status |
|---|---|---|
| Maus und Tastatur systemweit sehen **und verschlucken** | `SetWindowsHookEx(WH_MOUSE_LL / WH_KEYBOARD_LL)` | [D] |
| Nur beobachten, auch im Hintergrund | Raw Input mit `RIDEV_INPUTSINK` | [D] |
| Finger auf dem Präzisions-Touchpad | Raw Input der HID-Sammlung 0x0D/0x05, Kontaktzahl (Usage 0x54) per `HidP_GetUsageValue` | [D] |
| Touchscreen systemweit umleiten | `RegisterPointerInputTarget` – braucht `uiAccess` | [D] |

- **Low-Level-Hooks** müssen innerhalb von `LowLevelHooksTimeout` (höchstens 1 s) zurückkehren.
  Windows entfernt einen zu langsamen Hook **ohne Rückmeldung**. Die Hooks laufen deshalb in einem eigenen
  Thread mit hoher Priorität, entscheiden sofort und werden regelmäßig neu installiert
  (`src/platform/inputhook.h`).
  ([LowLevelMouseProc](https://learn.microsoft.com/windows/win32/winmsg/lowlevelmouseproc))
- **Mausrad oder Zwei-Finger-Scrollen?** Windows macht aus Zwei-Finger-Scrollen dieselbe Nachricht wie
  aus dem Mausrad (`WM_MOUSEWHEEL`). Es gibt dafür kein dokumentiertes Merkmal.
  - Ansatz: Das Touchpad wird zusätzlich per Raw Input beobachtet. Ein Mausrad-Ereignis zählt als
    Touchpad, wenn in den 50 ms davor zwei oder mehr Finger auflagen.
  - Zusätzlich: Ein klassisches Mausrad liefert Vielfache von 120. Hochauflösende Mäuse liefern aber
    auch feinere Werte.
  - Ob das zuverlässig trennt, ist offen [A, `wid-probe --watch-input`].
- **Pinch** kommt bei Programmen ohne eigene Gestenverarbeitung als Strg + Mausrad an [U].
- **Drei- und Vier-Finger-Wischen** sind Systemgesten. Sie lassen sich **nicht abfangen**, nur umstellen
  (`HKCU\Software\Microsoft\Windows\CurrentVersion\PrecisionTouchPad`, `ThreeFingerSlideEnabled` u. a.
  [U]; `SPI_SETTOUCHPADPARAMETERS` kennt seit 24H2 nur Schwenken, Zoomen, Tippen und Scrollrichtung
  [D]). Die Windows-Version kann Drei-Finger-Pannen deshalb nur anbieten, wenn man die Windows-Geste
  abschaltet. Dann liest sie die Finger selbst per Raw Input.
  ([TOUCHPAD_PARAMETERS](https://learn.microsoft.com/windows/win32/api/winuser/ns-winuser-touchpad_parameters))
- **Touchscreen:** Einen Finger auf der freien Fläche abzufangen geht nur mit `uiAccess`.
  Randwischgesten lassen sich gar nicht abfangen.

## 6. Tastatur und Windows-Taste

- **Tastenkürzel** laufen über den Low-Level-Tastatur-Hook, nicht über `RegisterHotKey`. Laut Microsoft
  sind Kürzel mit der Windows-Taste für das System reserviert, und `RegisterHotKey` kennt weder Mausrad
  noch Maustasten. Der Hook sieht alles außer Strg+Alt+Entf und Win+L.
  ([RegisterHotKey](https://learn.microsoft.com/windows/win32/api/winuser/nf-winuser-registerhotkey))
- **Startmenü:** Wird Win allein gedrückt und losgelassen, öffnet sich Start. Bei Win + Mausrad (Zoom),
  Win + Ziehen und jedem verschluckten Kürzel würde Start beim Loslassen aufgehen, genau wie unter
  Plasma. Abhilfe ist eine **Maskentaste**: Sobald etwas bei gedrückter Win- oder Alt-Taste verschluckt
  wird, sendet der Hook einmal die nicht belegte Taste VK 0xE8. Die Shell sieht dann eine
  Tastenkombination, und Start bleibt zu. Ebenso öffnet sich die Menüleiste nicht beim Loslassen von
  Alt. AutoHotkey macht es genauso.
  ([A_MenuMaskKey](https://www.autohotkey.com/docs/v2/lib/A_MenuMaskKey.htm),
  [Virtual-Key Codes: 0xE8 „Unassigned“](https://learn.microsoft.com/windows/win32/inputdev/virtual-key-codes))
  Prüfen: `wid-probe --test-winkey`.
- **Standardbelegung:** Statt Meta + Alt wie unter KDE gilt Win + Alt. Kollisionen mit Windows-Kürzeln
  (Win + Alt + D, K, R, G, B, Druck, Pfeil hoch/runter) sind in der Kürzeltabelle markiert.

## 7. Bildschirme

| Zweck | Schnittstelle | Status |
|---|---|---|
| Bildschirme, Arbeitsbereich ohne Taskleisten | `EnumDisplayMonitors`, `GetMonitorInfoW` (`rcWork`) | [D] |
| Wiedererkennen über Hersteller und Modell (EDID) | `QueryDisplayConfig` + `DisplayConfigGetDeviceInfo(DISPLAYCONFIG_TARGET_DEVICE_NAME)`: `edidManufactureId`, `edidProductCodeId`, `monitorDevicePath`, `connectorInstance` | [D] |
| Skalierung pro Bildschirm | Per-Monitor-DPI v2, `GetDpiForMonitor` | [D] |
| Änderungen | `WM_DISPLAYCHANGE` | [D] |

- DisplayConfig liefert **keine Seriennummer**. Zwei gleiche Monitore unterscheidet der Gerätepfad. Die
  Seriennummer steht im EDID-Block (SetupAPI) bzw. in `WmiMonitorID`, wird aber vorerst nicht gelesen.
- Das Programm läuft mit **Per-Monitor-DPI v2** und rechnet überall in physischen Pixeln. Wechselt ein
  Fenster auf einen Bildschirm mit anderer Skalierung, ändert es selbst seine Größe (`WM_DPICHANGED`).
  Das muss die Fläche übernehmen statt dagegen zu arbeiten.

## 8. Vollbild, Virtuelle Desktops, Taskleiste, Startmenü

- **Vollbild** (`src/platform/fullscreen.cpp`):
  - `SHQueryUserNotificationState` meldet „Vollbild-App läuft“ (`QUNS_BUSY`) und exklusives D3D-Vollbild [D].
  - Der randlose Fenstermodus von Spielen hat keinen eigenen Zustand. Er wird wie unter KDE erkannt: Der
    sichtbare Rahmen bedeckt genau den Bildschirm, und das Fenster ist nicht maximiert.
- **Virtuelle Desktops:**
  - Dokumentiert ist nur `IVirtualDesktopManager`: auf welchem Desktop ein Fenster liegt und ob das der
    aktuelle ist [D].
  - Desktops aufzählen, wechseln und benachrichtigt werden geht nur über undokumentierte Schnittstellen.
    Deren IDs haben sich zuletzt mit 24H2 (26100.2605) geändert [U]
    ([VirtualDesktopAccessor](https://github.com/Ciantic/VirtualDesktopAccessor)).
  - Die Windows-Version nutzt nur die dokumentierte Schnittstelle. Einen Desktopwechsel erkennt sie an den
    `EVENT_OBJECT_CLOAKED/UNCLOAKED`-Schüben mit `DWM_CLOAKED_SHELL`.
- **Taskleiste:** Den Arbeitsbereich pro Bildschirm liefert `rcWork`, auch mit Leisten anderer Programme.
  Ist die Taskleiste automatisch ausgeblendet, ist `rcWork` der ganze Bildschirm. Zweite Taskleisten
  heißen `Shell_SecondaryTrayWnd`.
- **Startmenü an der Mausposition: nicht möglich.** Windows bietet keine Schnittstelle, um Start zu
  positionieren.
  - Ersatz ist ein eigener Starter: `FOLDERID_AppsFolder` per `IShellItem` aufzählen und per
    `IApplicationActivationManager` bzw. `shell:AppsFolder\<AUMID>` starten [D].
  - Der Starter folgt in einer späteren Version.

## 9. uiAccess, erhöhte Fenster und Touchscreen

`uiAccess="true"` im Manifest erlaubt UIPI zu umgehen, also erhöhte Fenster zu bedienen, und gibt Zugang
zu `RegisterPointerInputTarget` (Touchscreen) und besonderen Z-Ebenen. Bedingungen:
- eine Authenticode-Signatur, der der Rechner vertraut;
- Installation unter `Program Files`;
- Start per `ShellExecute`.

Microsoft sieht das nur für Hilfstechnologien vor. Unter dem Administratorschutz wurden 2026 mehrere
Umgehungen über uiAccess gefunden und geschlossen
([Project Zero](https://projectzero.google/2026/02/windows-administrator-protection.html)).

**Entscheidung:** Version 0.1 läuft **ohne** uiAccess und ohne Erhöhung. Fenster erhöhter Programme
bleiben, wo sie sind (sie verhalten sich wie angepinnt), und die App weist darauf hin. Touchscreen-Pannen
folgt mit einer signierten Installation.

## 10. Einstellungs-App, Notizen und Dienst

| KDE | Windows |
|---|---|
| KConfig (`kwinrc`) | INI-Datei unter `%APPDATA%\WIN Infinit Desktop\settings.ini`, gleiche Schlüssel wie `infinitdesktop.kcfg` |
| D-Bus `org.kde.InfinitDesktop` | Named Pipe (`QLocalServer`/`QLocalSocket`), nur für den eigenen Benutzer (`UserAccessOption`), JSON-Zeilen mit denselben Methoden |
| KGlobalAccel | eigene Kürzel-Tabelle in den Einstellungen, ausgewertet vom Tastatur-Hook |
| KI18n (`.po`-Kataloge) | dieselben `.po`-Dateien, gelesen von einer kleinen Ersatzschicht mit den Funktionen `i18n()`, `i18nc()`, `i18np()` |
| `.desktop`-Dateien (Programmliste) | Startmenü-Programme aus `FOLDERID_AppsFolder` |
| Autostart über KWin | `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` |
| `~/.local/state/kde-infinit-desktop` | `%LOCALAPPDATA%\WIN Infinit Desktop` |

KDE Frameworks laufen grundsätzlich auch unter Windows (KDE Craft), KGlobalAccel aber nicht. Die
Build-Kette ist schwer und langsam. Deshalb gibt es die schmale Ersatzschicht statt Craft.

## Verteilung und Vertrauen

- **SmartScreen** warnt bei unsignierten Programmen und auch bei signierten ohne „Ruf“. Seit 2024 gibt es
  auch für EV-Zertifikate keinen Sofort-Ruf mehr
  ([SmartScreen reputation](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/smartscreen-reputation)).
- **Smart App Control** blockiert unsignierte Programme ganz. Jede `.exe`/`.dll` muss signiert sein, mit
  RSA (ECC unterstützt SAC nicht)
  ([Code signing for SAC](https://learn.microsoft.com/en-us/windows/apps/develop/smart-app-control/code-signing-for-smart-app-control)).
- **Signieren für Open Source:**
  - Die [SignPath Foundation](https://signpath.org/terms) signiert kostenlos für OSI-lizenzierte Projekte
    (GPL-3.0 passt). Die Builds müssen dafür auf GitHub Actions laufen.
  - Microsofts Artifact Signing (früher Trusted Signing) kostet etwa 10 $/Monat. Einzelpersonen sind
    nur in den USA und Kanada zugelassen.
- **Paket:** Zunächst ein ZIP mit Installationsskript für den eigenen Benutzer (kein Administrator nötig).
  Später folgen MSI (WiX) unter `Program Files` (Voraussetzung für uiAccess) und ein winget-Eintrag.
- **Bis zur Signatur** blockiert Smart App Control das Programm. Wer es ausprobieren will, muss Smart App
  Control ausschalten. Das steht so in der Installationsanleitung.

## Offene Fragen und wid-probe

`wid-probe.exe` (`tools/wid-probe`, ohne Qt, eine einzige Datei) beantwortet die Punkte, die nur echte
Hardware klärt. Es läuft auch in der CI auf Windows Server 2025 (x64) und Windows 11 25H2 (ARM64).

| Befehl | Frage |
|---|---|
| `wid-probe` / `--json` | Version, Bildschirme mit EDID-Identität, Snap- und Touchpad-Einstellungen, alle Fenster mit ihrer Einordnung (Fläche, Dialog, Shell, …), erhöht?, Vollbild?, virtueller Desktop |
| `--test-thumbnail` | Bleibt ein DWM-Thumbnail live, wenn das Fenster geparkt (x = 29000), fast ganz außerhalb, verborgen (cloak), minimiert oder verdeckt ist? |
| `--test-move self\|HWND` | Werden Positionen bis ±29000 angenommen? Was passiert bei 40000? Was kostet ein Verschieben (synchron/asynchron)? Mit dem HWND eines erhöhten Fensters (z. B. Task-Manager): Schlägt das Verschieben fehl? |
| `--watch-input 15` | Lassen sich Mausrad und Zwei-Finger-Scrollen über Raw-Input-Kontakte trennen? Kommt Pinch als Strg + Mausrad? |
| `--test-winkey 20` | Bleibt Start bei Win + Mausrad mit der Maskentaste zu, und geht es bei Win allein weiter auf? |
| `--watch-events 30` | Kommen alle Fenster-Ereignisse an, auch beim Wechsel des virtuellen Desktops? |

Fenstertitel stehen nur mit `--titles` im Bericht, Pfade im Benutzerprofil werden zu `%USERPROFILE%`.

## Vergleichbare Projekte

| Projekt | Darstellung | Eingabe |
|---|---|---|
| [Spatial Canvas](https://github.com/13auth/spatial-canvas) | Windows.Graphics.Capture pro Fenster, D3D11 | echtes Fenster erst bei 100 % |
| [CanvasWindowComposer](https://github.com/schnepg1/CanvasWindowComposer) | DWM-Thumbnails von Hintergrund, Fenstern, Taskleiste | Mausnachrichten per `PostMessage` |
| [The Infinity](https://github.com/lokeswara-s/the-infinity) | DWM-Thumbnails; minimiert ferne Fenster | Klick zoomt auf 100 % |
| [Deskspace](https://github.com/djekanovic/Deskspace) | DWM-Thumbnails in einer Übersicht | Doppelklick fokussiert |
| [WinfiniteCanvas](https://github.com/Nick-de-Bruin/WinfiniteCanvas) | verschiebt Fenster wirklich | – |
| [LeopardWM](https://github.com/jcardama/LeopardWM) | Parken, Verbergen, Animation mit `DwmFlush` | – |
| [PaperWM-Win](https://github.com/Valijuu/PaperWM-Win), [komorebi](https://github.com/LGUG2Z/komorebi), [GlazeWM](https://github.com/glzr-io/glazewm) | Kachel-Fenstermanager | – |
| [VirtualDesktopAccessor](https://github.com/Ciantic/VirtualDesktopAccessor) | undokumentierte Desktop-Schnittstellen je Build | – |
