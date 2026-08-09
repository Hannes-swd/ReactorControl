
-- Testpult: Schluessel gibt die Anlage frei, zwei Pumpen und ein Ventil halten den Druck im
-- gruenen Band (3-6 bar), das Geld verdient. Der Druck ist noch keine Physik, nur eine
-- Platzhalter-Bilanz - die echte Rechnung kommt spaeter aus src/simulation/.

local WARN_DRUCK  = 6.0
local STOER_DRUCK = 9.0
local GELD_VON, GELD_BIS = 3.0, 6.0


-- Setzt eine Lampe. Waehrend des Lampentests leuchten alle, damit man defekte Meldeleuchten
-- findet, ohne die Anlage in den jeweiligen Zustand fahren zu muessen - so macht man das in
-- einer echten Leitwarte auch.
local function lampe(element, an)
    element.state = (an or time() < test_bis) and "an" or "aus"
end

-- Unquittierte Stoerungen blinken, quittierte leuchten dauerhaft. Dadurch zieht eine neue
-- Meldung den Blick auf sich, eine bereits bekannte nicht mehr.
local function blinkt()
    return (time() % 1.0) < 0.5
end

local function alles_aus()
    p1_an = false
    p2_an = false
    ventil_auf = false
end


-- --- Anlage -----------------------------------------------------------------------------

function anlage_schluessel.onclick()
    -- Die Registry hat den Schluessel bereits weitergeschaltet, hier wird nur uebernommen.
    anlage_an = (anlage_schluessel.state == "an")
    if not anlage_an then
        alles_aus()
    end
end

function anlage_notaus.onclick()
    alles_aus()
    anlage_an = false
    anlage_schluessel.state = "aus"
    stoerung = true
    quittiert = false
end


-- --- Pumpen -----------------------------------------------------------------------------

-- Anlauf nur bei Anlagenfreigabe. Ohne Freigabe laeuft die Pumpe nicht an und meldet
-- stattdessen Stoerung - der Spieler soll merken, dass die Reihenfolge zaehlt.
function p1_ein.onclick()
    if anlage_an then
        p1_an = true
    else
        p1_stoer = true
        quittiert = false
    end
end

function p1_aus.onclick()
    p1_an = false
end

function p2_ein.onclick()
    if anlage_an then
        p2_an = true
    else
        p2_stoer = true
        quittiert = false
    end
end

function p2_aus.onclick()
    p2_an = false
end


-- --- Ventil -----------------------------------------------------------------------------

function v1_auf.onclick()
    ventil_auf = true
end

function v1_zu.onclick()
    ventil_auf = false
end


-- --- Meldungen --------------------------------------------------------------------------

function melde_quittieren.onclick()
    quittiert = true
    stoerung = false
    p1_stoer = false
    p2_stoer = false
end

function melde_test.onclick()
    test_bis = time() + 2.0
end


-- --- Zaehler (Demo fuer das zweite Display) ----------------------------------------------

function knopf_hoch.onclick()
    zaehler = zaehler + 1
end

function knopf_runter.onclick()
    zaehler = zaehler - 1
end


-- --- Ablauf pro Frame ---------------------------------------------------------------------

function tick(dt)
    pumpen_zahl = (p1_an and 1 or 0) + (p2_an and 1 or 0)

    -- Platzhalter-Druckbilanz: der Reaktor heizt auf, jede Pumpe und das offene Ventil
    -- kuehlen dagegen. Mit einer Pumpe steigt der Druck langsam, mit zweien faellt er.
    if anlage_an then
        local zufuhr = 1.5 - pumpen_zahl * 0.8 - (ventil_auf and 0.9 or 0.0)
        druck = druck + zufuhr * dt
    else
        druck = druck - 0.4 * dt
    end
    if druck < 0.0 then druck = 0.0 end

    warnung = druck > WARN_DRUCK
    if druck > STOER_DRUCK then
        -- Schnellabschaltung: Anlage faellt aus, Schluessel springt zurueck.
        stoerung = true
        quittiert = false
        anlage_an = false
        anlage_schluessel.state = "aus"
        alles_aus()
    end

    -- Geld gibt es nur im gruenen Band - Druck halten ist die eigentliche Aufgabe.
    if anlage_an and druck >= GELD_VON and druck <= GELD_BIS then
        geld = geld + 150 * dt
    end

    -- Rueckmeldung: jede Lampe wird jeden Frame neu gesetzt, damit sie immer den echten
    -- Zustand zeigt (und ein versehentlicher Klick auf eine Lampe sofort korrigiert wird).
    local stoer_an = stoerung and (quittiert or blinkt())

    lampe(anlage_lampe_betrieb, anlage_an)
    lampe(anlage_lampe_stoer,   stoer_an)

    lampe(p1_lampe_betrieb, p1_an)
    lampe(p1_lampe_stoer,   p1_stoer and blinkt())
    lampe(p2_lampe_betrieb, p2_an)
    lampe(p2_lampe_stoer,   p2_stoer and blinkt())

    lampe(v1_lampe_auf, ventil_auf)
    lampe(v1_lampe_zu,  not ventil_auf)

    lampe(melde_lampe_warnung,  warnung)
    lampe(melde_lampe_stoerung, stoer_an)
end


-- --- Displays -------------------------------------------------------------------------------

function melde_anzeige.ondraw()
    screen.clear(6, 12, 8)
    screen.text(8, 8, 18, "KUEHLKREIS", 0, 200, 100)

    -- Ueber der Warngrenze wird die Zahl rot - die Anzeige selbst ist damit auch eine Meldung.
    local r, g, b = 0, 255, 120
    if druck > WARN_DRUCK then r, g, b = 255, 90, 60 end
    screen.seg7(14, 34, 62, string.format("%4.1f", druck), r, g, b)

    screen.text(8, 112, 16, "BAR", 0, 200, 100)
    screen.text(80, 112, 16, "PUMPEN " .. pumpen_zahl, 0, 200, 100)
    screen.text(8, 136, 16, ventil_auf and "VENTIL AUF" or "VENTIL ZU", 0, 200, 100)

    screen.bar(10, 164, 236, 24, druck / 10.0, r, g, b)
end

function zaehler_anzeige.ondraw()
    screen.clear(6, 12, 8)
    screen.text(8, 8, 18, "ZAEHLER", 0, 200, 100)
    -- "%4d" haelt die Zahl vierstellig rechtsbuendig, damit sie beim Zaehlen nicht wandert.
    screen.seg7(27, 60, 70, string.format("%4d", zaehler), 0, 255, 120)
    screen.text(8, 180, 16, string.format("GELD %d", math.floor(geld)), 0, 200, 100)
end
