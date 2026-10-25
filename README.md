# WIN Infinit Desktop

**Eine unendliche Arbeitsfläche für Windows 11 – auf jedem Bildschirm.**

WIN Infinit Desktop ist die Windows-Version von [KDE Infinit Desktop](https://github.com/RandomNam3/kde-infinit-desktop).
Jeder Monitor wird zu einer in alle Richtungen unendlichen Fläche, auf der Fenster in Originalgröße liegen.
Du verschiebst die Fläche mit Maus, Touchpad oder Tastatur, zoomst heraus, um alles zu überblicken, und
wieder hinein, um weiterzuarbeiten. Taskleiste und Hintergrundbild bleiben dabei stehen.

> **Stand:** Version 0.1.0 – Dienst, Notizen, Gruppen, Bereiche, Karte, Einstellungs-App und Installer.
> Zielsysteme sind Windows 11 **25H2** und **26H2** (x64; ARM64 folgt).

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
| `src/notes/` | die Notiz-App `wid-notes.exe`: Notiz-Zettel mit Markdown auf der Fläche |
| `src/settings/` | die Einstellungs-App `wid-settings.exe`, dieselbe Oberfläche wie unter KDE |
| `src/shared/`, `po/de/` | Pipe-Protokoll und Übersetzungen der Apps, deutscher Katalog |
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

## Installieren

Aus dem [neuesten Release](https://github.com/RandomNam3/win-infinit-desktop/releases/latest)
`win-infinit-desktop-<Version>-x64.zip` herunterladen, entpacken und im entpackten Ordner ausführen:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\install.ps1
```

- Installiert wird für den angemeldeten Benutzer, ohne Administratorrechte, nach
  `%LOCALAPPDATA%\Programs\WIN Infinit Desktop`, mit Einträgen im Startmenü und im Autostart.
- Updates kommen über die Einstellungs-App (Update-Check) oder mit `install.ps1 -Update` aus einem neuen ZIP.
- Entfernen über Einstellungen › Apps oder mit `install.ps1 -Uninstall` (Einstellungen und Notizen
  bleiben; `-RemoveData` löscht sie mit).

Danach:
1. Das Symbol erscheint im Infobereich; sein Menü hat Notizen, Karte, Gruppen und die Einstellungen.
2. Auf freiem Desktop ziehen oder mit dem Mausrad zoomen. Win + Alt + Ziehen verschiebt die Fläche
   auch über Fenstern.
3. Beim Beenden über das Symbol kommen alle Fenster auf den Bildschirm zurück.

Jeder CI-Lauf hat außerdem ein Artefakt `win-infinit-desktop-x64` mit denselben Programmen.

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
