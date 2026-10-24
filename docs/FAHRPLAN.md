# WIN Infinit Desktop – Funktionsabgleich und Fahrplan

Grundfunktionen und Verhalten sollen der KDE-Version entsprechen. Diese Tabelle zeigt für jede Funktion,
wie die Windows-Version sie umsetzt. Die Begründungen stehen in [SCHNITTSTELLEN.md](SCHNITTSTELLEN.md).

**gleich**: verhält sich wie unter KDE · **angepasst**: gleiches Ziel, anderer Weg ·
**später**: geplant, nicht in 0.1 · **nicht möglich**: Windows bietet keine Schnittstelle

## Fläche und Kamera

| Funktion (KDE) | Windows | Umsetzung |
|---|---|---|
| Eigene Fläche pro Bildschirm | gleich | Kamera je Bildschirm-Identität (EDID) |
| Eigene Fläche pro virtuellem Desktop (optional) | angepasst | Desktop-ID per `IVirtualDesktopManager`, Wechsel über Cloak-Ereignisse |
| Pannen: Ziehen auf freier Fläche (links, Mitte) | gleich | Low-Level-Maus-Hook, ab 6 px; ein Klick erreicht weiter den Desktop |
| Pannen überall mit Modifikator (KDE: Meta + Alt) | gleich | **Win + Alt** + Ziehen |
| Umschalt / Strg + Mausrad auf freier Fläche | gleich | waagerecht / senkrecht |
| Nachgleiten nach einem Wisch | gleich | `VelocityTracker`/`CameraMotion` aus dem gemeinsamen Kern |
| Klick hält gleitende Fläche an | gleich | |
| Zoom: Mausrad auf freier Fläche, Win + Mausrad überall | gleich | Win-Taste mit Maskentaste, Start öffnet sich nicht |
| Rastet bei 100 % ein | gleich | |
| Pinch auf freier Fläche | angepasst | kommt als Strg + Mausrad an |
| Zwei-Finger-Scrollen = Pannen | angepasst | Touchpad-Kontakte per Raw Input (siehe offene Fragen) |
| Drei-Finger-Wischen | angepasst, später | nur wenn die Windows-Geste abgeschaltet ist |
| Touchscreen: ein Finger auf freier Fläche | später | braucht signierte Installation mit uiAccess |
| Tastenkürzel (Zoom, 100 %, Übersicht, Ursprung, Mitte, Pfeile, Lesezeichen) | gleich | Win + Alt statt Meta + Alt; Tastatur-Hook |
| Lesezeichen 1–4 | gleich | |
| Fenster an den Rand ziehen → Fläche scrollt | gleich | Bewegung am Rand im Hook anhalten, Fläche verschieben; Snap währenddessen aus |
| Schnelles Werfen auf den Nachbarbildschirm | gleich | Geschwindigkeit der Hand (Raw-Deltas) wie unter KDE |

## Fenster

| Funktion (KDE) | Windows | Umsetzung |
|---|---|---|
| Fenster außerhalb des Bildes parken | gleich | außerhalb aller Bildschirme, innerhalb ±30000 |
| Geparkte Fenster schlafen | gleich | Chromium und Windows drosseln verdeckte und ferne Fenster von selbst |
| Markierungen am Rand, Klick fliegt hin | gleich | durchklickbares Fenster mit Alphakanal über allem |
| Aktivierte Fenster ins Bild holen (Taskleiste, Alt+Tab) | gleich | `EVENT_SYSTEM_FOREGROUND` |
| Anpinnen, „immer im Vordergrund“ | gleich | nicht verschieben bzw. `HWND_TOPMOST` |
| Fenster öffnen sich, wo sie zuletzt waren | gleich | `PlacementMemory` aus dem Kern, Programm = AppUserModelID oder Pfad |
| Vollbild bleibt stehen, sperrt die Gesten | gleich | randloses Vollbild wie unter KDE erkannt |
| Minimiertes Vollbild als Kachel | später | Kachel mit Programmsymbol; live ist nicht möglich (minimierte Fenster zeichnen nicht) |
| Fenster erhöhter Programme | angepasst | bleiben stehen (wie angepinnt), Hinweis in der App |
| Startmenü an der Mausposition | nicht möglich | Start lässt sich nicht positionieren; eigener Starter **später** |

## Herausgezoomt (Übersicht)

| Funktion (KDE) | Windows | Umsetzung |
|---|---|---|
| Alle Fenster verkleinert und live | angepasst | DWM-Thumbnails in einem Werkzeugfenster pro Bildschirm |
| Hintergrund und Taskleiste bleiben stehen | gleich | die Übersicht bedeckt nur den Arbeitsbereich |
| Punktraster | angepasst | nur in der Übersicht (zwischen Hintergrundbild und Fenstern zeichnen geht unter Windows nur mit einem Trick (WorkerW), der mit Updates bricht) |
| Titelleiste ziehen verschiebt, Ränder ändern die Größe | gleich | übernimmt die Übersicht, dann `SetWindowPos` |
| Doppelklick auf die Titelleiste → 100 % | gleich | |
| In das Fenster klicken, scrollen, tippen | **angepasst** | Klick taucht ein (100 % mit diesem Fenster in der Mitte), dann kommt der Klick an; Tippen geht an das aktive Fenster; Mausrad zoomt |
| Programme zeichnen verkleinert scharf neu | nicht möglich | Windows kann einem fremden Fenster keine andere Skalierung geben |

## Neue Funktionen (beide Versionen)

| Funktion | Beschreibung | Windows |
|---|---|---|
| **Notiz-Zettel** | Per Kürzel oder Rechtsklick-Menü anlegen; Markdown; automatisch gespeichert; ein- und ausblenden; anpinnen, damit sie auch außerhalb der Kamera sichtbar bleiben | gleich: dieselbe Notiz-App (Qt Quick) wie unter KDE, die Zettel sind kleine Fenster auf der Fläche |
| **Gruppen** | Fenster gruppieren (Liste in der App oder Bereich mit der Maus aufziehen); per Kürzel öffnen oder zusammenholen, die Kamera springt hin; Anordnung wird gespeichert (umschaltbar: auf Befehl oder automatisch); ein Programm in mehreren Gruppen springt mit, solange nur ein Fenster davon offen ist | gleich, Gruppenlogik im gemeinsamen Kern |
| **Bereiche** | Die Fläche in Bereiche teilen (Rechteck aufziehen); Bereiche wachsen und schrumpfen mit den Fenstern darin | gleich, Logik im gemeinsamen Kern |
| **Karte** | Per Kürzel eine Karte der Fläche mit Fenstern, Gruppen und Bereichen; Ziehen und Klicken bewegen die Kamera | gleich: eigenes Fenster über allem |
| **Rechtsklick-Menü** | Herausgezoomt: Rechtsklick auf freie Fläche; bei 100 %: Win + Rechtsklick auf freie Fläche (das Desktop-Menü von Windows bleibt). Dazu Menüs auf Notizen und am Symbol im Infobereich | gleich |

## Einstellungs-App

| KDE | Windows |
|---|---|
| Qt-Quick-App mit Suche, Erklärungen, sofortiger Übernahme | gleiche QML-Oberfläche |
| Seite „Alle KDE-Tastenkürzel“ | Windows-Kürzel als feste Tabelle zur Kollisionsprüfung |
| Update-Check mit Patchnotes, Update startet den Installer | gleich: Releases dieses Repos, Installer in PowerShell |
| Einstellungen in `kwinrc` | `%APPDATA%\WIN Infinit Desktop\settings.ini`, gleiche Schlüssel |

## Fahrplan

1. **Grundlage** ✓: Win32-Schicht, gemeinsamer Kern, Diagnose-Werkzeug, CI für x64 und ARM64, Doku.
2. **Dienst 0.1** ✓ (`win-infinit-desktop.exe`, siehe [ARCHITEKTUR.md](ARCHITEKTUR.md)):
   - Fläche bei 100 %: Pannen (Ziehen, Win + Alt, mittlere Taste, Umschalt/Strg + Mausrad,
     Touchpad), Nachgleiten, Zoom, alle Tastenkürzel der KDE-Tabelle, Lesezeichen;
   - Parken, Markierungen, Anpinnen (auch immer im Vordergrund), Fensterplatz-Gedächtnis,
     Vollbild-Sperre, Kantenscrollen und Wurf auf den Nachbarbildschirm;
   - Übersicht mit Thumbnails (verschieben, Größe ändern, eintauchen);
   - Symbol im Infobereich, Absturzschutz, Named-Pipe-Schnittstelle (`wid-ctl`).

   Noch nicht im Dienst:
   - Kachel für minimierte Vollbild-Fenster;
   - eigene Fläche pro virtuellem Desktop (Fenster anderer Desktops behalten ihren Platz, die Kamera
     ist für alle gleich);
   - Drei-Finger-Pannen und Touchscreen.
3. **Einstellungs-App** auf Windows, Notiz-App, Gruppen, Bereiche, Karte.
4. **Installation:** ZIP mit Installationsskript, Release-Workflow (Tag und Release für jede Version),
   Update-Check. Bis dahin liegt ein ZIP mit Dienst, `wid-ctl` und `wid-probe` als Artefakt an jedem
   CI-Lauf.
5. **Später:** Signatur (SignPath), MSI unter Program Files mit uiAccess (erhöhte Fenster,
   Touchscreen), eigener Starter, Drei-Finger-Pannen, Weitergabe von Klicks an klassische Programme.

## Offene Fragen für echte Hardware

Mit `wid-probe` zu klären (siehe [SCHNITTSTELLEN.md](SCHNITTSTELLEN.md#offene-fragen-und-wid-probe)):

1. Bleiben DWM-Thumbnails geparkter Fenster live? Davon hängt ab, ob Fenster außerhalb des Bildes in der
   Übersicht live oder als letztes Bild erscheinen.
2. Trennt die Touchpad-Kontaktzahl zuverlässig Zwei-Finger-Scrollen vom Mausrad?
3. Bleibt Start mit der Maskentaste bei Win + Mausrad zu?
4. Lassen sich Fenster erhöhter Programme verschieben (erwartet: nein)?
