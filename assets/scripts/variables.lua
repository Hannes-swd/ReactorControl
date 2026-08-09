
-- Zustand der Testanlage. Reine Platzhalter, bis die echte Simulation in src/simulation/
-- steht - hier geht es nur darum, dass jedes Bedienelement und jede Lampe etwas zu tun hat.

anlage_an = false      -- Schluessel auf START gedreht
p1_an = false          -- Kuehlmittelpumpe 1 laeuft
p2_an = false          -- Kuehlmittelpumpe 2 laeuft
p1_stoer = false       -- Pumpe 1 Stoerung (Start ohne Anlagenfreigabe)
p2_stoer = false
ventil_auf = false     -- Abblaseventil V1

warnung = false        -- Druck ueber Warngrenze
stoerung = false       -- Sammelstoerung, bleibt bis zum Quittieren stehen
quittiert = true       -- false = Stoerung noch nicht quittiert -> Lampen blinken
test_bis = 0           -- Zeitpunkt, bis zu dem der Lampentest alle Lampen zwingt

druck = 0.0            -- bar, Platzhalter-Kuehlkreisdruck
pumpen_zahl = 0        -- wie viele Pumpen gerade laufen (fuer die Anzeige)

zaehler = 0

-- geld und ziel zeigt das Spiel selbst oben rechts im Fenster an (siehe src/ui/hud.h):
-- geld = aktueller Kontostand, ziel = Betrag, der erreicht werden muss. Beides sind ganz
-- normale Variablen - jedes Skript darf sie aendern, die Anzeige folgt sofort.
geld = 0
ziel = 5000
