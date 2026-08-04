-- Eigene globale Variablen fuer die Reaktor-Logik.
--
-- Diese Datei laeuft immer als erste Skriptdatei. Alles, was hier OHNE "local" geschrieben
-- wird, ist danach in jeder anderen Skriptdatei (z.B. logic.lua) unter demselben Namen
-- verfuegbar - ohne Deklaration, ohne Import:
--
--     variables.lua:   temperatur = 20
--     logic.lua:       temperatur = temperatur + 1
--                      log("Temperatur:", temperatur)
--
-- Wichtig:
--   * "local x = 1" gilt nur innerhalb dieser Datei. Fuer eine globale Variable also
--     einfach "x = 1" schreiben (kein local davor).
--   * Beim Speichern werden alle Skripte neu geladen und die Variablen stehen wieder auf
--     den Werten, die hier stehen. Diese Datei ist der Startzustand, nicht der Spielstand.
--   * Ein Name, den es schon als Element gibt (die ids aus assets/placements/*.json, z.B.
--     lampe_1), ist hier nicht erlaubt und meldet einen Fehler - sonst waere das Element
--     ab dann nicht mehr erreichbar.
--   * Variablen koennen auch anderswo neu angelegt werden; hier stehen sie nur alle an
--     einer Stelle beisammen und existieren garantiert von Anfang an.


-- Zahlen
temperatur = 20
druck = 1.0
leistung = 0

-- Wahrheitswerte
notaus_aktiv = false
kuehlung_an = true

-- Text
reaktor_name = "Block A"

-- Grenzwerte, ab denen die Logik reagieren soll
max_temperatur = 350
warn_temperatur = 280

-- Mehrere zusammengehoerende Werte lassen sich als Tabelle buendeln; der Zugriff laeuft
-- dann ueber einen Punkt, also z.B. pumpe.drehzahl
pumpe = {
    laeuft = false,
    drehzahl = 0,
}
