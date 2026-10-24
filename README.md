# WIN Infinit Desktop

**Eine unendliche Arbeitsfläche für Windows 11 – auf jedem Bildschirm.**

WIN Infinit Desktop ist die Windows-Version von [KDE Infinit Desktop](https://github.com/RandomNam3/kde-infinit-desktop).
Jeder Monitor wird zu einer in alle Richtungen unendlichen Fläche, auf der Fenster in Originalgröße liegen.
Du verschiebst die Fläche mit Maus, Touchpad oder Tastatur, zoomst heraus, um alles zu überblicken, und
wieder hinein, um weiterzuarbeiten. Taskleiste und Hintergrundbild bleiben dabei stehen.

> **Stand:** erste Version des Dienstes, noch kein Release. Zielsysteme sind Windows 11 **25H2** und **26H2**
> (x64 und ARM64).

## Wie es unter Windows funktioniert

Unter KDE ist Infinit Desktop ein Plugin im Compositor. Windows hat dafür keine Schnittstelle. Die
Windows-Version ist deshalb ein Hintergrundprogramm:
- Es **verschiebt Fenster wirklich**, sobald du die Fläche bewegst.
- Herausgezoomt zeigt es eine **Übersicht** mit verkleinerten Live-Vorschauen (DWM-Thumbnails).
- Eingaben fängt es über **Low-Level-Hooks** ab.

Welche Windows-Schnittstellen das sind, was sie können und wo die Grenzen liegen, steht in
[docs/SCHNITTSTELLEN.md](docs/SCHNITTSTELLEN.md). Was gleich bleibt und was sich unterscheidet, zeigt
[docs/FAHRPLAN.md](docs/FAHRPLAN.md).

## Bestandteile

| Verzeichnis | Inhalt |
|---|---|
| `src/core/` | Kamera, Animationen, Nachgleiten, Parkplätze, Navigation, Fensterplatz-Gedächtnis – **gemeinsam mit KDE** (siehe [UPSTREAM.md](UPSTREAM.md)) |
| `src/platform/` | Win32-Schicht: Bildschirme (mit EDID), Fenster und ihre Einordnung, Fenster-Ereignisse, Eingabe-Hooks mit Maskentaste, Touchpad-Kontakte, virtuelle Desktops, Vollbild, Verschieben |
| `src/service/` | der Dienst `win-infinit-desktop.exe`: Fläche, Übersicht, Markierungen, Eingabe, Symbol im Infobereich, Steuerung per Named Pipe |
| `tools/wid-ctl/` | Kommandozeile für den Dienst (`wid-ctl state`, `wid-ctl pan dx=-500 dy=0`, …) |
| `tools/wid-probe/` | Diagnose-Werkzeug: prüft auf deinem Rechner, was die Schnittstellen tun |
| `tests/` | Unit-Tests des Kerns und des Dienstes, Laufzeittest (`tests/runtime/smoke.ps1`) |

## Bauen

Voraussetzungen: Visual Studio 2022 oder 2026 (C++), CMake ≥ 3.22, Ninja, Qt ≥ 6.8 (MSVC).

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\msvc2022_64
cmake --build build
ctest --test-dir build
```

Nur das Diagnose-Werkzeug (ohne Qt): `-DWID_WITH_QT=OFF`.

## Ausprobieren

Noch gibt es kein Release. Jeder CI-Lauf hat ein Artefakt `win-infinit-desktop-x64` mit dem Dienst,
`wid-ctl` und `wid-probe`:

1. Entpacken und `win-infinit-desktop.exe` starten. Das Symbol erscheint im Infobereich.
2. Auf freiem Desktop ziehen oder mit dem Mausrad zoomen. Win + Alt + Ziehen verschiebt die Fläche
   auch über Fenstern.
3. Beenden über das Symbol. Alle Fenster kommen dabei auf den Bildschirm zurück.

Die Programme sind noch nicht signiert: Smart App Control blockiert sie, SmartScreen warnt (siehe
[SCHNITTSTELLEN.md](docs/SCHNITTSTELLEN.md#verteilung-und-vertrauen)). Was unter Windows anders ist als
unter KDE, steht in [FAHRPLAN.md](docs/FAHRPLAN.md), der technische Aufbau in
[ARCHITEKTUR.md](docs/ARCHITEKTUR.md).

## Diagnose: wid-probe

```powershell
wid-probe > bericht.txt              # System, Bildschirme, alle Fenster
wid-probe --test-thumbnail           # bleiben Vorschauen geparkter Fenster live?
wid-probe --watch-input 15           # Mausrad und Touchpad unterscheiden
wid-probe --test-winkey 20           # Win + Mausrad, ohne dass sich Start öffnet
```

Fenstertitel stehen nur mit `--titles` im Bericht. Die fertigen `wid-probe.exe` (x64 und ARM64) liegen
als Artefakte an jedem CI-Lauf.

## Lizenz

GPL-3.0-or-later, wie KDE Infinit Desktop.
