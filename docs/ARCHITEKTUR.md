# WIN Infinit Desktop – Technische Dokumentation

Aufbau und Designentscheidungen der Windows-Version. Welche Windows-Schnittstellen sie nutzt und warum,
steht in [Abschnitt 15](#15-windows-schnittstellen), woher der gemeinsame Kern kommt, in
[Abschnitt 16](#16-gemeinsamer-kern-mit-kde-infinit-desktop). Zielplattform ist Windows 11 25H2/26H2
(Build 26200/26300) auf x64; die Win32-Schicht und `wid-probe` bauen auch für ARM64.

---

## 1. Überblick

```
┌──────────────────────────── win-infinit-desktop.exe (ein Prozess) ──────────────────────────┐
│                                                                                              │
│  Hook-Thread                         Haupt-Thread (Qt-Ereignisschleife)                      │
│  ┌──────────────────────┐  Befehle   ┌──────────────────────────────────────────────────┐   │
│  │ InputHook            │──────────▶ │ CanvasService                                    │   │
│  │  WH_MOUSE_LL         │ (Warte-    │  Bildschirme, Canvases, verwaltete Fenster,      │   │
│  │  WH_KEYBOARD_LL      │  schlange) │  Kamera, Parken, Fensterplatz-Gedächtnis         │   │
│  │ InputRouter          │ ◀───────── │  WindowEvents (SetWinEventHook)                  │   │
│  │  entscheidet sofort  │  Snapshot  │  Overview  (je Bildschirm, DWM-Thumbnails)       │   │
│  └──────────────────────┘  (atomar)  │  Markers   (kleine geschichtete Fenster)          │   │
│                                      │  Tray      (Shell_NotifyIcon)                     │   │
│                                      │  Organizer (Notizen, Gruppen, Bereiche, Karte)    │   │
│                                      │  IpcServer (Named Pipe, JSON-Zeilen)              │   │
│                                      │  CrashGuard, Einstellungen (INI, beobachtet)      │   │
│                                      └──────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────────────────────────────────────┘
        ▲ Named Pipe win-infinit-desktop-<Sitzung>                ▲ %APPDATA%\WIN Infinit Desktop\settings.ini
        │                                                          │
   wid-ctl.exe, wid-settings.exe, wid-notes.exe               wid-settings.exe, Texteditor
```

| Verzeichnis | Inhalt | Abhängigkeiten |
|---|---|---|
| `src/core/` | Kamera, Animationen, Geschwindigkeit, Parkplätze, Navigation, Fensterplatz-Gedächtnis, Gruppen, Bereiche, Notizen, Karte – **unverändert aus KDE** (siehe [Abschnitt 16](#16-gemeinsamer-kern-mit-kde-infinit-desktop)) | Qt Core |
| `src/platform/` | Win32-Schicht (Bildschirme, Fenster, Ereignisse, Hooks, Touchpad, Desktops, Vollbild, Verschieben) | Windows SDK |
| `src/service/` | der Dienst `win-infinit-desktop.exe` | Qt Gui, Qt Network, core, platform |
| `src/shared/` | Namen der Pipes von Dienst und Notiz-App (`pipenames.h`, nur Header); für die Apps: Pipe-Client und -Server, Übersetzungen aus dem `.po`-Katalog, Pfade | Qt Core, Qt Network, Qt Qml |
| `src/notes/` | die Notiz-App `wid-notes.exe` | Qt Quick, shared |
| `src/settings/` | die Einstellungs-App `wid-settings.exe` mit dem Schema `win-infinit-desktop.kcfg` | Qt Quick, shared |
| `po/de/` | deutscher Katalog beider Apps (`win-infinit-desktop.po`) | – |
| `tools/i18n/` | `check_catalog.py`: fehlende und unbenutzte Übersetzungen | Python 3 |
| `tools/wid-ctl/` | Kommandozeilen-Client der IPC-Schnittstelle | Qt Network |
| `tools/wid-probe/` | Diagnose der Windows-Schnittstellen auf dem Rechner | Windows SDK |
| `tools/wid-testwindow/` | Testfenster für den Laufzeittest | Windows SDK |

Der Dienst nutzt Qt Gui ohne Widgets und ohne QML. Zeichnen (Markierungen) und Text kommen von
`QPainter`, alle Fenster sind eigene Win32-Fenster.

## 2. Threads

- **Haupt-Thread:** Qts Ereignisschleife verteilt alle Nachrichten des Threads. Darüber kommen:
  - die WinEvents (out of context, also im eigenen Thread),
  - die Nachrichten der eigenen Fenster (Dienstfenster, Übersicht, Markierungen, Tray),
  - die Named-Pipe-Verbindungen und die Timer.
- **Hook-Thread:** `InputHook` installiert die Low-Level-Hooks in einem eigenen Thread mit hoher
  Priorität. Der Thread hat eine eigene Nachrichtenschleife.
  - `InputRouter` entscheidet dort in Mikrosekunden, ob ein Ereignis durchgeht oder verschluckt wird.
    Er liest dafür nur einen unveränderlichen `InputSnapshot`, ein atomarer `shared_ptr`, den der
    Haupt-Thread bei jeder Änderung neu veröffentlicht.
  - Was zu tun ist, legt er als `InputCommand` in eine Warteschlange. Den Haupt-Thread weckt einmal
    `PostMessage`. Aufeinanderfolgende Verschiebungen werden dabei zusammengefasst.
  - Der Hook-Thread ruft keine Fensterfunktionen anderer Prozesse auf. Einzige Ausnahme ist
    `WindowFromPoint` bei Tastendruck und Mausrad, um die freie Desktop-Fläche zu erkennen.
- Windows entfernt einen Hook ohne Rückmeldung, wenn er zu lange braucht. Deshalb werden die Hooks
  alle 5 Minuten neu installiert.

## 3. Datenmodell

**Canvas:** eine pro Bildschirm, Schlüssel ist die Bildschirm-Identität (`EDID:DEL:41A3`). Haben zwei
gleiche Monitore dieselbe Identität, kommt der Gerätepfad dazu.

```
Kamera = (center, zoom)      center: Punkt der Fläche in der Bildschirmmitte
Bildschirm(p) = Bildschirm.mitte + (p − center) · zoom
```

Koordinaten sind physische Pixel, denn der Dienst ist Per-Monitor-DPI-v2. Fensterpositionen auf der
Fläche gelten für den sichtbaren Rahmen (`DWMWA_EXTENDED_FRAME_BOUNDS`), ohne die unsichtbaren
Ränder.

Jede Canvas hat zwei Kameras:
- `camera` ist das, was gezeigt wird.
- `committed` ist die Kamera, zu der die physischen Fensterpositionen passen.

Bei 100 % sind beide gleich. In der Übersicht bleiben die Fenster stehen, und nur ihre Vorschauen folgen
`camera`.

**ManagedWindow:** ein Eintrag pro HWND. Er wird immer neu nachgeschlagen, Zeiger auf Einträge werden
nie aufbewahrt. Er enthält:
- Flächenposition, Größe und Canvas;
- Zustand: angepinnt, Vollbild, minimiert, wird gezogen, versteckt oder auf anderem Desktop, nicht
  verschiebbar (erhöht), geparkt;
- die letzten 8 eigenen Verschiebe-Aufträge, um eigene von fremden Bewegungen zu unterscheiden.

| Zustand | Verhalten |
|---|---|
| auf der Fläche | bewegt sich mit der Kamera, wird geparkt, wenn es nicht im Bild ist |
| angepinnt | bleibt auf dem Bildschirm; „immer im Vordergrund“ über `HWND_TOPMOST` |
| Vollbild | Rahmen = Bildschirm und nicht maximiert (auch randloses Vollbild); bleibt stehen, sperrt den Bildschirm, solange es den Fokus hat |
| wird gezogen | zwischen `EVENT_SYSTEM_MOVESIZESTART` und `…END` nicht angefasst |
| versteckt / anderer Desktop | wird nicht bewegt, behält seine Flächenposition und kommt beim Wiedererscheinen dorthin zurück |
| nicht verschiebbar | erhöhte Programme (UIPI) und Fenster, die sich dem Parken widersetzen |

Aufgenommen werden nur Fenster mit `WindowClass::Canvas` (siehe `classify()`), keine aus dem eigenen
Prozess und keine aus `ExcludedWindowClasses`. Die Liste vergleicht Programmname, Fensterklasse und
App-Schlüssel.

## 4. Bei 100 %: echte Fensterpositionen

- Jede Kameraänderung markiert die Canvas nur als geändert. `flush()` wendet die Änderungen höchstens
  alle 16 ms an, während Animationen direkt im Animationsschritt.
- Liegt ein Fenster ganz außerhalb seines Bildschirms, wird es **geparkt**: an die Stelle, die
  `computeParkingSpot` aus dem gemeinsamen Kern liefert. Sie liegt außerhalb aller Bildschirme, aber
  nahe dem eigenen, innerhalb von ±30000. Geparkte Chromium-Fenster hören auf zu zeichnen und sparen
  so Strom.
- Verschoben wird mit `SetWindowPos(…, SWP_ASYNCWINDOWPOS | SWP_NOACTIVATE | SWP_NOZORDER)`. Ein
  hängendes Programm hält den Dienst so nicht auf.
- **Eigene oder fremde Bewegung?** `EVENT_OBJECT_LOCATIONCHANGE` meldet auch die eigenen,
  asynchronen Bewegungen, oft verspätet.
  - Eine Position, die in den letzten 1,5 s angefordert wurde (±1 px), zählt als eigene.
  - Alles andere hat das Programm oder Windows bewegt (Win + Pfeiltasten, ein anderer Bildschirm). Die
    neue Position wird als Flächenposition übernommen.
- **Geparkte Fenster,** die jemand anderes bewegt, werden wieder geparkt, wie unter KDE. Widersetzt sich
  ein Fenster mehr als dreimal in 3 s, bleibt es, wo es ist, und gilt als nicht verschiebbar.
- Nach `WM_DISPLAYCHANGE` oder einem geänderten Arbeitsbereich wartet der Dienst 400 ms. Dann liest er
  die Bildschirme neu und wendet das Layout an. Fenster eines verschwundenen Bildschirms gehören
  danach zu dem, auf dem sie jetzt liegen.

## 5. Übersicht (Zoom ≠ 100 %)

- Sobald der Zoom oder das Ziel einer Animation nicht 100 % ist, öffnet sich auf diesem Bildschirm die
  Übersicht. Sie ist ein Werkzeugfenster über dem Arbeitsbereich, die Taskleiste bleibt sichtbar.
- Chromium zählt Werkzeugfenster nicht als verdeckend. Die Programme dahinter zeichnen deshalb weiter,
  und ihre Vorschauen bleiben live.
- Aufbau von unten nach oben:
  1. dunkler Hintergrund mit Punktraster (GDI);
  2. das Hintergrundbild, als Thumbnail von `Progman` mit geringer Deckkraft;
  3. die DWM-Thumbnails der Fenster in Stapelreihenfolge.
- Thumbnails stapeln in der Reihenfolge, in der sie registriert werden. Ändert sich die
  Stapelreihenfolge, werden sie neu registriert. Sonst gehen nur geänderte Rechtecke an DWM.
  Ausgeblendet hält die Übersicht keine Thumbnails.
- Die echten Fenster bleiben in der Übersicht stehen. Erst beim Zurück auf 100 % wird festgeschrieben,
  also physisch verschoben.

| Aktion in der Übersicht | Wirkung |
|---|---|
| Titelleiste ziehen (oberer Streifen, 32 px × Zoom) | Fenster auf der Fläche verschieben |
| Rand oder Ecke ziehen (6 px) | Größe ändern, sofort auch physisch |
| Klick in den Fensterinhalt, Doppelklick | eintauchen: 100 % mit dem Fenster in der Mitte, danach wird es aktiviert |
| Ziehen auf freier Fläche oder im Inhalt, mittlere Taste | Fläche verschieben, mit Nachgleiten |
| Mausrad | zoomen zum Mauszeiger |
| Rechtsklick auf freie Fläche | Menü: 100 %, alle Fenster zeigen, Einstellungen, Beenden |
| Esc (über den Tastatur-Hook) | zurück auf 100 % |

Die Übersicht wird nie aktiviert (`WS_EX_NOACTIVATE`, `MA_NOACTIVATE`). Die Tastatur bleibt beim aktiven
Programm, wie unter KDE: Tippen geht auch herausgezoomt an das aktive Fenster.

## 6. Eingabe

| Ereignis | Verhalten |
|---|---|
| Linke Taste auf freiem Desktop | geht durch; ab 3 px Bewegung wird ein Pan daraus. Der Desktop bekommt sofort ein (markiertes) Loslassen, damit er kein Auswahlrechteck zeichnet. Der Zeiger bewegt sich normal weiter. |
| Pan-Modifikator (Standard Win + Alt) + linke Taste | Pan überall, Drücken und Loslassen werden verschluckt |
| Mittlere Taste auf freiem Desktop | Pan |
| Klick, während die Fläche gleitet | hält sie nur an |
| Zoom-Modifikator (Standard Win) + Mausrad | Zoom überall |
| Mausrad auf freiem Desktop | Zoom |
| Zwei Finger auf dem Touchpad, freier Desktop | Pan. Touchpad heißt: In den 50 ms davor meldete Raw Input mindestens zwei Kontakte. |
| Pinch (injiziertes Strg + Mausrad) auf freiem Desktop | Zoom |
| Umschalt / Strg + Mausrad auf freiem Desktop | waagerecht / senkrecht verschieben |
| Tastenkürzel (Tabelle wie KDE, Win statt Meta) | werden mit ihrem Loslassen verschluckt; Wiederholung nur bei Pfeilen und Zoom |

- **Windows-Taste:** Wird bei gedrückter Win- oder Alt-Taste etwas verschluckt, sendet der Hook einmal
  VK 0xE8. Startmenü und Menüleisten bleiben dann zu.
- **Fenster ziehen:** Startet der Benutzer ein Verschieben (`EVENT_SYSTEM_MOVESIZESTART`, Zeiger im
  oberen Bereich des Rahmens), gilt Folgendes:
  - Der Router hält den Zeiger an der Grenze zum Nachbarbildschirm fest, solange die Hand langsamer ist
    als `FlingThreshold`.
  - Die Geschwindigkeit der Hand misst der `VelocityTracker`, auch während die Bewegungen verschluckt
    werden.
  - Am Rand scrollt die Fläche mit `EdgePanSpeed`. Das gezogene Fenster bleibt unter dem Zeiger, es
    wandert also auf der Fläche.
  - Windows-Snap ist nur für die Dauer dieses Ziehens aus, ohne Eintrag im Benutzerprofil
    (`SystemParametersInfo` ohne `SPIF_UPDATEINIFILE`). Danach wird es wiederhergestellt, auch beim
    Beenden. Stürzt der Dienst während des Ziehens ab, merkt er das an der Datei `snap-off` im
    Zustandsverzeichnis und schaltet Snap beim nächsten Start wieder ein.

## 7. Markierungen

Für jedes Fenster außerhalb des Bildes zeigt eine kleine Markierung die Richtung:
- Die Position berechnet `layoutMarkers()` mit denselben Regeln wie unter KDE.
- Jede Markierung ist ein eigenes, 64 × 64 px großes, geschichtetes Fenster. Es ist oben fixiert, nie
  aktiv und nicht in Bildschirmaufnahmen.
- So wird pro Bild nur ein kleines Bild gezeichnet und positioniert. Ein Fenster über den ganzen
  Bildschirm hätte bei jeder Bewegung Megabytes gekostet.
- Transparente Pixel lassen die Maus durch. Ein Klick fliegt zum Fenster.
- Programmsymbole werden pro Fenster einmal geholt: `WM_GETICON` mit 50 ms Zeitlimit, dann das
  Klassensymbol, dann das Symbol der Programmdatei.

## 8. Fensterplatz-Gedächtnis, Anpinnen, Vollbild

- **Gedächtnis:** `PlacementMemory` aus dem gemeinsamen Kern. Der Programmschlüssel ist die
  AppUserModelID, sonst der Programmpfad, klein geschrieben. Der Bildschirm wird über
  `OutputIdentity` wiedererkannt: Name = GDI-Name, Hersteller und Modell aus der EDID.
  - Neu ist ein Fenster, wenn es innerhalb von 5 s nach `EVENT_OBJECT_CREATE` erscheint. Es bekommt
    den jüngsten freien Platz seines Programms.
  - Liegt der Platz außerhalb des Bildes, fährt die Kamera hin.
- **Anpinnen** (Win + Alt + T) lässt das Fenster an seiner Bildschirmposition. Mit „immer im
  Vordergrund“ (Win + Alt + Umschalt + T) wird es `HWND_TOPMOST`. Beim Lösen oder Beenden nimmt der
  Dienst das wieder zurück.
- **Vollbild:** Solange ein Vollbild-Fenster den Fokus hat, sind Gesten auf diesem Bildschirm
  wirkungslos. Desktop, Taskleiste und Shell zählen nicht.

## 9. Persistenz und Konfiguration

| Datei | Inhalt |
|---|---|
| `%APPDATA%\WIN Infinit Desktop\settings.ini`, Gruppe `[Effect-infinitdesktop]` | Einstellungen: gleiche Schlüssel und Standardwerte wie `infinitdesktop.kcfg` der KDE-Version |
| dieselbe Datei, Gruppe `[Shortcuts]` | Tastenkürzel: Schlüssel = Aktions-ID der KDE-Version (`InfinitDesktop Home`), Wert z. B. `Win+Alt+H;Ctrl+Alt+H`, leer = keins |
| `%LOCALAPPDATA%\WIN Infinit Desktop\canvas.json` | Kameras und Lesezeichen je Canvas (nie gezoomt wiederhergestellt) |
| `%LOCALAPPDATA%\WIN Infinit Desktop\placements.json` | Fensterplatz-Gedächtnis |
| `%LOCALAPPDATA%\WIN Infinit Desktop\groups.json`, `areas.json`, `notes.json` | Gruppen, Bereiche und Plätze der Notizen, gleiches JSON wie unter KDE |
| `%LOCALAPPDATA%\WIN Infinit Desktop\crashguard` | Zähler des Absturzschutzes |
| `%LOCALAPPDATA%\WIN Infinit Desktop\service.log` | Protokoll (1 MB, eine alte Generation) |

- Einstellungen wirken sofort: Der Dienst beobachtet die Datei und das Verzeichnis, Editoren ersetzen
  die Datei beim Speichern. Zusätzlich gibt es den Befehl `reloadSettings`.
- Debug-Ausgaben kommen mit `DebugLogging=true` oder der Umgebungsvariablen `WID_DEBUG=1`.

## 10. Stabilität und Ressourcen

**In Ruhe läuft nichts:**
- Wach wird der Dienst nur durch Ereignisse. Dazu gehören leider auch die Zeigerbewegungen:
  `EVENT_OBJECT_LOCATIONCHANGE` meldet auch den Mauszeiger (`OBJID_CURSOR`) und lässt sich nicht nach
  Objekt filtern. Die Prüfung verwirft solche Ereignisse in Mikrosekunden; der Hook-Thread sieht jede
  Bewegung ohnehin.
- Alle Timer sind Einmal-Timer oder laufen nur während einer Bewegung: Anwenden 16 ms, Animation
  16 ms, Kantenscrollen 16 ms, Ausblenden der Markierungen 2 s, Speichern 3 s, Bildschirmwechsel
  400 ms.
- Ohne Eingabe und ohne Fensterereignisse schläft der Prozess.
- Die einzige Ausnahme ist das Neuinstallieren der Hooks alle 5 Minuten.

**Keine Lecks:**
- Jedes Win32-Objekt hat einen Besitzer, der es freigibt (`Unique<…>` in `win32util.h`): Fenster,
  Thumbnails, Symbole, GDI-Objekte, Menüs, Handles, Hooks, WinEvent-Hooks.
- Die Übersicht gibt ihre Thumbnails beim Ausblenden frei.
- Der Symbol-Cache verliert seinen Eintrag mit dem Fenster.

**Keine Zugriffe auf Freigegebenes:**
- Fenstereinträge werden nur über ihr HWND nachgeschlagen.
- Jedes HWND aus einem Ereignis wird mit `IsWindow` geprüft. Ereignisse kommen asynchron, das Fenster
  kann schon weg sein.
- Fensterprozeduren holen ihr Objekt aus `GWLP_USERDATA`, und die Destruktoren lösen es vorher.
- Callbacks, die die Übersicht oder die Markierungen umbauen können, laufen mit Kopien. Beenden geht
  über `QTimer::singleShot(0)`.
- Der Hook-Thread wird vor dem `InputRouter` beendet, den er benutzt.

**Absturzschutz:**
- Wie unter KDE zählt `CrashGuard` Starts. Nach drei Abstürzen in Folge startet der Dienst im
  abgesicherten Modus: keine Hooks, keine Fensterverwaltung. Verlassen geht über das Tray-Menü oder
  den Befehl `leaveSafeMode`.
- **Nichts bleibt geparkt:**
  - Beim Beenden, auch beim Abmelden (`WM_ENDSESSION`), holt der Dienst alle Fenster außerhalb der
    Bildschirme in den Arbeitsbereich zurück und stellt Snap und „immer im Vordergrund“ wieder her.
  - Nach einem Absturz holt der nächste Start Fenster zurück, die ganz außerhalb aller Bildschirme
    liegen.
  - Im Tray-Menü gibt es zusätzlich „Alle Fenster zurückholen“.

## 11. IPC-Schnittstelle

Named Pipe `\\.\pipe\win-infinit-desktop-<Sitzungs-ID>`, nur für den eigenen Benutzer
(`QLocalServer::UserAccessOption`), höchstens 16 Verbindungen, Zeilen bis 64 KB.

```
→ {"id": 1, "method": "pan", "params": {"dx": -200, "dy": 0}}
← {"id": 1, "result": {"x": 200, "y": 0, "zoom": 1}}
← {"event": "statusChanged", "params": {"paused": false, "safeMode": false}}
```

| Methode | Parameter | Ergebnis |
|---|---|---|
| `version` | – | Versionsnummer |
| `state` | – | Bildschirme mit Kamera und Übersicht, alle Fenster mit Zustand, Flächen- und Bildschirmrechteck |
| `paused`, `setPaused` | `paused` | Gesten pausiert? |
| `safeMode`, `leaveSafeMode` | – | abgesicherter Modus? |
| `reloadSettings` | – | `true` |
| `pan` | `dx`, `dy` (Bildschirmpixel), `monitor` | neue Kamera |
| `zoomBy` / `setZoom` | `factor` / `zoom`, `monitor` | neue Kamera (Zoom um die Bildschirmmitte) |
| `zoomToFit`, `resetZoom`, `goHome` | `monitor` | neue Kamera |
| `setCamera` | `x`, `y`, `zoom`, `monitor` | neue Kamera |
| `windowPlacements`, `forgetWindowPlacements`, `forgetApplicationPlacements` | `app` | Fensterplatz-Gedächtnis |
| `bringWindowsBack` | – | alle Fenster in den Arbeitsbereich |
| `quit` | – | beendet den Dienst (mit Zurückholen) |
| `groups`, `setGroups` | `json` (Text) | Gruppen als JSON-Text, mit `openWindows` je Gruppe |
| `areas`, `setAreas` | `json` (Text) | Bereiche als JSON-Text |
| `openGroup` | `id` oder `index` | öffnet die Gruppe (wie Win + Alt + F1 … F9) |
| `saveGroupLayout` | `id` (leer: Gruppe des aktiven Fensters) | speichert die Anordnung |
| `groupFromSelection`, `areaFromSelection`, `showMap` | – | Rechteck aufziehen bzw. Karte, auf dem Bildschirm unter dem Zeiger |
| `newNote` | `x`, `y` (optional, Bildschirmpunkt) | bittet die Notiz-App um eine neue Notiz |
| `toggleNotes` | – | Notizen ein- oder ausblenden (über die Notiz-App) |
| `expectNote` | `id`, `hwnd` (dezimal) | die Notiz-App meldet ein Notizfenster an, bevor sie es zeigt |
| `noteState` | `id` | `{id, open, pinned, keepAbove}` |
| `setNotePinned` | `id`, `pinned`, `keepAbove` | pinnt eine Notiz an oder löst sie; Ergebnis wie `noteState` |
| `forgetNote`, `retainNotes` | `id` / `ids` | vergisst Plätze gelöschter Notizen |
| `applications` | – | offene Programme (`app`, `launch`, `program`, `title`) für die Auswahl in der Einstellungs-App |

Ereignisse an alle Clients: `statusChanged`, `groupsChanged`, `areasChanged`,
`notePinnedChanged {id, pinned, keepAbove}`.

Ohne `monitor` gilt der Bildschirm unter dem Mauszeiger. Navigationsbefehle wirken sofort, ohne
Animation, wie die D-Bus-Methoden der KDE-Version. Beispiel:
`wid-ctl setCamera x=0 y=0 zoom=0.5 monitor=0`.

## 12. Tests

- **Unit-Tests** (`ctest`):
  - der gemeinsame Kern (wie unter KDE);
  - Kürzel-Parser und -Tabelle, Einstellungen aus der INI-Datei;
  - die Entscheidungen des `InputRouter`: Klick, Ziehen, Modifikator, mittlere Taste, Anhalten beim
    Gleiten, Mausrad-Varianten, Touchpad, Pause und Sperre, Kürzel, Fenster ziehen mit Kante und Wurf;
  - Markierungen, IPC-Zeilen, Absturzschutz;
  - Canvas-Menü, Startbefehle von Gruppenmitgliedern, Fenster-Handles und Notiz-IDs aus der IPC,
    Esc für Karte und Auswahl, Win + Rechtsklick.
- **AddressSanitizer:** Ein eigener CI-Job baut alles mit MSVC `/fsanitize=address` und führt die
  Unit-Tests aus.
- **Laufzeittest** (`tests/runtime/smoke.ps1`, in der CI auf Windows Server 2025):
  1. ein Testfenster öffnen, den Dienst starten;
  2. prüfen, dass das Fenster übernommen wird;
  3. per IPC um −200 px verschieben: Das Fenster bewegt sich genau so weit;
  4. um −6000 px: Das Fenster ist geparkt und auf keinem Bildschirm mehr;
  5. eine Gruppe mit dem Programm des Testfensters 1500 px weiter rechts öffnen: Das Fenster wandert
     dorthin, die Kamera folgt;
  6. das Testfenster als Notiz anmelden, per IPC anpinnen und wieder lösen;
  7. den Dienst beenden: Das Fenster ist wieder auf einem Bildschirm.

  Der Test hängt von der Desktop-Sitzung des Runners ab. Er wird berichtet, ist aber nicht
  Voraussetzung.
- **Diagnose auf einem Rechner:** `wid-probe` (siehe [Abschnitt 15](#diagnose-wid-probe)).

## 13. Notizen, Gruppen, Bereiche und Karte

Die Logik ist der gemeinsame Kern (`groups.h`, `areas.h`, `noteplacements.h`, `minimap.h`).
`Organizer` (`src/service/organizer.*`) verbindet ihn mit den Win32-Fenstern, der Notiz-App, den
Dateien und seinen Overlay-Fenstern, wie der gleichnamige Teil des KWin-Effekts.

**Lebensdauer:**
- `ManagedWindow` trägt Notiz-ID, Gruppe mit Mitglied und Bereich. Der Organizer hält keine
  Fensterzeiger. Ausnahme sind angemeldete Notizfenster: Sie werden bei `EVENT_OBJECT_DESTROY`
  vergessen und verfallen nach 10 s.
- Gestartete Gruppenmitglieder werden nur über ihre Programm-ID erwartet (30 s).
- Overlay-Zustand merkt sich den Schlüssel des Bildschirms, nicht seinen Index.
- Overlay-Fenster werden einmal angelegt und nur ausgeblendet, nie aus ihrem eigenen Handler heraus
  zerstört. Ausgeblendet halten sie kein Bild.

**Gruppen:**
- Öffnen (Win + Alt + F1 … F9, Tray, IPC):
  1. `planGroup` wählt die Fenster: erst die mit dieser Gruppe markierten, dann das einzige Fenster
     des Programms, sonst das nächste unmarkierte.
  2. Diese Fenster bekommen ihre Plätze auf dem Canvas des Gruppen-Bildschirms. Minimierte werden
     mit `ShowWindowAsync` geholt und 300 ms später noch einmal platziert, weil Windows sie an ihren
     alten Platz zurücklegt.
  3. Fehlende Programme startet `ShellExecuteEx` in einem eigenen Thread: Pfade direkt,
     AppUserModelIDs über `shell:AppsFolder\<AUMID>`. Das neue Fenster wird beim Übernehmen an
     seinen Platz gelegt.
  4. Die Kamera fährt hin; ist die Gruppe größer als der Bildschirm, zoomt sie heraus.
- Programm-ID und Startbefehl kommen vom Fenster: AppUserModelID, sonst der Programmpfad. Die ID ist
  klein geschrieben, der Startbefehl bleibt, wie er ist.
- Die Anordnung folgt wie unter KDE den Fenstern: verschobene Gruppen verschieben ihren Ursprung,
  im Modus „automatisch“ wird die neue Anordnung gespeichert.

**Bereiche:**
- Gezeichnet werden sie in der Übersicht, im Hintergrund unter den Thumbnails, und auf der Karte.
- Bei 100 % lässt Windows nicht zwischen Hintergrundbild und Fenstern zeichnen (wie beim
  Punktraster).
- Ein Fenster gehört zum Bereich, in den seine Mitte fällt. Dynamische Bereiche wachsen und
  schrumpfen mit ihren Fenstern; Größenänderungen werden je Runde der Ereignisschleife
  zusammengefasst.

**Notizen:**
- Die Notizen sind Fenster der Notiz-App `wid-notes.exe`.
- Ablauf beim Anzeigen:
  1. Die App legt das Fenster an.
  2. Sie meldet es mit `expectNote {id, hwnd}` beim Dienst an, bevor sie es zeigt.
  3. Der Dienst übernimmt genau dieses Fenster als Notiz, auch als Werkzeugfenster ohne
     Taskleisten-Eintrag. Ist es schon sichtbar, übernimmt er es sofort.
- Der Dienst speichert nur den Platz (`notes.json`); Text, Größe und Farbe speichert die App.
- Angepinnte Notizen bleiben auf dem Bildschirm, „immer im Vordergrund“ ist `HWND_TOPMOST`.
- Der Dienst ruft die App über ihre eigene Pipe (`notesServerName()`) auf, ohne je zu warten
  (`NotesClient`):
  - Aufrufe warten höchstens in einer kurzen Schlange (8).
  - Läuft die App nicht, wird sie einmal gestartet. Nach 10 s ohne Verbindung werden die Aufrufe
    verworfen.
  - Liest die App nicht mehr, trennt der Dienst ab 64 KB offenen Daten.
  - Beim Start bittet der Dienst eine laufende App, ihre Notizen neu anzumelden; gestartet wird sie
    dafür nicht.

**Overlays** (`overlays.*`): nie aktivierte, oberste Werkzeugfenster.
- **Schleier:** eine Farbe, eine Deckkraft für das ganze Fenster. DWM mischt sie, gezeichnet wird
  einmal.
- **Auswahlrechteck:** ein durchklickbares halbtransparentes Fenster, das nur verschoben wird.
- **Kartenfeld:** ein undurchsichtiges Fenster mit einem `QImage`, neu gezeichnet nur, wenn sich
  Kamera oder Fenster ändern (höchstens 60-mal pro Sekunde).
- **Meldungen:** ein kleines Fenster mit Alphakanal; verschwindet nach Ablauf oder bleibt, solange
  die Auswahl läuft.
- Esc kommt über den Tastatur-Hook, weil die Overlays nie den Fokus haben. Es schließt zuerst die
  Karte bzw. die Auswahl, sonst die Übersicht.

**Canvas-Menü:**
- Herausgezoomt: Rechtsklick auf freie Fläche der Übersicht. Bei 100 %: Win + Rechtsklick auf den
  Desktop. Der Hook verschluckt nur diese Kombination, das Desktop-Menü von Windows bleibt.
- Es ist ein natives Menü (`TrackPopupMenuEx`) des Dienstfensters. Es öffnet sich erst nach dem
  laufenden Ereignis (`QTimer::singleShot(0)`), denn es hat eine eigene Nachrichtenschleife. Danach
  wird alles neu nachgeschlagen.
- Einträge wie unter KDE, dazu „Beenden“.

## 14. Notiz-App und Einstellungs-App

Beide Apps sind die Qt-Quick-Apps von KDE Infinit Desktop (QML fast unverändert) ohne KDE
Frameworks. Was dort KDE erledigt, übernehmen kleine Teile in `src/shared` und den Apps:

| KDE | Windows |
|---|---|
| KI18n, `KLocalizedContext` | `Localizer` liest den `.po`-Katalog (als Ressource eingebaut) beim Start; `i18n()`, `i18nc()`, `i18np()`, `i18ncp()` mit derselben Bedeutung in C++ und QML (`LocalizedContext`). Platzhalter werden in einem Durchgang ersetzt. Ohne deutsche Oberflächensprache bleibt der Text englisch (`WID_LANGUAGE=de` erzwingt Deutsch). |
| D-Bus | JSON-Zeilen über Named Pipes: `IpcClient` (asynchron, Antworten an Rückrufe, die mit ihrem Kontextobjekt verfallen; Zeitlimit; begrenzte Warteschlangen) und `JsonLineServer` (Zeilen höchstens 64 KiB, höchstens 16 Verbindungen). |
| `kwinrc` mit KConfigSkeleton | `SettingsFile` schreibt `%APPDATA%\WIN Infinit Desktop\settings.ini` so, wie der Dienst sie liest (Gruppe `[Effect-infinitdesktop]`, Enums als Name, Farben als `r,g,b`, Listen mit Komma, Standardwerte fallen weg). Das Schema `win-infinit-desktop.kcfg` ist das von KDE ohne die Einträge, die unter Windows nichts bewirken, mit Texten für Windows. |
| KGlobalAccel | Kürzel in `[Shortcuts]` derselben Datei in der Schreibweise des Dienstes (`Win+Alt+H;Win+Alt+Num+`, `keycombo.h`). Ohne Eintrag gelten die Standardkürzel, ein leerer Wert hebt die Belegung auf. Die Seite „Mit Windows“ zeigt die dokumentierten Kürzel von Windows 11 als feste Tabelle und markiert Überschneidungen (etwa Win + Alt + B für HDR). |
| KService, Icon-Theme | Startmenü über den Shell-Ordner `AppsFolder` (in einem eigenen Thread, `IShellItem`), Symbole über `IShellItemImageFactory`. Programm-ID ist wie im Dienst die AppUserModelID oder der Programmpfad in Kleinbuchstaben, der Startbefehl behält die Schreibweise. |
| KStatusNotifierItem | entfällt: das Symbol im Infobereich gehört dem Dienst. |

**Notiz-App (`wid-notes.exe`):**
- Startet der Dienst bei Bedarf; läuft einmal je Sitzung (die Pipe `…-notes` gehört der ersten
  Instanz, eine zweite gibt `--new-note` weiter und endet).
- Jede Notiz ist ein randloses Werkzeugfenster (`Qt::Tool`: nicht in der Taskleiste und nicht in
  Alt + Tab). Vor dem Anzeigen meldet die App das Fenster mit `expectNote {id, hwnd}` an und wartet
  höchstens 500 ms auf die Antwort; der Dienst übernimmt genau dieses Fenster und legt es an
  seinen Platz. Ausgeblendete Notizen haben kein Fenster, alle Fenster teilen die einfache
  Render-Schleife (`QSG_RENDER_LOOP=basic`).
- Speicherung wie unter KDE: `<id>.md` und `index.json` in `%APPDATA%\WIN Infinit Desktop\notes`,
  verzögert und atomar (`QSaveFile`).
- Anfragen auf ihrer Pipe (`newNote`, `toggleNotes`, `setNotesVisible`, `reannounceNotes`, `notes`,
  `quit`) beantwortet sie sofort und erledigt die Fensterarbeit danach, damit der Dienst nie auf
  QML wartet. Ohne laufenden Dienst gehen ihre Aufrufe verloren; startet der Dienst neu, ruft er
  `reannounceNotes`.

**Einstellungs-App (`wid-settings.exe`):**
- Einmal je Sitzung; ein zweiter Start holt das Fenster nach vorn (und öffnet `--page`).
- Der Zustand („Effect“ in QML) kommt vom Dienst (`state`, Ereignis `statusChanged`); solange die
  App offen ist, verbindet sie sich alle 3 s neu, wenn der Dienst fehlt. „Einschalten“ startet den
  Dienst und trägt ihn unter `HKCU\…\Run` ein, „Ausschalten“ beendet ihn und entfernt den Eintrag.
- Gruppen und Bereiche bearbeitet sie als ganze JSON-Dokumente (`groups`/`setGroups`,
  `areas`/`setAreas`), die Zahl der Notizen fragt sie die Notiz-App, ohne sie zu starten.
- Während ein Kürzel aufgenommen wird, bittet sie den Dienst mit `blockShortcuts` um Ruhe.
- Update-Check: Releases dieses Repositorys; ein Release enthält
  `win-infinit-desktop-<Version>-<x64|arm64>.zip`. „Update installieren“ lädt das passende Paket in
  einem PowerShell-Fenster herunter und startet dessen `install.ps1 -Update`. Nur Downloads von
  GitHub werden verwendet.
- `--screenshot <Ordner>` rendert alle Seiten mit Beispieldaten, ohne Pipes und mit einer
  temporären Einstellungsdatei; die CI prüft damit, dass alle QML-Seiten fehlerfrei laden.

**Tests** (auch unter Linux, ohne die QML-Apps bei Qt < 6.5): Notizspeicher, Katalog und
`i18n`-Funktionen, Pipe-Protokoll (Antworten, Ereignisse, Zeitlimit, verfallene Rückrufe, zu lange
Zeilen), Tastenkombinationen, Einstellungsdatei, Update-Check. Unter Windows liest
`test_settingsinterop` mit dem Code des Dienstes, was die App schreibt, und vergleicht die
Standardwerte und Standardkürzel beider Seiten.

## 15. Windows-Schnittstellen

Welche Schnittstellen Windows 11 (25H2 und 26H2) für eine unendliche Arbeitsfläche bietet, was sie können
und wo die Grenzen liegen.
Grundlage ist eine Recherche im Oktober 2026 (Microsoft Learn über die Quelltexte in `MicrosoftDocs/win32` und
`MicrosoftDocs/sdk-api`, Windows-Blogs, Projekte mit ähnlichem Ziel). Kennzeichnung: **[D]** dokumentiert und
unterstützt, **[U]** undokumentiert oder nur von Dritten beschrieben, kann sich mit jedem Windows-Build ändern.

### Das Grundproblem

Unter KDE ist Infinit Desktop ein Plugin im Compositor (KWin). Es darf Fenster an beliebiger Stelle und
in beliebiger Größe **zeichnen** und Eingaben abfangen, **bevor** KWin sie verteilt. Unter Windows ist der
Compositor (DWM) geschlossen: Es gibt keine Plugin-Schnittstelle. Ein normales Programm kann nur

1. fremde Fenster **verschieben** und in der Größe ändern (`SetWindowPos`),
2. **Live-Vorschauen** fremder Fenster in eigene Fenster einblenden (DWM-Thumbnails, Windows.Graphics.Capture),
3. Maus und Tastatur **systemweit abfangen** (Low-Level-Hooks) und
4. **Ereignisse** über fremde Fenster empfangen (WinEvents).

Daraus folgt das Modell der Windows-Version:
- **Bei 100 %** werden Fenster wirklich verschoben, wie unter KDE. Alles ist normal bedienbar.
- **Herausgezoomt** zeigt ein eigenes Fenster pro Bildschirm (die *Übersicht*) verkleinerte
  Live-Vorschauen. Die echten Fenster bleiben dahinter liegen.

### Versionen: Windows 11 25H2 und 26H2

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
  seit Okt. 2026 an. Der Dienst braucht keine Erhöhung und ist davon nicht betroffen.
- **Smart App Control** blockiert unsignierte Programme. Seit KB5083769 (Apr. 2026) lässt es sich ohne
  Neuinstallation aus- und wieder einschalten. Siehe [Verteilung](#verteilung-und-vertrauen).
- **Bildschirmaufnahme-Datenschutz**: Einstellungen → Datenschutz → „Screenshots und Bildschirmaufnahmen“
  steuert Windows.Graphics.Capture, das der Dienst nicht verwendet.

Quellen: [Release Preview 26H2](https://blogs.windows.com/windows-insider/2026/08/27/releasing-windows-11-version-26h2-to-the-release-preview-channel/),
[What's new 26H2](https://learn.microsoft.com/en-us/windows/whats-new/whats-new-windows-11-version-26h2),
[IT-Pro-Leitfaden 26H2](https://techcommunity.microsoft.com/blog/windows-itpro-blog/an-it-pro%E2%80%99s-guide-to-windows-11-version-26h2/4559760),
[Administratorschutz](https://learn.microsoft.com/en-us/windows/security/application-security/application-control/administrator-protection).


### Fenster finden und verfolgen

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

### Fenster verschieben und parken

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
  `ERROR_ACCESS_DENIED`). Das ginge nur mit einem erhöhten Dienst oder mit `uiAccess` (siehe
  [uiAccess](#uiaccess-erhöhte-fenster-und-touchscreen)).
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

### Herauszoomen: verkleinerte Live-Ansichten

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
  nur verborgen statt geparkt; fremde Fenster lassen sich aber nur über eine undokumentierte
  Schnittstelle verbergen (komorebi: `IApplicationView::SetCloak`), die der Dienst nicht verwendet. Auch
  DirectComposition hört bei Fenstern außerhalb des Bildschirms auf zusammenzusetzen
  ([CreateSurfaceFromHwnd](https://learn.microsoft.com/windows/win32/api/dcomp/nf-dcomp-idcompositiondevice-createsurfacefromhwnd)).
- **Die Übersicht darf Chromium-Fenster nicht verdecken**, sonst hören sie auf zu zeichnen und ihre
  Vorschau friert ein. `WS_EX_TOOLWINDOW`-Fenster zählt Chromium nicht als verdeckend. Die Übersicht ist
  deshalb ein Werkzeugfenster.

### Herausgezoomt arbeiten

Unter KDE leitet das Plugin Klicks, Mausrad und Tastatur pixelgenau an das verkleinerte Fenster weiter,
und die Programme zeichnen für die kleinere Größe neu. **Unter Windows geht beides nicht vollständig:**

| Ansatz | Ergebnis |
|---|---|
| Fenster eines anderen Prozesses mit anderer DPI zeichnen lassen | **nicht möglich**: `SetThreadDpiAwarenessContext` wirkt nur im eigenen Thread; ein gefälschtes `WM_DPICHANGED` bringt das Programm durcheinander [D] |
| Bildschirmlupe mit Eingabe-Umrechnung (`MagSetInputTransform`) | **nicht möglich**: nur Vergrößern, braucht `uiAccess` [D] |
| Mausnachrichten mit umgerechneten Koordinaten posten (`PostMessage WM_LBUTTONDOWN …`) | geht für klassische Win32-Programme; bricht bei Spielen, Raw-Input, Drag & Drop, nativen Menüs, Programmen, die den echten Zeiger lesen, und bei allem, was `WM_POINTER` nutzt (UWP, WinUI, XAML) [D/U] |
| Echtes Fenster kurz unter den Zeiger schieben und `SendInput` | stimmt nur im Moment des Klicks; beim Ziehen läuft die Position auseinander, weil das Fenster in voller Größe reagiert |
| UI Automation (Invoke, Scroll, Text) | für einzelne Aktionen, nicht als allgemeine Eingabe |

**Entscheidung für die Windows-Version:**
- Titelleiste ziehen, Ränder ziehen (Größe ändern) und Doppelklick (zu 100 %) übernimmt die Übersicht
  selbst. Danach folgt `SetWindowPos`. Das verhält sich wie unter KDE.
- Die **Tastatur** geht an das aktive Fenster. Ein Klick auf ein Fenster aktiviert es, Tippen funktioniert
  also auch herausgezoomt.
- **Klick in den Fensterinhalt:** Die Kamera „taucht ein“, also 100 % mit diesem Fenster in der Mitte,
  und der Klick kommt an. Das ist die verlässliche Variante.
- **Mausrad** über einem Fenster zoomt (unter KDE scrollt es im Programm).
- Text wird nur verkleinert, nicht neu gezeichnet.

Vergleichbare Projekte bieten keine vollständige Bedienung im verkleinerten Zustand: Spatial Canvas
tauscht bei 100 % das echte Fenster ein, CanvasWindowComposer postet Mausnachrichten, und The Infinity
zoomt beim Klick auf 100 % ([Übersicht der Projekte](#vergleichbare-projekte)).

### Maus, Mausrad, Touchpad, Touchscreen

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
- **Pinch** kommt bei Programmen ohne eigene Gestenverarbeitung als Strg + Mausrad an [U].
- **Drei- und Vier-Finger-Wischen** sind Systemgesten. Sie lassen sich **nicht abfangen**, nur umstellen
  (`HKCU\Software\Microsoft\Windows\CurrentVersion\PrecisionTouchPad`, `ThreeFingerSlideEnabled` u. a.
  [U]; `SPI_SETTOUCHPADPARAMETERS` kennt seit 24H2 nur Schwenken, Zoomen, Tippen und Scrollrichtung
  [D]). Sie bleiben deshalb bei Windows.
  ([TOUCHPAD_PARAMETERS](https://learn.microsoft.com/windows/win32/api/winuser/ns-winuser-touchpad_parameters))
- **Touchscreen:** Einen Finger auf der freien Fläche abzufangen geht nur mit `uiAccess`, ohne den der
  Dienst läuft; Touch-Eingaben bleiben deshalb bei Windows. Randwischgesten lassen sich gar nicht
  abfangen.

### Tastatur und Windows-Taste

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

### Bildschirme

| Zweck | Schnittstelle | Status |
|---|---|---|
| Bildschirme, Arbeitsbereich ohne Taskleisten | `EnumDisplayMonitors`, `GetMonitorInfoW` (`rcWork`) | [D] |
| Wiedererkennen über Hersteller und Modell (EDID) | `QueryDisplayConfig` + `DisplayConfigGetDeviceInfo(DISPLAYCONFIG_TARGET_DEVICE_NAME)`: `edidManufactureId`, `edidProductCodeId`, `monitorDevicePath`, `connectorInstance` | [D] |
| Skalierung pro Bildschirm | Per-Monitor-DPI v2, `GetDpiForMonitor` | [D] |
| Änderungen | `WM_DISPLAYCHANGE` | [D] |

- DisplayConfig liefert **keine Seriennummer**. Zwei gleiche Monitore unterscheidet der Gerätepfad. Die
  Seriennummer steht im EDID-Block (SetupAPI) bzw. in `WmiMonitorID` und wird nicht gelesen.
- Das Programm läuft mit **Per-Monitor-DPI v2** und rechnet überall in physischen Pixeln. Wechselt ein
  Fenster auf einen Bildschirm mit anderer Skalierung, ändert es selbst seine Größe (`WM_DPICHANGED`).
  Das muss die Fläche übernehmen statt dagegen zu arbeiten.

### Vollbild, Virtuelle Desktops, Taskleiste, Startmenü

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

### uiAccess, erhöhte Fenster und Touchscreen

`uiAccess="true"` im Manifest erlaubt UIPI zu umgehen, also erhöhte Fenster zu bedienen, und gibt Zugang
zu `RegisterPointerInputTarget` (Touchscreen) und besonderen Z-Ebenen. Bedingungen:
- eine Authenticode-Signatur, der der Rechner vertraut;
- Installation unter `Program Files`;
- Start per `ShellExecute`.

Microsoft sieht das nur für Hilfstechnologien vor. Unter dem Administratorschutz wurden 2026 mehrere
Umgehungen über uiAccess gefunden und geschlossen
([Project Zero](https://projectzero.google/2026/02/windows-administrator-protection.html)).

**Entscheidung:** Der Dienst läuft **ohne** uiAccess und ohne Erhöhung. Fenster erhöhter Programme
bleiben, wo sie sind (sie verhalten sich wie angepinnt).

### Einstellungs-App, Notizen und Dienst

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

### Verteilung und Vertrauen

- **Paket:** ein ZIP mit `install.ps1`, das für den eigenen Benutzer ohne Administratorrechte installiert
  (siehe [INSTALL.md](INSTALL.md)).
- **SmartScreen** warnt bei Programmen ohne „Ruf“, auch bei signierten; seit 2024 gibt es auch für
  EV-Zertifikate keinen Sofort-Ruf mehr
  ([SmartScreen reputation](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/smartscreen-reputation)).
  Der Installer entfernt die Download-Markierung (`Unblock-File`) von den kopierten Dateien.
- **Smart App Control** blockiert nicht signierte Programme ganz. Jede `.exe`/`.dll` müsste mit RSA signiert
  sein (ECC unterstützt SAC nicht)
  ([Code signing for SAC](https://learn.microsoft.com/en-us/windows/apps/develop/smart-app-control/code-signing-for-smart-app-control)).
  Die Programme sind nicht signiert; mit eingeschaltetem Smart App Control starten sie nicht.

### Diagnose: wid-probe

`wid-probe.exe` (`tools/wid-probe`, ohne Qt, eine einzige Datei) zeigt, was diese Schnittstellen auf einem
Rechner tun. Es läuft auch in der CI auf Windows Server 2025 (x64) und Windows 11 25H2 (ARM64).

| Befehl | Prüft |
|---|---|
| `wid-probe` / `--json` | Version, Bildschirme mit EDID-Identität, Snap- und Touchpad-Einstellungen, alle Fenster mit ihrer Einordnung (Fläche, Dialog, Shell, …), erhöht?, Vollbild?, virtueller Desktop |
| `--test-thumbnail` | ob ein DWM-Thumbnail live bleibt, wenn das Fenster geparkt (x = 29000), fast ganz außerhalb, verborgen (cloak), minimiert oder verdeckt ist |
| `--test-move self\|HWND` | Positionen bis ±29000, Verhalten bei 40000, Kosten eines Verschiebens (synchron/asynchron); mit dem HWND eines erhöhten Fensters (z. B. Task-Manager) das Verhalten bei UIPI |
| `--watch-input 15` | Mausrad und Zwei-Finger-Scrollen über die Raw-Input-Kontakte, Pinch als Strg + Mausrad |
| `--test-winkey 20` | Start bei Win + Mausrad mit der Maskentaste und bei Win allein |
| `--watch-events 30` | die Fenster-Ereignisse, auch beim Wechsel des virtuellen Desktops |

Fenstertitel stehen nur mit `--titles` im Bericht, Pfade im Benutzerprofil werden zu `%USERPROFILE%`.

### Vergleichbare Projekte

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

## 16. Gemeinsamer Kern mit KDE Infinit Desktop

Die Windows-Version teilt die reine Logik mit
[KDE Infinit Desktop](https://github.com/RandomNam3/kde-infinit-desktop). Diese Dateien werden **unverändert**
übernommen. Änderungen daran gehören zuerst in das KDE-Repository und werden dann hierher kopiert, damit
beide Versionen gleich rechnen.

| Hier | Dort | Stand (Commit im KDE-Repository) |
|---|---|---|
| `src/core/` | `src/core/` | `1340a16` |
| `tests/test_{camera,motion,velocitytracker,parking,navigation,placementmemory,groups,areas}.cpp` | `tests/` | `1340a16` |

Prüfen, ob beide noch gleich sind:

```bash
diff -r src/core ../kde-infinit-desktop/src/core --exclude=CMakeLists.txt
```
