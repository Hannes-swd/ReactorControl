#pragma once

#include <string>
#include <vector>

struct lua_State;

class ElementRegistry;

// Fuehrt die Reaktor-/Button-Logik als Lua-Skripte aus, damit Spiellogik ohne C++-Aenderung
// und ohne Neukompilieren geschrieben werden kann: alle *.lua-Dateien in einem Ordner (siehe
// GameConfig::ScriptsDirectory) werden geladen, jeden Frame ausgefuehrt und beim Speichern
// automatisch neu geladen (Hot-Reload, siehe Update).
//
// Jedes in der ElementRegistry registrierte Element (id aus den Platzierungs-JSONs) ist im
// Skript direkt als globale Variable dieses Namens verfuegbar - ohne Deklaration, ohne
// Import. Die Variable ist ein Stellvertreter-Objekt ("Proxy") auf den Registry-Eintrag:
//
//   notaus_taste.state          -- aktueller Zustandsname (Text), lesbar UND schreibbar
//   notaus_taste.state_index    -- derselbe Zustand als Index in states (0-basiert), ebenfalls
//                                  lesbar und schreibbar
//   notaus_taste.states         -- Liste aller moeglichen Zustandsnamen des Typs (nur lesbar)
//   notaus_taste.clicked        -- true in genau dem Frame, in dem geklickt wurde (nur lesbar)
//   notaus_taste.id             -- die eigene id als Text (nur lesbar)
//
// Alle anderen Felder gehoeren dem Skript: notaus_taste.zaehler = 0 legt einfach ein eigenes
// Feld am Element ab, das dort dauerhaft liegen bleibt.
//
//   function notaus_taste.onclick()   -- wird bei Klick auf dieses Element aufgerufen
//       lampe_1.state = "rot"
//   end
//
// Eigene Variablen braucht man ebenfalls nicht anzumelden: eine Zuweisung ohne "local" legt
// eine globale Variable an, die alle Skriptdateien sehen. Damit sie an einer Stelle
// beisammenliegen und sicher vor allen anderen Skripten existieren, gibt es dafuer die Datei
// GameConfig::ScriptVariablesFile im Skriptordner - sie wird immer zuerst ausgefuehrt (siehe
// ListScriptPaths). Namen, die schon einem Element gehoeren, sind dabei gesperrt und melden
// einen Fehler, statt das Element zu verdecken (siehe GlobalNewIndex).
//
// Zusaetzlich gibt es zwei frei definierbare globale Funktionen und ein paar Helfer:
//
//   function start()      -- einmalig direkt nach dem (Neu-)Laden der Skripte
//   function tick(dt)     -- jeden Frame; dt = vergangene Sekunden seit dem letzten Frame
//   toggle(element)       -- einen Zustand weiterschalten (wie ein Klick), zyklisch
//   after(sek, fn)        -- fn einmalig nach sek Sekunden ausfuehren
//   every(sek, fn)        -- fn dauerhaft alle sek Sekunden ausfuehren
//   log(...)              -- Ausgabe in die Konsole (print macht dasselbe)
//   time()                -- Laufzeit des Programms in Sekunden
//   elements()            -- Liste aller vorhandenen Element-ids
//   get_state(id) / set_state(id, zustand) / clicked(id)
//                         -- wie die Proxy-Felder, aber mit der id als Text; noetig fuer ids,
//                            die keine gueltigen Lua-Namen sind (z.B. mit Bindestrich).
//
// Fehler im Skript stuerzen das Spiel nicht ab: jeder Aufruf laeuft in einem pcall, die
// Fehlermeldung landet in LastError() (main.cpp blendet sie im Fenster ein) und die naechste
// gespeicherte Skriptversion setzt sie automatisch zurueck.
//
// Reihenfolge pro Frame (siehe Update): erst wertet die ElementRegistry den Klick selbst aus
// (eigener Zustandswechsel + "triggers" aus der JSON, siehe element_registry.h), danach laeuft
// das Skript. Setzt ein Skript denselben Zustand nochmal anders, gewinnt also das Skript.
class ScriptSystem {
public:
    ScriptSystem() = default;
    ~ScriptSystem();

    ScriptSystem(const ScriptSystem&) = delete;
    ScriptSystem& operator=(const ScriptSystem&) = delete;

    // Merkt sich Ordner und Registry und laedt alle darin liegenden *.lua-Dateien einmal.
    // Fehlender Ordner oder gar keine Skripte sind kein Fehler - das Spiel laeuft dann einfach
    // ohne Logik weiter.
    void Load(const char* dirPath, ElementRegistry& registry);

    // Einmal pro Frame nach ElementRegistry::SetFrameClick aufrufen: prueft regelmaessig auf
    // geaenderte/neue/geloeschte Skriptdateien und laedt in dem Fall alles neu, ruft dann den
    // onclick-Handler des in diesem Frame geklickten Elements, faellige Timer und tick(dt) auf.
    void Update(float deltaTime);

    // Letzte Lua-Fehlermeldung ("" wenn alles laeuft). Wird beim erfolgreichen Neuladen und
    // beim naechsten fehlerfreien Frame geleert.
    const std::string& LastError() const { return lastError; }

private:
    // Eine geladene Skriptdatei mit ihrem Aenderungszeitpunkt beim Laden - Grundlage der
    // Hot-Reload-Erkennung (Pfadliste UND Zeitstempel muessen gleich bleiben, damit auch neu
    // angelegte oder geloeschte Dateien erkannt werden).
    struct ScriptFile {
        std::string path;
        long modTime;
    };

    // Baut einen frischen lua_State auf (alte Skript-Globals/Handler verschwinden dabei
    // vollstaendig), registriert Elementzugriff und Helfer und fuehrt alle Skriptdateien in
    // alphabetischer Reihenfolge aus.
    void Reload();
    void Shutdown();

    // Aufrufpaar fuer eine globale Lua-Funktion: BeginCall legt sie auf den Stack und liefert
    // false, wenn es sie gar nicht gibt (dann ist nichts zu tun); dazwischen legt der Aufrufer
    // die Argumente ab; EndCall fuehrt sie aus und faengt Laufzeitfehler ab.
    bool BeginCall(const char* name);
    void EndCall(int argCount);

    void SetError(const std::string& message);
    void ClearError();

    lua_State* L = nullptr;
    ElementRegistry* registry = nullptr;
    std::string directory;
    std::vector<ScriptFile> files;
    float reloadCheckTimer = 0.0f;
    std::string lastError;
};
