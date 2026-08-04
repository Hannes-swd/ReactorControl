#include "script_system.h"

#include "../core/gameconfig.h"
#include "../scene/element_registry.h"

#include "raylib.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

#include <algorithm>
#include <cstring>

namespace {

// Wie oft (in Sekunden) auf geaenderte Skriptdateien geprueft wird. Klein genug, dass sich
// Speichern im Editor sofort anfuehlt, gross genug, dass es kein Verzeichnis-Scan pro Frame ist.
constexpr float kReloadCheckInterval = 0.5f;

// Schluessel in der Lua-Registry (nicht der ElementRegistry!): Cache der bereits erzeugten
// Element-Proxies und die von allen Proxies geteilte Metatabelle.
constexpr const char* kProxyCacheKey = "reactorcontrol.element_proxies";
constexpr const char* kProxyMetaKey = "reactorcontrol.element_meta";

// Feld in der Metatabelle eines Proxies, das dessen Element-id haelt (siehe ProxyId). Beginnt
// mit __, damit es nicht mit einem Lua-Metamethodennamen kollidieren kann.
constexpr const char* kProxyIdField = "__element_id";

// Wird ueber Lua ausgefuehrt, bevor die Benutzer-Skripte laufen: alles, was sich einfacher in
// Lua als ueber die C-API ausdruecken laesst (Timer, die Frame-Reihenfolge). __frame ist der
// einzige Einstiegspunkt, den ScriptSystem::Update pro Frame aufruft - so liegt die Reihenfolge
// "Klick -> Timer -> tick" sichtbar an einer Stelle und braucht nur ein einziges pcall.
constexpr const char* kPrelude = R"LUA(
local timers = {}

function after(seconds, fn)
    timers[#timers + 1] = { left = seconds, fn = fn }
    return fn
end

function every(seconds, fn)
    timers[#timers + 1] = { left = seconds, interval = seconds, fn = fn }
    return fn
end

-- Bricht alle mit after/every geplanten Ausfuehrungen von fn ab.
function cancel(fn)
    for i = #timers, 1, -1 do
        if timers[i].fn == fn then table.remove(timers, i) end
    end
end

print = log

-- Schaltet ein Element einen Zustand weiter (genau wie ein Klick es tut) und faengt hinter dem
-- letzten Zustand wieder beim ersten an. schritt darf auch negativ sein (rueckwaerts).
function toggle(element, schritt)
    local states = element.states
    if #states < 2 then return end
    element.state_index = (element.state_index + (schritt or 1)) % #states
end

local function run_timers(dt)
    local i = 1
    while i <= #timers do
        local timer = timers[i]
        timer.left = timer.left - dt
        if timer.left <= 0 then
            if timer.interval then
                timer.left = timer.left + timer.interval
                i = i + 1
            else
                table.remove(timers, i)
            end
            timer.fn()
        else
            i = i + 1
        end
    end
end

function __frame(dt, clicked_id)
    if clicked_id ~= "" then
        local element = _G[clicked_id]
        if element ~= nil and type(element.onclick) == "function" then
            element.onclick(element)
        end
    end
    run_timers(dt)
    if type(tick) == "function" then tick(dt) end
end
)LUA";

// Die ElementRegistry haengt als Upvalue an jeder registrierten C-Funktion - so braucht keine
// globale Variable zu existieren und mehrere ScriptSystem-Instanzen waeren moeglich.
ElementRegistry& Registry(lua_State* L) {
    return *static_cast<ElementRegistry*>(lua_touserdata(L, lua_upvalueindex(1)));
}

// Die id eines Proxy-Objekts. Sie liegt in dessen Metatabelle und nicht im Proxy selbst, damit
// der Proxy-Table komplett dem Benutzer gehoert: alles, was ein Skript direkt hineinschreibt
// (onclick, eigene Zaehler, ...), kann so nie ein eingebautes Feld verdecken.
std::string ProxyId(lua_State* L, int proxyIndex) {
    if (lua_getmetatable(L, proxyIndex) == 0) {
        return std::string();
    }
    lua_getfield(L, -1, kProxyIdField);
    const char* id = lua_tostring(L, -1);
    std::string result = id != nullptr ? id : "";
    lua_pop(L, 2);
    return result;
}

// Argumente sind bei allen Helfern die id als Text - fuer die id-basierten globalen Funktionen
// (get_state/set_state/clicked), die auch mit ids funktionieren, die keine Lua-Namen sind.
std::string ArgId(lua_State* L, int index) {
    return std::string(luaL_checkstring(L, index));
}

// __index der Proxies: leitet die Lesezugriffe .state/.state_index/.states/.clicked an die
// ElementRegistry weiter. Alles andere (z.B. onclick oder eigene Felder) liegt direkt im
// Proxy-Table und kommt hier gar nicht erst an.
int ElementIndex(lua_State* L) {
    if (lua_type(L, 2) != LUA_TSTRING) {
        lua_pushnil(L);
        return 1;
    }

    ElementRegistry& registry = Registry(L);
    std::string id = ProxyId(L, 1);
    const char* key = lua_tostring(L, 2);

    if (std::strcmp(key, "state") == 0) {
        lua_pushstring(L, registry.GetStateName(id).c_str());
        return 1;
    }
    if (std::strcmp(key, "state_index") == 0) {
        lua_pushinteger(L, registry.GetStateIndex(id));
        return 1;
    }
    if (std::strcmp(key, "clicked") == 0) {
        lua_pushboolean(L, registry.WasClicked(id));
        return 1;
    }
    if (std::strcmp(key, "id") == 0) {
        lua_pushstring(L, id.c_str());
        return 1;
    }
    if (std::strcmp(key, "states") == 0) {
        const std::vector<std::string>& states = registry.StateNames(id);
        lua_createtable(L, static_cast<int>(states.size()), 0);
        for (size_t i = 0; i < states.size(); i++) {
            lua_pushstring(L, states[i].c_str());
            lua_rawseti(L, -2, static_cast<lua_Integer>(i) + 1);
        }
        return 1;
    }

    lua_pushnil(L);
    return 1;
}

// __newindex der Proxies: ".state = ..." und ".state_index = ..." sind echte Zustandswechsel
// in der Registry, jede andere Zuweisung (onclick, eigene Hilfsfelder) wird ganz normal im
// Proxy gespeichert. Die rein lesbaren Felder melden bei Zuweisung einen Fehler, statt sich
// stillschweigend als totes Feld im Proxy abzulegen - das wuerde ab dann den echten Wert
// verdecken (ein direkt im Table liegendes Feld erreicht dieses __newindex nie wieder).
int ElementNewIndex(lua_State* L) {
    if (lua_type(L, 2) == LUA_TSTRING) {
        ElementRegistry& registry = Registry(L);
        const char* key = lua_tostring(L, 2);

        if (std::strcmp(key, "state") == 0) {
            std::string id = ProxyId(L, 1);
            if (lua_type(L, 3) == LUA_TNUMBER) {
                registry.SetState(id, static_cast<int>(lua_tointeger(L, 3)));
            } else if (lua_type(L, 3) == LUA_TSTRING) {
                registry.SetState(id, std::string(lua_tostring(L, 3)));
            } else {
                return luaL_error(L, "%s.state braucht einen Zustandsnamen (Text) oder Index (Zahl)", id.c_str());
            }
            return 0;
        }

        if (std::strcmp(key, "state_index") == 0) {
            std::string id = ProxyId(L, 1);
            if (lua_type(L, 3) != LUA_TNUMBER) {
                return luaL_error(L, "%s.state_index braucht eine Zahl", id.c_str());
            }
            registry.SetState(id, static_cast<int>(lua_tointeger(L, 3)));
            return 0;
        }

        if (std::strcmp(key, "clicked") == 0 || std::strcmp(key, "states") == 0 ||
            std::strcmp(key, "id") == 0) {
            return luaL_error(L, "%s.%s ist nur lesbar", ProxyId(L, 1).c_str(), key);
        }
    }

    lua_rawset(L, 1);
    return 0;
}

// __index von _G: macht jede Element-id automatisch als globale Variable verfuegbar, ohne dass
// im Skript irgendetwas deklariert werden muesste. Der erzeugte Proxy wird gecacht, damit
// "notaus_taste" bei jedem Zugriff dasselbe Objekt ist (sonst waere ein einmal gesetztes
// onclick beim naechsten Frame wieder weg).
int GlobalIndex(lua_State* L) {
    if (lua_type(L, 2) != LUA_TSTRING) {
        lua_pushnil(L);
        return 1;
    }

    const char* name = lua_tostring(L, 2);
    if (!Registry(L).Exists(name)) {
        lua_pushnil(L);
        return 1;
    }

    lua_getfield(L, LUA_REGISTRYINDEX, kProxyCacheKey);
    lua_getfield(L, -1, name);
    if (!lua_isnil(L, -1)) {
        return 1;
    }
    lua_pop(L, 1);

    // Eigene Metatabelle pro Element: die beiden Zugriffs-Metamethoden werden aus der
    // gemeinsamen Vorlage uebernommen, dazu kommt nur die id dieses Elements.
    lua_newtable(L); // Proxy
    lua_newtable(L); // Metatabelle
    lua_getfield(L, LUA_REGISTRYINDEX, kProxyMetaKey);
    lua_getfield(L, -1, "__index");
    lua_setfield(L, -3, "__index");
    lua_getfield(L, -1, "__newindex");
    lua_setfield(L, -3, "__newindex");
    lua_pop(L, 1);
    lua_pushstring(L, name);
    lua_setfield(L, -2, kProxyIdField);
    lua_setmetatable(L, -2);

    lua_pushvalue(L, -1);
    lua_setfield(L, -3, name); // im Cache ablegen, Proxy bleibt oben liegen
    return 1;
}

// __newindex von _G: faengt genau den Fall ab, in dem eine eigene globale Variable denselben
// Namen wie ein Element bekommen soll. Eine solche Zuweisung wuerde das Element ab da
// unerreichbar machen (ein direkt in _G liegender Wert kommt nie mehr bei GlobalIndex an) -
// darum lieber eine klare Fehlermeldung statt eines stumm kaputten Elements. Jede andere
// Zuweisung wird ganz normal gespeichert.
int GlobalNewIndex(lua_State* L) {
    if (lua_type(L, 2) == LUA_TSTRING) {
        const char* name = lua_tostring(L, 2);
        if (Registry(L).Exists(name)) {
            return luaL_error(L, "'%s' ist bereits der Name eines Elements und kann nicht als eigene "
                                 "Variable verwendet werden", name);
        }
    }
    lua_rawset(L, 1);
    return 0;
}

int LuaGetState(lua_State* L) {
    lua_pushstring(L, Registry(L).GetStateName(ArgId(L, 1)).c_str());
    return 1;
}

int LuaSetState(lua_State* L) {
    ElementRegistry& registry = Registry(L);
    std::string id = ArgId(L, 1);
    if (lua_type(L, 2) == LUA_TNUMBER) {
        registry.SetState(id, static_cast<int>(lua_tointeger(L, 2)));
    } else {
        registry.SetState(id, std::string(luaL_checkstring(L, 2)));
    }
    return 0;
}

int LuaClicked(lua_State* L) {
    lua_pushboolean(L, Registry(L).WasClicked(ArgId(L, 1)));
    return 1;
}

int LuaElements(lua_State* L) {
    std::vector<std::string> ids = Registry(L).AllIds();
    std::sort(ids.begin(), ids.end());
    lua_createtable(L, static_cast<int>(ids.size()), 0);
    for (size_t i = 0; i < ids.size(); i++) {
        lua_pushstring(L, ids[i].c_str());
        lua_rawseti(L, -2, static_cast<lua_Integer>(i) + 1);
    }
    return 1;
}

int LuaTime(lua_State* L) {
    lua_pushnumber(L, GetTime());
    return 1;
}

// log(...) haengt alle Argumente durch tostring aneinander und schreibt sie ueber raylibs
// TraceLog - damit landen Skriptausgaben in derselben Konsolenausgabe wie der Rest des Spiels.
int LuaLog(lua_State* L) {
    std::string message;
    int argCount = lua_gettop(L);
    for (int i = 1; i <= argCount; i++) {
        if (i > 1) {
            message += "\t";
        }
        message += luaL_tolstring(L, i, nullptr);
        lua_pop(L, 1);
    }
    TraceLog(LOG_INFO, "SKRIPT: %s", message.c_str());
    return 0;
}

// Alle *.lua-Dateien direkt im Ordner (nicht rekursiv), alphabetisch sortiert - damit die
// Ausfuehrungsreihenfolge bei mehreren Dateien nicht vom Dateisystem abhaengt. Einzige
// Ausnahme ist die Variablen-Datei (GameConfig::ScriptVariablesFile): sie wird nach vorne
// gezogen und laeuft immer zuerst, damit die dort gesetzten Werte schon bereitstehen, wenn
// die uebrigen Skripte ausgefuehrt werden.
std::vector<std::string> ListScriptPaths(const std::string& dirPath) {
    std::vector<std::string> paths;
    if (dirPath.empty() || !DirectoryExists(dirPath.c_str())) {
        return paths;
    }

    FilePathList filePaths = LoadDirectoryFilesEx(dirPath.c_str(), ".lua", false);
    for (unsigned int i = 0; i < filePaths.count; i++) {
        paths.emplace_back(filePaths.paths[i]);
    }
    UnloadDirectoryFiles(filePaths);

    std::sort(paths.begin(), paths.end());

    auto variables = std::find_if(paths.begin(), paths.end(), [](const std::string& path) {
        return std::strcmp(GetFileName(path.c_str()), GameConfig::ScriptVariablesFile) == 0;
    });
    if (variables != paths.end()) {
        std::rotate(paths.begin(), variables, variables + 1);
    }

    return paths;
}

} // namespace

ScriptSystem::~ScriptSystem() {
    Shutdown();
}

void ScriptSystem::Load(const char* dirPath, ElementRegistry& elementRegistry) {
    directory = dirPath != nullptr ? dirPath : "";
    registry = &elementRegistry;
    Reload();
}

void ScriptSystem::Shutdown() {
    if (L != nullptr) {
        lua_close(L);
        L = nullptr;
    }
}

void ScriptSystem::Reload() {
    Shutdown();
    ClearError();

    files.clear();
    for (const std::string& path : ListScriptPaths(directory)) {
        files.push_back(ScriptFile{ path, GetFileModTime(path.c_str()) });
    }

    L = luaL_newstate();
    luaL_openlibs(L);

    // Von allen Proxies geteilte Metatabelle (__index/__newindex, siehe ElementIndex/
    // ElementNewIndex) und der Proxy-Cache, beide in der Lua-Registry statt global, damit sie
    // aus Skripten heraus nicht versehentlich ueberschrieben werden koennen.
    lua_newtable(L);
    lua_pushlightuserdata(L, registry);
    lua_pushcclosure(L, ElementIndex, 1);
    lua_setfield(L, -2, "__index");
    lua_pushlightuserdata(L, registry);
    lua_pushcclosure(L, ElementNewIndex, 1);
    lua_setfield(L, -2, "__newindex");
    lua_setfield(L, LUA_REGISTRYINDEX, kProxyMetaKey);

    lua_newtable(L);
    lua_setfield(L, LUA_REGISTRYINDEX, kProxyCacheKey);

    const struct { const char* name; lua_CFunction fn; } functions[] = {
        { "get_state", LuaGetState },
        { "set_state", LuaSetState },
        { "clicked",   LuaClicked },
        { "elements",  LuaElements },
        { "time",      LuaTime },
        { "log",       LuaLog },
    };
    for (const auto& entry : functions) {
        lua_pushlightuserdata(L, registry);
        lua_pushcclosure(L, entry.fn, 1);
        lua_setglobal(L, entry.name);
    }

    if (luaL_dostring(L, kPrelude) != LUA_OK) {
        SetError(std::string("Interner Skriptfehler: ") + lua_tostring(L, -1));
        lua_pop(L, 1);
        return;
    }

    // _G bekommt ein __index, das unbekannte Namen als Element-ids interpretiert, und ein
    // __newindex, das eigene Variablen mit Elementnamen abweist (siehe GlobalNewIndex). Beides
    // wird bewusst erst hier gesetzt, also nachdem die eingebauten Funktionen und der Prelude
    // ihre Globals angelegt haben: sonst wuerde eine Element-id, die zufaellig "log" oder
    // "every" heisst, den Fehler von GlobalNewIndex ausserhalb eines pcall ausloesen.
    lua_pushglobaltable(L);
    lua_newtable(L);
    lua_pushlightuserdata(L, registry);
    lua_pushcclosure(L, GlobalIndex, 1);
    lua_setfield(L, -2, "__index");
    lua_pushlightuserdata(L, registry);
    lua_pushcclosure(L, GlobalNewIndex, 1);
    lua_setfield(L, -2, "__newindex");
    lua_setmetatable(L, -2);
    lua_pop(L, 1);

    for (const ScriptFile& file : files) {
        if (luaL_dofile(L, file.path.c_str()) != LUA_OK) {
            SetError(lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }

    TraceLog(LOG_INFO, "SKRIPT: %d Datei(en) aus %s geladen", static_cast<int>(files.size()), directory.c_str());

    if (BeginCall("start")) {
        EndCall(0);
    }
}

void ScriptSystem::Update(float deltaTime) {
    reloadCheckTimer += deltaTime;
    if (reloadCheckTimer >= kReloadCheckInterval) {
        reloadCheckTimer = 0.0f;

        // Neu laden, sobald sich irgendeine Datei geaendert hat ODER die Dateiliste selbst
        // eine andere ist (neu angelegte/geloeschte Skripte).
        std::vector<std::string> currentPaths = ListScriptPaths(directory);
        bool changed = currentPaths.size() != files.size();
        for (size_t i = 0; !changed && i < currentPaths.size(); i++) {
            changed = currentPaths[i] != files[i].path ||
                      GetFileModTime(currentPaths[i].c_str()) != files[i].modTime;
        }
        if (changed) {
            Reload();
        }
    }

    if (L == nullptr || registry == nullptr) {
        return;
    }

    if (BeginCall("__frame")) {
        lua_pushnumber(L, deltaTime);
        lua_pushstring(L, registry->FrameClickedId().c_str());
        EndCall(2);
    }
}

bool ScriptSystem::BeginCall(const char* name) {
    if (L == nullptr) {
        return false;
    }
    lua_getglobal(L, name);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        return false;
    }
    return true;
}

void ScriptSystem::EndCall(int argCount) {
    if (lua_pcall(L, argCount, 0, 0) != LUA_OK) {
        SetError(lua_tostring(L, -1));
        lua_pop(L, 1);
        return;
    }
    ClearError();
}

void ScriptSystem::SetError(const std::string& message) {
    // Nur bei Aenderung loggen: ein Fehler in tick() wuerde sonst 60x pro Sekunde in der
    // Konsole stehen.
    if (message != lastError) {
        TraceLog(LOG_WARNING, "SKRIPT: %s", message.c_str());
    }
    lastError = message;
}

void ScriptSystem::ClearError() {
    lastError.clear();
}
