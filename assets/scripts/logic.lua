
local notaus_aktiv = false
local laufzeit = 0

function start()
    log("Reaktor-Logik geladen. Elemente:", table.concat(elements(), ", "))
    lampe_1.state = "gruen"
end


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


function lampe_1.onclick()
    log("Lampe steht jetzt auf", lampe_1.state)
end


function tick(dt)
    laufzeit = laufzeit + dt
end


every(0.5, function()
    if not notaus_aktiv then return end

    if lampe_1.state == "rot" then
        lampe_1.state = "aus"
    else
        lampe_1.state = "rot"
    end
end)
