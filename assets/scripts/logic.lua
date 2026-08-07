
function knopf_hoch.onclick()
    zaehler = zaehler + 1
    -- Platzhalter, bis die Reaktorlogik steht: jeder Schritt bringt Geld, damit die Anzeige
    -- oben rechts (geld/ziel, siehe variables.lua) etwas zu zeigen hat.
    geld = geld + 250
end


function knopf_runter.onclick()
    zaehler = zaehler - 1
    geld = geld - 250
end


function zaehler_anzeige.ondraw()
    screen.clear(6, 12, 8)
    -- "%4d" haelt die Zahl vierstellig rechtsbuendig, damit sie beim Zaehlen nicht wandert.
    screen.seg7(27, 77, 70, string.format("%4d", zaehler), 0, 255, 120)
end
