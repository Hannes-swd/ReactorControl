-- ============================================================================
--  Reaktor-Logik
--  Hier wird die komplette Spiellogik geschrieben - kein C++, kein Kompilieren.
--  Datei speichern -> das laufende Spiel laedt sie sofort neu (Hot-Reload).
--  Beliebig viele weitere *.lua-Dateien in diesem Ordner werden auch geladen.
-- ============================================================================
--
--  JEDES ELEMENT aus assets/placements/*.json ist automatisch unter seiner "id"
--  als Variable da. Beispiel: notaus_taste, lampe_1
--
--    notaus_taste.state          aktueller Zustand als Text (lesen UND setzen)
--    notaus_taste.state_index    derselbe Zustand als Zahl (lesen UND setzen, 0 = erster)
--    notaus_taste.states         Liste aller moeglichen Zustaende (nur lesen)
--    notaus_taste.clicked        true genau in dem Moment des Klicks (nur lesen)
--    notaus_taste.id             die id selbst (nur lesen)
--
--  Eigene Felder darfst du direkt am Element ablegen und bleiben erhalten:
--    notaus_taste.anzahl = (notaus_taste.anzahl or 0) + 1
--
--  FUNKTIONEN, die das Spiel automatisch aufruft:
--
--    function start()                einmal beim (Neu-)Laden dieser Datei
--    function tick(dt)               jeden Frame, dt = Sekunden seit letztem Frame
--    function <element>.onclick()    bei Klick auf genau dieses Element
--
--  HELFER:
--
--    toggle(element)          einen Zustand weiterschalten (wie ein Klick), zyklisch
--    after(sek, funktion)     einmal nach sek Sekunden ausfuehren
--    every(sek, funktion)     dauerhaft alle sek Sekunden ausfuehren
--    cancel(funktion)         geplante Ausfuehrungen wieder abbrechen
--    log("text", zahl, ...)   Ausgabe in der Konsole (print geht auch)
--    time()                   Laufzeit in Sekunden
--    elements()               Liste aller vorhandenen Element-ids
--    get_state(id) / set_state(id, zustand) / clicked(id)
--                             dasselbe ueber die id als Text - noetig fuer ids,
--                             die keine gueltigen Lua-Namen sind
--
--  Eigene Variablen einfach oben in der Datei anlegen (z.B. reaktor_temperatur)
--  und in tick() weiterrechnen - sie bleiben zwischen den Frames erhalten.
-- ============================================================================


-- Eigener Zustand der Simulation ---------------------------------------------

local notaus_aktiv = false
local laufzeit = 0


-- Wird einmal beim Laden aufgerufen ------------------------------------------

function start()
    log("Reaktor-Logik geladen. Elemente:", table.concat(elements(), ", "))
    lampe_1.state = "gruen"
end


-- Klick auf den Notaus-Schluessel --------------------------------------------

function notaus_taste.onclick()
    notaus_aktiv = not notaus_aktiv

    if notaus_aktiv then
        log("NOTAUS ausgeloest")
        lampe_1.state = "rot"
    else
        log("Notaus zurueckgesetzt")
        lampe_1.state = "gruen"
    end
end


-- Klick auf die Lampe: schaltet von selbst durch alle Farben (das macht die
-- Registry automatisch bei jedem Klick) - hier nur eine Ausgabe dazu.

function lampe_1.onclick()
    log("Lampe steht jetzt auf", lampe_1.state)
end


-- Laeuft jeden Frame ---------------------------------------------------------

function tick(dt)
    laufzeit = laufzeit + dt
end


-- Beispiel fuer einen wiederkehrenden Vorgang: solange der Notaus aktiv ist,
-- blinkt die Lampe zweimal pro Sekunde zwischen rot und aus.

every(0.5, function()
    if not notaus_aktiv then return end

    if lampe_1.state == "rot" then
        lampe_1.state = "aus"
    else
        lampe_1.state = "rot"
    end
end)
