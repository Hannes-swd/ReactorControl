
function knopf_hoch.onclick()
    zaehler = zaehler + 1
end


function knopf_runter.onclick()
    zaehler = zaehler - 1
end


function zaehler_anzeige.ondraw()
    screen.clear(6, 12, 8)
    -- "%4d" haelt die Zahl vierstellig rechtsbuendig, damit sie beim Zaehlen nicht wandert.
    screen.seg7(27, 77, 70, string.format("%4d", zaehler), 0, 255, 120)
end
