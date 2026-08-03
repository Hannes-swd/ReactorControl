#pragma once

#include <string>
#include <unordered_map>
#include <vector>

// Reiner Logik-/Zustands-Speicher fuer platzierte Elemente (Buttons, Schalter, Lampen,
// Anzeigen, ...) - kennt weder Modelle noch Rendering, damit er spaeter unveraendert von
// Simulationslogik oder einem Speicherstand mitbenutzt werden kann. PlacementSystem fuellt
// die Registry beim Laden (RegisterType/RegisterInstance) und fragt sie beim Zeichnen nach
// dem aktuellen Zustand jeder Instanz; main.cpp meldet Klicks (SetFrameClick) und beliebige
// andere Systeme koennen Zustand/Klick jederzeit auslesen (GetState.../WasClicked).
//
// Zustand: jede Instanz hat einen Typ (definiert eine geordnete Liste moeglicher
// Zustandsnamen) und einen aktuellen Index in dieser Liste. Ein Klick schaltet den Index
// modulo Zustandsanzahl weiter - bei einem 2-Zustands-Schalter ein Toggle, bei einer
// 4-Zustands-Lampe ein Rundlauf, bei einem 1-Zustands-Taster bleibt der Zustand gleich,
// nur das Klick-Event ist auslesbar. So deckt ein einziger Mechanismus alle Faelle ab, ohne
// dass Zustandsanzahl oder "ist klickbar" irgendwo im Code unterschieden werden muss.
//
// Klick-Event: WasClicked(id) vergleicht nur gegen die zuletzt per SetFrameClick gemeldete
// id - kein "read-and-clear". Dadurch koennen beliebig viele Stellen im selben Frame
// denselben Klick abfragen, ohne sich gegenseitig das Event wegzulesen; im naechsten Frame
// macht der naechste SetFrameClick-Aufruf (auch mit leerer id) die alte Meldung ungueltig.
//
// Reaktorlogik/Trigger: ein Klick kann per RegisterTrigger zusaetzlich den Zustand ANDERER
// Elemente setzen (z.B. Notaus-Schluessel klicken -> Lampe zwingend auf "aus"), unabhaengig
// vom eigenen Zustandswechsel des geklickten Elements. Das ist rein datengetrieben (siehe
// PlacementSystem, "triggers" in der JSON) - hier im Code kommt kein einziger konkreter
// Element-Name vor.
class ElementRegistry {
public:
    // Legt einen Typ mit seiner geordneten Zustandsliste an (oder ueberschreibt ihn, falls
    // derselbe typeId erneut registriert wird). states darf nicht leer sein.
    void RegisterType(const std::string& typeId, std::vector<std::string> states);

    // Legt eine Instanz mit gegebenem Typ und Start-Zustand an. typeId muss zuvor per
    // RegisterType angelegt worden sein, sonst wird die Instanz nicht registriert.
    void RegisterInstance(const std::string& id, const std::string& typeId, int initialStateIndex);

    int GetStateIndex(const std::string& id) const;
    const std::string& GetStateName(const std::string& id) const;

    // true, wenn `id` per RegisterInstance angelegt wurde. Wird vom Skriptsystem gebraucht,
    // um zu entscheiden, ob ein im Skript benutzter Name ueberhaupt ein Element ist.
    bool Exists(const std::string& id) const;

    // Alle registrierten Element-ids (unsortiert). Fuer Uebersichts-/Debugausgaben und
    // die Skript-Funktion elements().
    std::vector<std::string> AllIds() const;

    // Geordnete Zustandsliste des Typs, den `id` hat - leer, falls id unbekannt.
    const std::vector<std::string>& StateNames(const std::string& id) const;

    void SetState(const std::string& id, int stateIndex);
    void SetState(const std::string& id, const std::string& stateName);

    // Meldet: wird `onId` geklickt, soll zusaetzlich `targetId` auf den Zustand `targetState`
    // gesetzt werden (siehe SetFrameClick). Mehrfachaufrufe mit demselben onId sammeln sich -
    // ein Klick kann so beliebig viele andere Elemente gleichzeitig setzen.
    void RegisterTrigger(const std::string& onId, const std::string& targetId, const std::string& targetState);

    // Vom main-Loop einmal pro Frame aufgerufen: id des in diesem Frame geklickten Elements,
    // oder "" wenn diesen Frame nichts geklickt wurde. Schaltet bei nicht-leerer id zusaetzlich
    // deren eigenen Zustand einen Schritt weiter UND wendet alle fuer diese id registrierten
    // Trigger an (siehe Klassenkommentar).
    void SetFrameClick(const std::string& id);

    // true genau in dem Frame, in dem SetFrameClick(id) zuletzt mit dieser id aufgerufen wurde.
    bool WasClicked(const std::string& id) const;

    // id des zuletzt per SetFrameClick gemeldeten Klicks ("" falls keiner). Fuer Systeme, die
    // nicht nach einer bestimmten id fragen, sondern wissen wollen OB ueberhaupt geklickt
    // wurde (z.B. das Skriptsystem, das daraufhin den passenden onclick-Handler sucht).
    const std::string& FrameClickedId() const { return frameClickedId; }

    // Anzahl Zustaende des Typs, den `id` hat. 0 falls id unbekannt.
    size_t StateCount(const std::string& id) const;

private:
    struct Instance {
        std::string typeId;
        int stateIndex = 0;
    };

    static const std::string kEmptyState;
    static const std::vector<std::string> kEmptyStates;

    std::unordered_map<std::string, std::vector<std::string>> stateNamesByType;
    std::unordered_map<std::string, Instance> instances;
    std::string frameClickedId;

    // onId -> Liste von (targetId, targetState), siehe RegisterTrigger.
    std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> triggersByOnId;
};
