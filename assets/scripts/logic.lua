
-- Doppelblockanlage. Zwei baugleiche Bloecke (linker und rechter Fluegel), dazwischen die
-- gemeinsame Anlagenfuehrung auf der Mittelflaeche.
--
-- Die Blocklogik steht hier nur EINMAL: alle Bedienelemente eines Blocks heissen b1_... bzw.
-- b2_..., deshalb reicht eine Schleife ueber die Blocknummer. Zugegriffen wird ueber _G, weil
-- jede Element-id automatisch eine globale Variable ist (siehe script_system.h).
--
-- Die Bilanzen sind PLATZHALTER, keine echte Thermohydraulik - siehe Kommentar in variables.lua.

-- --- Grenzwerte -------------------------------------------------------------------------------
local P_DRUCK_WARN, P_DRUCK_SCRAM = 170.0, 185.0
local P_TEMP_WARN,  P_TEMP_SCRAM  = 320.0, 340.0
local NIVEAU_WARN,  NIVEAU_SCRAM  =  30.0,  10.0
local DREH_TRIP                   = 110.0
local ABBRAND_RATE                = 0.0016   -- % Abbrand je Sekunde bei 100 % Leistung
local REVISION_RATE               = 6.0      -- % Abbrand, die eine Sekunde Revision zurueckholt
local SYNC_MIN, SYNC_MAX          =  98.0, 102.0


-- --- Helfer -----------------------------------------------------------------------------------

local function el(name)
    return _G[name]
end

-- Waehrend des Lampentests leuchten alle Lampen, damit man defekte Meldeleuchten findet.
local function lampe(name, an)
    local e = _G[name]
    if e ~= nil then
        e.state = (an or time() < test_bis) and "an" or "aus"
    end
end

-- Unquittierte Stoerungen blinken, quittierte leuchten dauerhaft.
local function blinkt()
    return (time() % 1.0) < 0.5
end

local function melde(text)
    stoerung = true
    quittiert = false
    stoer_text = text
end

-- Stromversorgung: eine Einspeisung UND eine Schiene muessen stehen. Das ist die unterste Stufe
-- der Kette - ohne sie laeuft in der gesamten Anlage kein Antrieb.
local function strom()
    return (agg.eb or agg.ns1 or agg.ns2) and (agg.schA or agg.schB)
end

-- Ereigniskosten. Sie sind der Grund, warum eine Schnellabschaltung mehr ist als eine rote
-- Lampe: der Block steht danach, und der Wiederanlauf kostet. Wer knapp an den Grenzwerten
-- fahrt, wettet gegen genau diesen Betrag.
local KOSTEN_SCRAM = 2500.0
local KOSTEN_BOR   =  600.0

local function scram(b, grund)
    local s = bl[b]
    if not s.scram then
        s.scram = true
        s.scram_zeit = time()
        s.stab = 0.0
        geld = geld - KOSTEN_SCRAM
        melde("B" .. b .. " " .. grund)
    end
end


-- --- Gemeinsame Aggregate ----------------------------------------------------------------------
-- Jedes Aggregat hat dieselbe Bediengruppe (EIN/AUS) und dieselbe Rueckmeldung (BTR/STR). Manche
-- brauchen zusaetzlich ein Vorsystem: die Komponentenkuehlung den Zwischenkuehlkreis, der
-- Kondensator das Hauptkuehlwasser, die Vakuumpumpe beides.
local vorsystem = {
    kkw1 = function() return agg.zwk end,
    kkw2 = function() return agg.zwk end,
    kd1  = function() return agg.kw1 end,
    kd2  = function() return agg.kw2 end,
    vak1 = function() return agg.kd1 and agg.kw1 end,
    vak2 = function() return agg.kd2 and agg.kw2 end,
}

-- Die Stromversorgung selbst darf nicht von sich abhaengen, sonst liesse sie sich nie starten.
local ohne_strom = { eb = true, ns1 = true, ns2 = true, bat = true, schA = true, schB = true }

for name in pairs(agg) do
    local key = name
    el(key .. "_b1").onclick = function()
        if not (ohne_strom[key] or strom()) then
            melde(string.upper(key) .. " OHNE STROM")
            return
        end
        local vor = vorsystem[key]
        if vor ~= nil and not vor() then
            melde(string.upper(key) .. " OHNE VORSYSTEM")
            return
        end
        agg[key] = true
    end
    el(key .. "_b2").onclick = function() agg[key] = false end
end

kupp_b1.onclick = function() kupp_an = true end
kupp_b2.onclick = function() kupp_an = false end

ba_b1.onclick = function() auto_an = false end
ba_b2.onclick = function() auto_an = true end

meld_b1.onclick = function()
    quittiert = true
    stoerung = false
    -- Auch den Klartext loeschen: sonst steht die alte Meldung rot auf dem Display, obwohl
    -- laengst quittiert wurde, und sieht wie eine anstehende Stoerung aus.
    stoer_text = ""
    for b = 1, 2 do
        local s = bl[b]
        s.stoer_hkp1, s.stoer_hkp2 = false, false
        s.stoer_spw1, s.stoer_spw2 = false, false
    end
end
meld_b2.onclick = function() test_bis = time() + 2.0 end

akt_b1.onclick  = function() akt_hoch = false end
akt_b2.onclick  = function() test_bis = time() + 2.0 end
leck_b1.onclick = function() leck_hoch = false end
leck_b2.onclick = function() test_bis = time() + 2.0 end
cnt1_b1.onclick = function() cnt_iso[1] = true end
cnt1_b2.onclick = function() cnt_iso[1] = false end
cnt2_b1.onclick = function() cnt_iso[2] = true end
cnt2_b2.onclick = function() cnt_iso[2] = false end


-- --- Blockbedienung ----------------------------------------------------------------------------

local block_antriebe = { "hkp1", "hkp2", "vol", "nk", "si1", "si2", "nak", "spw1", "spw2", "nspw" }
local braucht_kkw = { hkp1 = true, hkp2 = true }

for b = 1, 2 do
    local s = bl[b]
    local p = "b" .. b .. "_"

    -- Blockfreigabe: Schluesselschalter und NOT-AUS liegen auf der Mittelflaeche.
    el("frg" .. b .. "_b1").onclick = function()
        s.frei = (el("frg" .. b .. "_b1").state == "an")
        if not s.frei then s.stab = 0.0 end
    end
    el("frg" .. b .. "_b2").onclick = function()
        scram(b, "NOT-AUS")
        s.frei = false
        el("frg" .. b .. "_b1").state = "aus"
        s.hkp1, s.hkp2 = false, false
        s.spw1, s.spw2 = false, false
        s.tu, s.gen, s.netz = false, false, false
        s.fd, s.by = false, false
    end

    -- Regelstaebe: ein Klick verstellt um 5 %, aber nur mit freigegebener Stabgruppe.
    el(p .. "rx_b1").onclick = function()
        if not (s.g1 or s.g2) then
            melde("B" .. b .. " KEINE STABGRUPPE")
        elseif s.frei and not s.scram then
            s.stab = math.min(s.stab + 5.0, 100.0)
        end
    end
    el(p .. "rx_b2").onclick = function() s.stab = math.max(s.stab - 5.0, 0.0) end

    el(p .. "stg_b1").onclick = function() s.g1 = not s.g1 end
    el(p .. "stg_b2").onclick = function() s.g2 = not s.g2 end

    el(p .. "sc_b1").onclick = function() scram(b, "SCRAM VON HAND") end
    el(p .. "sc_b2").onclick = function()
        if quittiert and s.p_druck < P_DRUCK_WARN and s.p_temp < P_TEMP_WARN
           and s.d_niveau > NIVEAU_WARN then
            s.scram = false
        end
    end

    el(p .. "bor_b1").onclick = function()
        -- Bor abzureinigen ist teuer, deshalb nur einmal berechnen, nicht bei jedem Klick.
        if not s.bor then geld = geld - KOSTEN_BOR end
        s.bor = true
    end
    el(p .. "bor_b2").onclick = function() s.bor = false end
    el(p .. "dsp_b1").onclick = function() s.dsp = true end
    el(p .. "dsp_b2").onclick = function() s.dsp = false end
    el(p .. "dh_b1").onclick = function() s.dh_hzg = not s.dh_hzg end
    el(p .. "dh_b2").onclick = function() s.dh_spr = not s.dh_spr end

    for _, a in ipairs(block_antriebe) do
        local key = a
        el(p .. key .. "_b1").onclick = function()
            if not (s.frei and strom()) then
                melde("B" .. b .. " " .. string.upper(key) .. " GESPERRT")
                s["stoer_" .. key] = true
            elseif braucht_kkw[key] and not agg["kkw" .. b] then
                melde("B" .. b .. " OHNE KOMPKUEHLUNG")
                s["stoer_" .. key] = true
            else
                s[key] = true
            end
        end
        el(p .. key .. "_b2").onclick = function() s[key] = false end
    end

    el(p .. "pv_b1").onclick = function() s.pv = true end
    el(p .. "pv_b2").onclick = function() s.pv = false end
    el(p .. "fd_b1").onclick = function() if s.frei then s.fd = true end end
    el(p .. "fd_b2").onclick = function() s.fd = false end
    el(p .. "by_b1").onclick = function() s.by = true end
    el(p .. "by_b2").onclick = function() s.by = false end

    -- Die Turbine wird freigegeben, auch wenn noch kein Dampf ansteht - sie laeuft dann
    -- einfach noch nicht hoch. Damit der Bediener nicht raetselt, warum nichts passiert,
    -- meldet sie in dem Fall die fehlende Voraussetzung im Klartext.
    el(p .. "tu_b1").onclick = function()
        if s.vakuum <= 50.0 then
            melde("B" .. b .. " KEIN VAKUUM")
        else
            s.tu = true
            if s.d_druck <= 15.0 then melde("B" .. b .. " DAMPFDRUCK TIEF") end
        end
    end
    el(p .. "tu_b2").onclick = function() s.tu = false; s.netz = false end
    el(p .. "gen_b1").onclick = function() s.gen = true end
    el(p .. "gen_b2").onclick = function() s.gen = false; s.netz = false end

    -- Netzschalter des Blocks (liegt auf der Mittelflaeche)
    el("ntz" .. b .. "_b1").onclick = function()
        if s.gen and s.dreh >= SYNC_MIN and s.dreh <= SYNC_MAX then
            s.netz = true
        else
            melde("B" .. b .. " SYNC GESPERRT")
        end
    end
    el("ntz" .. b .. "_b2").onclick = function() s.netz = false end
end


-- --- Simulation eines Blocks ---------------------------------------------------------------------

local function block_tick(b, dt)
    local s = bl[b]

    if not strom() then
        if s.hkp1 or s.hkp2 or s.spw1 or s.spw2 then
            melde("B" .. b .. " EIGENBEDARF AUS")
        end
        s.hkp1, s.hkp2 = false, false
        s.spw1, s.spw2 = false, false
        s.vol, s.nk, s.si1, s.si2, s.nak, s.nspw = false, false, false, false, false, false
        if s.frei then scram(b, "EIGENBEDARF AUS") end
    end

    -- Betriebsart AUTO: Speisewasser regelt das Dampferzeugerniveau selbsttaetig.
    if auto_an and strom() and s.frei then
        s.spw1 = s.d_niveau < 55.0
        s.spw2 = s.d_niveau < 45.0
    end

    -- Reaktor: Leistung folgt der Stabstellung traege, dazu die negative Temperaturrueckkopplung,
    -- die den Reaktor von selbst stabilisiert.
    if s.scram then
        -- Nachzerfallswaerme: nach der Abschaltung bleiben Prozente uebrig, die nur langsam
        -- abklingen. Die Kuehlung darf danach nicht abgestellt werden.
        local nach = 6.0 * math.exp(-(time() - s.scram_zeit) / 90.0)
        s.leistung = math.max(nach, s.leistung - 80.0 * dt)
    else
        local ziel_l = 0.0
        if s.frei then
            ziel_l = s.stab - (s.p_temp - 300.0) * 0.25 - (s.bor and 40.0 or 0.0)
            -- Abgebrannter Brennstoff gibt weniger her: die erreichbare Leistung sinkt mit dem
            -- Abbrand. Das ist die Uhr, die zur Revision zwingt - frueher oder spaeter.
            ziel_l = math.min(ziel_l, 100.0 - s.abbrand)
        end
        s.leistung = s.leistung + (ziel_l - s.leistung) * dt * 0.5
    end
    s.leistung = math.max(s.leistung, 0.0)

    -- Abbrand und Revision. Nachgeladen wird nur im abgestellten, freigegebenen Zustand: Schluessel
    -- auf AUS, Staebe unten, Leistung abgeklungen. Wann man diesen Stillstand einlegt, entscheidet
    -- der Bediener - er kostet Erloes, aber ohne ihn faellt die Leistung immer weiter.
    s.abbrand = math.min(s.abbrand + s.leistung * dt * ABBRAND_RATE, 100.0)
    s.revision = (not s.frei) and s.stab <= 0.0 and s.leistung < 1.0 and s.abbrand > 0.0
    if s.revision then
        s.abbrand = math.max(s.abbrand - REVISION_RATE * dt, 0.0)
    end

    -- Primaerkreis
    local pp = (s.hkp1 and 1 or 0) + (s.hkp2 and 1 or 0)
    local de_faktor = s.d_niveau > 20.0 and 1.0 or 0.15
    local abfuhr = pp * 1.5 * math.max(s.p_temp - 200.0, 0.0) * de_faktor
                   + (s.nk and 200.0 or 0.0)
                   + (s.nak and 60.0 or 0.0)
                   + ((s.si1 or s.si2) and 80.0 or 0.0)
    s.p_temp = math.max(s.p_temp + (s.leistung * 3.0 - abfuhr) * dt / 40.0, 20.0)

    local p_ziel = 1.0 + math.max(s.p_temp - 100.0, 0.0) * 0.7
                   + (s.dh_hzg and 25.0 or 0.0) - (s.dh_spr and 25.0 or 0.0)
    s.p_druck = s.p_druck + (p_ziel - s.p_druck) * dt * 0.6
    if s.pv then s.p_druck = s.p_druck - 20.0 * dt end
    if s.dsp and s.p_druck < 40.0 then s.p_druck = s.p_druck + 8.0 * dt end
    s.p_druck = math.max(s.p_druck, 0.0)

    -- Dampferzeuger
    local entnahme = 0.0
    if s.fd and s.d_druck > 5.0 then entnahme = entnahme + (s.tu and 0.10 or 0.02) * s.d_druck end
    if s.by then entnahme = entnahme + 0.08 * s.d_druck end
    s.d_druck = math.max(s.d_druck + (abfuhr * 0.02 - entnahme) * dt, 0.0)

    local sp = (s.spw1 and 1 or 0) + (s.spw2 and 1 or 0) + (s.nspw and 0.5 or 0.0)
    s.d_niveau = math.min(math.max(s.d_niveau + (sp * 4.0 - entnahme * 1.2) * dt, 0.0), 100.0)

    -- Kondensator und Vakuum
    local vak_ziel = agg["vak" .. b] and 95.0 or 0.0
    s.vakuum = s.vakuum + (vak_ziel - s.vakuum) * dt * 0.4

    -- Turbine. Der Drehzahlregler haelt sie auf Nenndrehzahl; hochdrehen tut sie nur beim
    -- LASTABWURF, also wenn der Netzschalter faellt, waehrend noch Dampf auf der Maschine steht.
    if s.netz_vorher and not s.netz and s.tu and s.fd then s.lastabwurf = true end
    s.netz_vorher = s.netz
    if not s.tu or not s.fd then s.lastabwurf = false end

    local dreh_ziel = 0.0
    if s.tu and s.fd and s.d_druck > 15.0 and s.vakuum > 50.0 then
        dreh_ziel = s.lastabwurf and 115.0 or 100.0
    end
    s.dreh = s.dreh + (dreh_ziel - s.dreh) * dt * 0.5
    if s.dreh > DREH_TRIP then
        s.dreh = DREH_TRIP
        s.tu, s.fd, s.gen, s.netz = false, false, false, false
        melde("B" .. b .. " UEBERDREHZAHL")
    end

    if s.netz and (not s.gen or s.dreh < 95.0) then
        s.netz = false
        melde("B" .. b .. " VOM NETZ")
    end
    s.leistung_el = (s.netz and s.gen) and s.leistung * 3.3 or 0.0

    -- Verschleiss durch Ueberlast. Ueber 100 % zahlt das Netz einen Spitzenzuschlag (siehe
    -- wirtschaft_tick), aber die Antriebe halten das nicht beliebig lange aus. Je weiter drueber
    -- und je heisser der Primaerkreis, desto wahrscheinlicher faellt eine Pumpe aus.
    if s.leistung > 100.0 then s.ueberlast = s.ueberlast + dt end
    local risiko = math.max(s.leistung - 100.0, 0.0) * 0.004
                   + (s.p_temp > P_TEMP_WARN and 0.03 or 0.0)
    if risiko > 0.0 and math.random() < risiko * dt then
        local laufend = {}
        for _, a in ipairs({ "hkp1", "hkp2", "spw1", "spw2" }) do
            if s[a] then laufend[#laufend + 1] = a end
        end
        if #laufend > 0 then
            local a = laufend[math.random(#laufend)]
            s[a] = false
            s["stoer_" .. a] = true
            melde("B" .. b .. " " .. string.upper(a) .. " AUSGEFALLEN")
        end
    end

    -- Schutzausloesungen
    if s.p_druck > P_DRUCK_SCRAM then scram(b, "DRUCK HOCH") end
    if s.p_temp  > P_TEMP_SCRAM  then scram(b, "KUEHLMITTEL HEISS") end
    if s.d_niveau < NIVEAU_SCRAM and s.frei then scram(b, "DE-NIVEAU TIEF") end

    return s.p_druck > P_DRUCK_WARN or s.p_temp > P_TEMP_WARN
           or s.d_niveau < NIVEAU_WARN or s.d_niveau > 95.0
           or (s.vakuum < 50.0 and s.tu)
end


-- --- Rueckmeldungen eines Blocks -------------------------------------------------------------------

local function block_lampen(b)
    local s = bl[b]
    local p = "b" .. b .. "_"
    local stoer_an = stoerung and (quittiert or blinkt())

    lampe(p .. "rx_l1", s.leistung > 1.0)
    lampe(p .. "rx_l2", s.leistung > 105.0 or s.p_temp > P_TEMP_WARN)
    lampe(p .. "sc_l1", s.scram and (quittiert or blinkt()))
    lampe(p .. "sc_l2", not s.scram and s.frei)
    lampe(p .. "bor_l1", s.bor)
    lampe(p .. "bor_l2", s.bor)
    lampe(p .. "stg_l1", s.g1)
    lampe(p .. "stg_l2", s.g2)
    lampe(p .. "dsp_l1", s.dsp)
    lampe(p .. "dsp_l2", s.p_druck < 40.0)

    lampe(p .. "dh_l1", s.dh_hzg)
    lampe(p .. "dh_l2", s.dh_spr)
    lampe(p .. "pv_l1", s.pv)
    lampe(p .. "pv_l2", not s.pv)
    lampe(p .. "fd_l1", s.fd)
    lampe(p .. "fd_l2", not s.fd)
    lampe(p .. "by_l1", s.by)
    lampe(p .. "by_l2", not s.by)

    for _, a in ipairs(block_antriebe) do
        lampe(p .. a .. "_l1", s[a])
        lampe(p .. a .. "_l2", (s["stoer_" .. a] or false) and blinkt())
    end

    -- Freigabe statt Drehzahl: sonst gibt der Taster EIN keine sichtbare Rueckmeldung, solange
    -- noch kein Dampf ansteht. Die tatsaechliche Drehzahl steht auf dem Display BLOCK x.
    lampe(p .. "tu_l1", s.tu)
    lampe(p .. "tu_l2", s.dreh > 103.0)
    lampe(p .. "gen_l1", s.gen)
    lampe(p .. "gen_l2", s.gen and s.dreh >= SYNC_MIN and s.dreh <= SYNC_MAX and not s.netz)

    -- Blockfreigabe und Netzschalter liegen auf der Mittelflaeche
    lampe("frg" .. b .. "_l1", s.frei)
    lampe("frg" .. b .. "_l2", stoer_an)
    lampe("ntz" .. b .. "_l1", s.netz)
    lampe("ntz" .. b .. "_l2", not s.netz)

    -- Statusleiste des Blocks: schaltet beim Anfahren von links nach rechts durch
    local pp = (s.hkp1 and 1 or 0) + (s.hkp2 and 1 or 0)
    local sp = (s.spw1 and 1 or 0) + (s.spw2 and 1 or 0)
    lampe("st" .. b .. "_rx",  s.leistung > 1.0)
    lampe("st" .. b .. "_pri", pp >= 2)
    lampe("st" .. b .. "_dh",  s.p_druck > 100.0 and s.p_druck < P_DRUCK_WARN)
    lampe("st" .. b .. "_de",  s.d_niveau > NIVEAU_WARN)
    lampe("st" .. b .. "_spw", sp >= 1)
    lampe("st" .. b .. "_tur", s.dreh > 95.0)
    lampe("st" .. b .. "_gen", s.gen)
    lampe("st" .. b .. "_ntz", s.netz)
end



-- --- Wirtschaft --------------------------------------------------------------------------------
--
-- Hier entstehen die Entscheidungen des Spiels. Es gibt keinen richtigen Fahrweg, sondern nur
-- teurere und billigere: Strom bringt zum jeweiligen Marktpreis Geld, jedes laufende Aggregat
-- kostet welches, und beides laeuft gleichzeitig. Das Handbuch beschreibt die Moeglichkeiten,
-- nicht die Reihenfolge.

-- Betriebskosten je Sekunde. Sicherheit ist bewusst nicht gratis: die zweite Pumpe, der Diesel
-- und die Notkuehlung kosten laufend. Wer alles mitlaufen laesst, faehrt sicher und arm.
-- Nicht aufgefuehrt sind Lueftung, Brandschutz, Wartenklima und Batterie: sie kosten nichts.
-- Sonst waere ihr Abschalten eine Ersparnis ohne jeden Nachteil - also keine Entscheidung,
-- sondern nur ein Handgriff, den man jedes Mal machen muesste.
local KOSTEN_AGG = {
    eb = 2.0, ns1 = 16.0, ns2 = 16.0, schA = 0.5, schB = 0.5,
    zwk = 2.0, kw1 = 2.0, kw2 = 2.0, kd1 = 2.0, kd2 = 2.0,
    vak1 = 1.5, vak2 = 1.5, kkw1 = 1.5, kkw2 = 1.5,
}
local KOSTEN_BLOCK = {
    hkp1 = 3.0, hkp2 = 3.0, spw1 = 2.0, spw2 = 2.0, nspw = 2.5,
    vol = 1.0, nk = 9.0, si1 = 6.0, si2 = 6.0, nak = 4.0,
    dh_hzg = 1.5, dh_spr = 0.5,
}
local KOSTEN_BOR_LAUFEND  = 25.0   -- solange eingespeist wird
local KOSTEN_BLACKOUT     = 40.0   -- freigegebener Block ohne Stromversorgung
local KOSTEN_REVISION     = 55.0   -- Nachladen, solange die Revision laeuft
local VERLUST_UMLEIT      = 0.12   -- Dampf ueber die Umleitstation je bar: erzeugt, nicht verkauft
local UEBERLAST_ZUSCHLAG  = 1.45   -- Spitzenzuschlag auf den Erloes bei Leistung ueber 100 %

-- Preiskurve. Ein "Tag" dauert markt.tag Sekunden; darueber liegt eine kuerzere Welle, damit die
-- Spitzen nicht immer an derselben Stelle stehen und man den Zeitpunkt wirklich abpassen muss.
local function preis_kurve(t)
    local tag  = math.sin(2.0 * math.pi * t / markt.tag)
    local welle = math.sin(2.0 * math.pi * t / (markt.tag * 0.29))
    return math.max(0.25, 1.0 + 0.65 * tag + 0.22 * welle)
end

local function laufende_kosten()
    local k = 0.0
    for name, preis in pairs(KOSTEN_AGG) do
        if agg[name] then k = k + preis end
    end
    for b = 1, 2 do
        local s = bl[b]
        for name, preis in pairs(KOSTEN_BLOCK) do
            if s[name] then k = k + preis end
        end
        if s.bor then k = k + KOSTEN_BOR_LAUFEND end
        if s.revision then k = k + KOSTEN_REVISION end
        if s.frei and not strom() then k = k + KOSTEN_BLACKOUT end
        if s.by then k = k + VERLUST_UMLEIT * s.d_druck end
    end
    return k
end

-- Neues Fahrplanangebot. Das Band liegt immer so, dass ein Block allein es knapp halten kann und
-- zwei Bloecke es bequem halten - beide Wege sind gueltig, sie kosten nur Unterschiedliches.
local function neuer_fahrplan()
    local unten = 120.0 + math.random() * 330.0
    local dauer = 60.0 + math.random() * 60.0
    fahrplan.aktiv   = true
    fahrplan.min     = unten
    fahrplan.max     = unten + 110.0
    fahrplan.rest    = dauer
    fahrplan.praemie = dauer * 14.0 * markt.preis
    fahrplan.strafe  = fahrplan.praemie * 0.75
    fahrplan.angenommen = false
end

function wirtschaft_tick(dt)
    markt.zeit  = markt.zeit + dt
    markt.preis = preis_kurve(markt.zeit)

    -- Erloes. Der Spitzenzuschlag gilt nur fuer den Block, der tatsaechlich ueber 100 % faehrt -
    -- er ist die Bezahlung fuer das Ausfallrisiko in block_tick.
    local netz_mw = 0.0
    for b = 1, 2 do
        local s = bl[b]
        netz_mw = netz_mw + s.leistung_el
        local faktor = s.leistung > 100.0 and UEBERLAST_ZUSCHLAG or 1.0
        geld = geld + s.leistung_el * markt.basis * markt.preis * faktor * dt
    end

    markt.kosten = laufende_kosten()
    geld = geld - markt.kosten * dt

    -- Fahrplan. Angenommen wird er nicht per Taster, sondern indem man liefert: wer einmal im
    -- Band war, steht im Wort und zahlt, wenn er es wieder verlaesst. Wer das Angebot ignoriert,
    -- faehrt einfach weiter zum Marktpreis.
    if fahrplan.aktiv then
        fahrplan.rest = fahrplan.rest - dt
        local drin = netz_mw >= fahrplan.min and netz_mw <= fahrplan.max
        if drin then fahrplan.angenommen = true end
        if fahrplan.angenommen and not drin then
            geld = geld - fahrplan.strafe
            fahrplan.aktiv = false
            fahrplan.naechst = 40.0 + math.random() * 40.0
            melde("FAHRPLAN VERLETZT")
        elseif fahrplan.rest <= 0.0 then
            if fahrplan.angenommen then geld = geld + fahrplan.praemie end
            fahrplan.aktiv = false
            fahrplan.naechst = 40.0 + math.random() * 40.0
        end
    else
        fahrplan.naechst = fahrplan.naechst - dt
        if fahrplan.naechst <= 0.0 then neuer_fahrplan() end
    end
end


-- --- Ablauf pro Frame ------------------------------------------------------------------------------

function tick(dt)
    local warn = false
    for b = 1, 2 do
        if block_tick(b, dt) then warn = true end
    end

    -- Erloes, Betriebskosten und Fahrplan (siehe Abschnitt Wirtschaft weiter oben).
    wirtschaft_tick(dt)

    -- Aktivitaet und Leckage steigen, wenn ein Block ohne abgeriegeltes Containment ueberhitzt.
    for b = 1, 2 do
        if bl[b].p_temp > P_TEMP_WARN and not cnt_iso[b] then
            akt_hoch = true
            leck_hoch = true
        end
    end

    warnung = warn or akt_hoch or leck_hoch or not strom()

    -- Gemeinsame Aggregate: faellt im Betrieb der Strom oder ein Vorsystem weg, faellt das
    -- Aggregat mit ab.
    for name in pairs(agg) do
        if agg[name] then
            if not (ohne_strom[name] or strom()) then agg[name] = false end
            local vor = vorsystem[name]
            if vor ~= nil and not vor() then agg[name] = false end
        end
    end

    for b = 1, 2 do
        block_lampen(b)
    end

    local stoer_an = stoerung and (quittiert or blinkt())
    lampe("meld_l1", warnung)
    lampe("meld_l2", stoer_an)
    lampe("ba_l1", not auto_an)
    lampe("ba_l2", auto_an)
    lampe("kupp_l1", kupp_an)
    lampe("kupp_l2", not kupp_an)
    lampe("akt_l1", not akt_hoch)
    lampe("akt_l2", akt_hoch and blinkt())
    lampe("leck_l1", not leck_hoch)
    lampe("leck_l2", leck_hoch and blinkt())
    for b = 1, 2 do
        lampe("cnt" .. b .. "_l1", not cnt_iso[b])
        lampe("cnt" .. b .. "_l2", cnt_iso[b])
    end

    for name in pairs(agg) do
        lampe(name .. "_l1", agg[name])
        lampe(name .. "_l2", false)
    end
    -- Die Sammelstoerung der Stromversorgung haengt nicht am einzelnen Aggregat, sondern daran,
    -- ob ueberhaupt Strom auf einer Schiene steht.
    lampe("eb_l2", not strom())
end


-- --- Displays ---------------------------------------------------------------------------------------
--
-- Raster aller Anzeigen (Textur 256 x 224):
--   y =   4  Kopfzeile     y = 100  erste Textzeile   y = 152  zweite Textzeile
--   y =  32  Hauptwert     y = 124  Balken            y = 178  dritte Textzeile
--
-- Schriftgroesse 20: raylibs Standardschrift hat 10 Pixel Grundgroesse, ein glattes Vielfaches
-- wird pixelgenau verdoppelt. Krumme Faktoren verwaschen die Buchstaben schon in der Textur.

local GRUEN  = { 0, 255, 120 }
local DUNKEL = { 0, 205, 100 }
local ROT    = { 255, 90, 60 }

local SCHRIFT = 20
local Y_KOPF, Y_ZAHL, Y_Z1, Y_BALKEN, Y_Z2, Y_Z3 = 4, 32, 100, 124, 152, 178

local function kopf(text)
    screen.clear(6, 12, 8)
    screen.text(8, Y_KOPF, SCHRIFT, text, DUNKEL[1], DUNKEL[2], DUNKEL[3])
end

local function zeile(y, text, rot)
    local c = rot and ROT or DUNKEL
    screen.text(8, y, SCHRIFT, text, c[1], c[2], c[3])
end

local function zahl(text, kritisch)
    local c = kritisch and ROT or GRUEN
    screen.seg7(16, Y_ZAHL, 58, text, c[1], c[2], c[3])
end

local function balken(anteil, kritisch)
    local c = kritisch and ROT or GRUEN
    screen.bar(10, Y_BALKEN, 236, 20, math.min(math.max(anteil, 0.0), 1.0), c[1], c[2], c[3])
end


for b = 1, 2 do
    local s = bl[b]
    local p = "b" .. b .. "_"

    el(p .. "disp_rx").ondraw = function()
        kopf("REAKTOR " .. b .. " TH %")
        zahl(string.format("%5.1f", s.leistung), s.leistung > 105.0)
        zeile(Y_Z1, string.format("STAEBE %3.0f %%", s.stab))
        balken(s.stab / 100.0, false)
        if s.scram then
            zeile(Y_Z2, "SCRAM AKTIV", true)
        else
            zeile(Y_Z2, s.bor and "BOREINSPEISUNG" or (s.frei and "FREIGEGEBEN" or "GESPERRT"))
        end
        -- Abbrand: die Zahl, an der man ablesen kann, wie lange dieser Block noch Volllast kann.
        if s.revision then
            zeile(Y_Z3, string.format("REVISION %3.0f %%", s.abbrand))
        else
            zeile(Y_Z3, string.format("ABBRAND %3.0f %%", s.abbrand), s.abbrand > 70.0)
        end
    end

    el(p .. "disp_pri").ondraw = function()
        kopf("PRIMAER " .. b .. " GRD C")
        zahl(string.format("%5.1f", s.p_temp), s.p_temp > P_TEMP_WARN)
        zeile(Y_Z1, string.format("DRUCK %5.1f BAR", s.p_druck))
        balken(s.p_druck / 200.0, s.p_druck > P_DRUCK_WARN)
        local pp = (s.hkp1 and 1 or 0) + (s.hkp2 and 1 or 0)
        zeile(Y_Z2, string.format("HKP %d VON 2", pp))
    end

    el(p .. "disp_de").ondraw = function()
        kopf("DAMPFERZ " .. b .. " BAR")
        zahl(string.format("%5.1f", s.d_druck), false)
        zeile(Y_Z1, string.format("NIVEAU %4.1f %%", s.d_niveau))
        balken(s.d_niveau / 100.0, s.d_niveau < NIVEAU_WARN or s.d_niveau > 95.0)
        zeile(Y_Z2, s.fd and "FRISCHDAMPF AUF" or "FRISCHDAMPF ZU")
    end

    _G["disp_b" .. b].ondraw = function()
        kopf("BLOCK " .. b .. " EL MW")
        zahl(string.format("%5.0f", s.leistung_el), false)
        zeile(Y_Z1, string.format("DREHZAHL %4.0f %%", s.dreh))
        balken(s.vakuum / 100.0, s.vakuum < 50.0)
        zeile(Y_Z2, string.format("VAKUUM %4.0f %%", s.vakuum))
        zeile(Y_Z3, s.netz and "AM NETZ" or (s.gen and "ERREGT" or "VOM NETZ"))
    end
end

-- Das Marktdisplay. Kontostand und Ziel stehen bereits im Fenster oben rechts, deshalb zeigt
-- dieses Display das, was die Entscheidung traegt: was die Leistung gerade wert ist, was der
-- laufende Betrieb kostet und ob gerade ein Fahrplan zu holen ist.
disp_netz.ondraw = function()
    local netz_mw = bl[1].leistung_el + bl[2].leistung_el
    kopf("NETZ / MARKT MW")
    zahl(string.format("%5.0f", netz_mw), false)
    zeile(Y_Z1, string.format("PREIS %4.2f  -%3.0f/S", markt.preis, markt.kosten))
    balken(markt.preis / 2.0, markt.preis < 0.6)
    if fahrplan.aktiv then
        local drin = netz_mw >= fahrplan.min and netz_mw <= fahrplan.max
        zeile(Y_Z2, string.format("SOLL %4.0f-%4.0f MW", fahrplan.min, fahrplan.max), not drin)
        zeile(Y_Z3, string.format("%3.0f S  PRAEMIE %4.0f", fahrplan.rest, fahrplan.praemie),
              fahrplan.angenommen and not drin)
    else
        zeile(Y_Z2, "KEIN FAHRPLAN")
        zeile(Y_Z3, string.format("ANGEBOT IN %3.0f S", fahrplan.naechst))
    end
end

disp_meld.ondraw = function()
    kopf("MELDUNGEN")
    zeile(34, stoer_text ~= "" and stoer_text or "KEINE STOERUNG", stoer_text ~= "")
    zeile(62, strom() and "STROM OK" or "KEIN STROM", not strom())
    zeile(88, agg.zwk and "ZWISCHENKUEHL OK" or "ZWISCHENKUEHL AUS", not agg.zwk)
    zeile(114, akt_hoch and "AKTIVITAET HOCH" or "AKTIVITAET NORM", akt_hoch)
    zeile(140, leck_hoch and "LECKAGE HOCH" or "LECKAGE NORMAL", leck_hoch)
    zeile(166, auto_an and "SPEISUNG AUTO" or "SPEISUNG HAND")
    zeile(192, quittiert and "QUITTIERT" or "UNQUITTIERT", not quittiert)
end
