# Gemeinsamer Code mit KDE Infinit Desktop

Die Windows-Version teilt die reine Logik mit
[KDE Infinit Desktop](https://github.com/RandomNam3/kde-infinit-desktop). Diese Dateien werden **unverändert**
übernommen. Änderungen daran gehören zuerst in das KDE-Repo und werden dann hierher kopiert, damit beide
Versionen gleich rechnen.

| Hier | Dort | Stand (Commit im KDE-Repo) |
|---|---|---|
| `src/core/` | `src/core/` | `1340a16` (Branch `claude/funny-ride-js1sro`) |
| `tests/test_{camera,motion,velocitytracker,parking,navigation,placementmemory,groups,areas}.cpp` | `tests/` | `1340a16` |

Prüfen, ob beide noch gleich sind:

```bash
diff -r src/core ../kde-infinit-desktop/src/core --exclude=CMakeLists.txt
```
