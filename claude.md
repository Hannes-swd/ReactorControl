Erstelle das Grundgerüst für ein C++ Raylib-Projekt namens "ReactorControl".
 
ANFORDERUNGEN:
 
1. ORDNER-STRUKTUR:
   Erstelle folgende Ordnerstruktur:
   - src/
   - assets/
   - build/
   - CMakeLists.txt (im Root)
   - README.md
 
2. SRC-ORDNER - Header-Struktur (noch keine Implementierung):
   Erstelle folgende leere Header-Dateien (.h) mit nur Forward-Declarations:
   
   src/core/gamestate.h
   src/core/gameconfig.h
   src/core/enums.h
   
   src/simulation/reactor.h
   src/simulation/water.h
   src/simulation/heat.h
   
   src/ui/camera_controller.h
   src/ui/layout_engine.h
   src/ui/renderer.h
   
   src/main.cpp
 
3. CMAKE:
   Erstelle CMakeLists.txt mit:
   - C++ Standard 17
   - Raylib eingebunden (mit find_package oder FetchContent)
   - src/ Ordner als Source-Verzeichnis
   - Executable: reactor_control
 
4. MAIN.CPP:
   - Raylib initialisieren
   - Fenster öffnen (1600x900)
   - 3D Kamera einrichten
   - Leere 3D-Welt rendern:
     * DrawGrid für Referenz
     * Eine einfache 3D-Ebene/Box (der Tisch) zeichnen
     * Kamera mit Maus steuerbar (drehen)
   - Event-Loop: ESC zum Beenden
   - FPS anzeigen
 
5. README.md:
   - Projekt-Beschreibung: "Atomreaktor-Simulator mit Echtzeit-Simulation"
   - Build-Anleitung (cmake + make)
   - Ziel: "Spieler muss einen Atomreaktor kontrollieren und PDF zur Fehlersuche nutzen"
 
WICHTIG:
- Keine Button-Implementierung
- Keine Menü-Systeme
- Keine Simulation (nur Setup)
- Nur die 3D-Welt soll funktionieren
- Code soll sauber und gut strukturiert sein (OOP)
- Alle Dateien sollen vorhanden sein (auch wenn leer), damit später einfach implementiert werden kann