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
| `src/core/` | Kamera, Animationen, Geschwindigkeit, Parkplätze, Navigation, Fensterplatz-Gedächtnis, Gruppen, Bereiche, Notizen, Karte – **unverändert aus KDE** | Qt Core |
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
- **Echte Hardware:** `wid-probe` (siehe [SCHNITTSTELLEN.md](SCHNITTSTELLEN.md#offene-fragen-und-wid-probe)).

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
