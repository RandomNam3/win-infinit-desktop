# WIN Infinit Desktop – Technische Dokumentation

Aufbau und Designentscheidungen der Windows-Version. Welche Windows-Schnittstellen es gibt und warum
diese gewählt wurden, steht in [SCHNITTSTELLEN.md](SCHNITTSTELLEN.md). Hier geht es um die Umsetzung.
Zielplattform ist Windows 11 25H2/26H2 (Build 26200/26300), x64 und ARM64.

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
│                                      │  IpcServer (Named Pipe, JSON-Zeilen)              │   │
│                                      │  CrashGuard, Einstellungen (INI, beobachtet)      │   │
│                                      └──────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────────────────────────────────────┘
        ▲ Named Pipe win-infinit-desktop-<Sitzung>                ▲ %APPDATA%\WIN Infinit Desktop\settings.ini
        │                                                          │
   wid-ctl.exe, später die Einstellungs-App                   Einstellungs-App, Texteditor
```

| Verzeichnis | Inhalt | Abhängigkeiten |
|---|---|---|
| `src/core/` | Kamera, Animationen, Geschwindigkeit, Parkplätze, Navigation, Fensterplatz-Gedächtnis, Gruppen, Bereiche, Notizen, Karte – **unverändert aus KDE** | Qt Core |
| `src/platform/` | Win32-Schicht (Bildschirme, Fenster, Ereignisse, Hooks, Touchpad, Desktops, Vollbild, Verschieben) | Windows SDK |
| `src/service/` | der Dienst `win-infinit-desktop.exe` | Qt Gui, Qt Network, core, platform |
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
| Esc, Alt+F4 | zurück auf 100 % |

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
    Beenden.

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
| `%LOCALAPPDATA%\WIN Infinit Desktop\crashguard` | Zähler des Absturzschutzes |
| `%LOCALAPPDATA%\WIN Infinit Desktop\service.log` | Protokoll (1 MB, eine alte Generation) |

- Einstellungen wirken sofort: Der Dienst beobachtet die Datei und das Verzeichnis, Editoren ersetzen
  die Datei beim Speichern. Zusätzlich gibt es den Befehl `reloadSettings`.
- Debug-Ausgaben kommen mit `DebugLogging=true` oder der Umgebungsvariablen `WID_DEBUG=1`.

## 10. Stabilität und Ressourcen

**In Ruhe läuft nichts:**
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

Ohne `monitor` gilt der Bildschirm unter dem Mauszeiger. Navigationsbefehle wirken sofort, ohne
Animation, wie die D-Bus-Methoden der KDE-Version. Beispiel:
`wid-ctl setCamera x=0 y=0 zoom=0.5 monitor=0`.

## 12. Tests

- **Unit-Tests** (`ctest`):
  - der gemeinsame Kern (wie unter KDE);
  - Kürzel-Parser und -Tabelle, Einstellungen aus der INI-Datei;
  - die Entscheidungen des `InputRouter`: Klick, Ziehen, Modifikator, mittlere Taste, Anhalten beim
    Gleiten, Mausrad-Varianten, Touchpad, Pause und Sperre, Kürzel, Fenster ziehen mit Kante und Wurf;
  - Markierungen, IPC-Zeilen, Absturzschutz.
- **AddressSanitizer:** Ein eigener CI-Job baut alles mit MSVC `/fsanitize=address` und führt die
  Unit-Tests aus.
- **Laufzeittest** (`tests/runtime/smoke.ps1`, in der CI auf Windows Server 2025):
  1. ein Testfenster öffnen, den Dienst starten;
  2. prüfen, dass das Fenster übernommen wird;
  3. per IPC um −200 px verschieben: Das Fenster bewegt sich genau so weit;
  4. um −6000 px: Das Fenster ist geparkt und auf keinem Bildschirm mehr;
  5. den Dienst beenden: Das Fenster ist wieder auf einem Bildschirm.

  Der Test hängt von der Desktop-Sitzung des Runners ab. Er wird berichtet, ist aber nicht
  Voraussetzung.
- **Echte Hardware:** `wid-probe` (siehe [SCHNITTSTELLEN.md](SCHNITTSTELLEN.md#offene-fragen-und-wid-probe)).
