
-- Zustand der Doppelblockanlage: zwei baugleiche Bloecke plus gemeinsame Anlagenteile.
--
-- ACHTUNG: Alle Bilanzen sind PLATZHALTER, keine echte Thermohydraulik. Sie sind allein danach
-- gewaehlt, dass die Kopplungen zwischen den Anlagenteilen spuerbar sind. Wenn src/simulation/
-- steht, wandern Temperatur, Druck, Niveau und Drehzahl dorthin; das Pult bleibt unveraendert,
-- weil es nur diese Variablen liest.

-- --- Je Block ---------------------------------------------------------------------------------
-- bl[1] gehoert zum linken Fluegel, bl[2] zum rechten. Beide haben denselben Aufbau, deshalb
-- kommt die gesamte Blocklogik in logic.lua nur einmal vor.
bl = {}
for i = 1, 2 do
    bl[i] = {
        frei      = false,   -- Schluesselschalter auf START (Blockfreigabe)
        scram     = false,   -- Schnellabschaltung ausgeloest
        scram_zeit = 0,      -- Zeitpunkt der Ausloesung (fuer die Nachzerfallswaerme)

        stab      = 0.0,     -- Regelstaebe gesamt, 0 = eingefahren, 100 = ausgefahren
        g1        = false,   -- Stabgruppe 1 freigegeben
        g2        = false,   -- Stabgruppe 2 freigegeben
        leistung  = 0.0,     -- thermische Leistung in %
        bor       = false,   -- Boreinspeisung

        p_temp    = 25.0,    -- Kuehlmitteltemperatur in Grad C
        p_druck   = 1.0,     -- Primaerdruck in bar
        hkp1      = false,   -- Hauptkuehlmittelpumpen
        hkp2      = false,
        dh_hzg    = false,   -- Druckhalterheizung
        dh_spr    = false,   -- Druckhaltersprueheinrichtung
        pv        = false,   -- primaeres Abblaseventil
        vol       = false,   -- Volumenregelung
        nk        = false,   -- Notkuehlung
        si1       = false,   -- Sicherheitseinspeisung Strang 1
        si2       = false,   -- Sicherheitseinspeisung Strang 2
        nak       = false,   -- Nachkuehlsystem
        dsp       = false,   -- Druckspeicher scharf

        d_druck   = 1.0,     -- Dampferzeugerdruck in bar
        d_niveau  = 60.0,    -- Dampferzeugerniveau in %
        spw1      = false,   -- Speisewasserpumpen
        spw2      = false,
        nspw      = false,   -- Notspeisewasser
        fd        = false,   -- Frischdampfventil
        by        = false,   -- Umleitstation

        tu        = false,   -- Turbine freigegeben
        dreh      = 0.0,     -- Drehzahl in % der Nenndrehzahl
        gen       = false,   -- Generator erregt
        netz      = false,   -- Netzschalter des Blocks
        netz_vorher = false, -- Netzzustand des Vorframes, um Lastabwurf zu erkennen
        lastabwurf  = false,
        vakuum    = 0.0,     -- Kondensatorvakuum in %
        leistung_el = 0.0,   -- elektrische Leistung in MW

        stoer_hkp1 = false,  -- Einzelstoerungen der Antriebe
        stoer_hkp2 = false,
        stoer_spw1 = false,
        stoer_spw2 = false,

        -- --- Wirtschaft je Block ---
        abbrand   = 0.0,     -- Brennstoffabbrand in %, waechst mit der erzeugten Leistung
        revision  = false,   -- Block steht in Revision (Nachladen laeuft)
        ueberlast = 0.0,     -- Sekunden, die der Block ueber 100 % gefahren wurde
    }
end

-- --- Gemeinsame Anlagenteile -------------------------------------------------------------------
-- Einfache Ein/Aus-Aggregate. Der Schluessel ist zugleich der Namenspraefix der Bediengruppe
-- auf dem Pult, dadurch kommen Bedienung und Rueckmeldung in logic.lua ohne Sonderfaelle aus.
agg = {
    eb   = false,  -- Eigenbedarf (Normalversorgung)
    ns1  = false,  -- Notstromdiesel 1
    ns2  = false,  -- Notstromdiesel 2
    bat  = false,  -- Batterieversorgung
    schA = false,  -- Schiene A
    schB = false,  -- Schiene B
    zwk  = false,  -- Zwischenkuehlkreis
    lue  = false,  -- Lueftung
    kw1  = false,  -- Hauptkuehlwasser Block 1
    kw2  = false,  -- Hauptkuehlwasser Block 2
    kd1  = false,  -- Kondensator Block 1
    kd2  = false,  -- Kondensator Block 2
    vak1 = false,  -- Vakuumpumpe Block 1
    vak2 = false,  -- Vakuumpumpe Block 2
    kkw1 = false,  -- Komponentenkuehlung Block 1
    kkw2 = false,  -- Komponentenkuehlung Block 2
    brd  = false,  -- Brandschutzsystem
    wrt  = false,  -- Wartenklimatisierung
}

kupp_an = false     -- Schaltanlagenkupplung zwischen den beiden Netzschaltern

-- --- Meldungen ----------------------------------------------------------------------------------
auto_an   = false   -- Betriebsart AUTO: Speisewasser regelt das Niveau selbst
warnung   = false
stoerung  = false
quittiert = true
test_bis  = 0       -- Zeitpunkt, bis zu dem der Lampentest alle Lampen zwingt
stoer_text = ""     -- Klartext der letzten Stoerung

akt_hoch  = false   -- Aktivitaet ueber Grenzwert
leck_hoch = false   -- Leckage ueber Grenzwert
cnt_iso = { false, false }  -- Containment abgeriegelt, je Block

-- --- Wirtschaft ---------------------------------------------------------------------------------
-- Der Anlagenbetrieb ist ein Geschaeft, kein Ablaufplan: Strom bringt Geld, jedes laufende
-- Aggregat kostet Geld, und wie viel beides ist, haengt vom Zeitpunkt ab. Daraus entstehen die
-- Entscheidungen - das Handbuch schreibt keine davon vor (siehe docs/documentation.html,
-- Kapitel "Markt und Wirtschaft").

markt = {
    zeit  = 0.0,    -- Anlagenzeit in Sekunden, laeuft ab Spielstart
    tag   = 300.0,  -- Laenge eines vollen Preiszyklus ("Tag") in Sekunden
    preis = 1.0,    -- aktueller Preisfaktor, schwankt um 1.0
    basis = 0.22,   -- Erloes je MW und Sekunde bei Preisfaktor 1.0
    kosten = 0.0,   -- laufende Kosten der letzten Sekunde, nur zur Anzeige
}

-- Fahrplanangebot des Netzes: ein Lastband ueber eine feste Zeit. Wer es haelt, bekommt die
-- Praemie; wer es verlaesst, zahlt die Strafe. Annehmen muss man es nicht - das Angebot laeuft
-- einfach ab, wenn man weiter zum Spotpreis fahren will.
fahrplan = {
    aktiv   = false,
    min     = 0.0,   -- untere Bandgrenze in MW
    max     = 0.0,   -- obere Bandgrenze in MW
    rest    = 0.0,   -- verbleibende Sekunden
    praemie = 0.0,
    strafe  = 0.0,
    naechst = 45.0,  -- Sekunden bis zum naechsten Angebot
    gehalten = true, -- solange true, wurde das Band nie verlassen
}

-- geld und ziel zeigt das Spiel selbst oben rechts im Fenster an (siehe src/ui/hud.h).
geld = 0
ziel = 20000
