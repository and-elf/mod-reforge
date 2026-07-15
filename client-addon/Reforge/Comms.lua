--[[
  Reforge -- Comms layer.

  Two channels, both over the stock 3.3.5a LANG_ADDON path (no client patch):

  * INBOUND state (server -> client): RFRG frames pushed as LANG_ADDON whispers. Decoded here into
    ns.state and fired to UI callbacks. Mirrors the pure server codec
    modules/mod-reforge/src/core/reforge/Protocol.cpp -- same grammar: tab fields, ';' records,
    ':' sub-fields; the reforge cap crosses as permille (x1000).

  * OUTBOUND requests (client -> server): the built-in AzerothCore addon command channel. We issue
    the `.reforge …` chat commands (prefix "AzerothCore", opcode 'i') and read back the m/o/f/a
    replies. So the client never needs a bespoke server opcode; the command surface IS the API.
]]

local ADDON, ns = ...

ns.PROTOCOL = 1
ns.PREFIX = "RFRG"
ns.CMD_PREFIX = "AzerothCore"   -- AzerothCore addon command channel

-- Latest decoded state. UI reads these; empty until the first push.
ns.state = {
    hello = nil,                 -- { version, enabled }
    config = { maxFraction = 0.40 },
    currencies = {},             -- { { entry, count }, … } in server order
    items = {},                  -- [serverSlot] = { from, to, amount }
}

-- Stat ordinals -> names (mirror Reforge::ItemStat / ReforgeStatMap::StatName).
ns.StatName = {
    [0] = "Stamina", [1] = "Strength", [2] = "Agility", [3] = "Intellect", [4] = "Spirit",
    [5] = "Attack Power", [6] = "Spell Power", [7] = "Hit", [8] = "Crit", [9] = "Haste",
    [10] = "Expertise", [11] = "Armor Pen", [12] = "MP5", [13] = "Defense", [14] = "Dodge",
    [15] = "Parry", [16] = "Block", [17] = "Resilience",
}
-- The command token the server parses for each stat ordinal (ReforgeStatMap::StatName).
ns.StatToken = {
    [0] = "stamina", [1] = "strength", [2] = "agility", [3] = "intellect", [4] = "spirit",
    [5] = "attackpower", [6] = "spellpower", [7] = "hit", [8] = "crit", [9] = "haste",
    [10] = "expertise", [11] = "armorpen", [12] = "mp5", [13] = "defense", [14] = "dodge",
    [15] = "parry", [16] = "block", [17] = "resilience",
}

-- ---- Tiny callback registry: ns:On("event", fn); fired on each decode ----
ns.callbacks = {}
function ns:On(evt, fn)
    self.callbacks[evt] = self.callbacks[evt] or {}
    table.insert(self.callbacks[evt], fn)
end
function ns:Fire(evt)
    local list = self.callbacks[evt]
    if not list then return end
    for _, fn in ipairs(list) do
        local ok, err = pcall(fn)
        if not ok then
            DEFAULT_CHAT_FRAME:AddMessage("|cffff5555Reforge error:|r " .. tostring(err))
        end
    end
end

-- ---- Parse helpers ----
local function split(s, sep)
    local out, idx = {}, 1
    while true do
        local p = string.find(s, sep, idx, true)
        if not p then out[#out + 1] = string.sub(s, idx); break end
        out[#out + 1] = string.sub(s, idx, p - 1)
        idx = p + 1
    end
    return out
end
local function num(s) return tonumber(s) or 0 end

-- `message` is the frame body with the RFRG prefix already stripped by the client (starts at KIND).
local function decode(message)
    local t = split(message, "\t")
    local kind = t[1]

    if kind == "HELLO" then
        ns.state.hello = { version = num(t[2]), enabled = (t[3] == "1") }
        if ns.state.hello.version ~= ns.PROTOCOL then
            DEFAULT_CHAT_FRAME:AddMessage("|cffffcc00Reforge:|r server protocol v" .. ns.state.hello.version
                .. " != addon v" .. ns.PROTOCOL .. " -- please update the addon.")
        end
        ns:Fire("hello")
    elseif kind == "CFG" then
        ns.state.config.maxFraction = num(t[2]) / 1000
        ns:Fire("config")
    elseif kind == "CUR" then
        local list = {}
        if t[2] and t[2] ~= "" then
            for _, rec in ipairs(split(t[2], ";")) do
                local f = split(rec, ":")
                list[#list + 1] = { entry = num(f[1]), count = num(f[2]) }
            end
        end
        ns.state.currencies = list
        ns:Fire("currencies")
    elseif kind == "ITEM" then
        local items = {}
        if t[2] and t[2] ~= "" then
            for _, rec in ipairs(split(t[2], ";")) do
                local f = split(rec, ":")
                items[num(f[1])] = { from = num(f[2]), to = num(f[3]), amount = num(f[4]) }
            end
        end
        ns.state.items = items
        ns:Fire("items")
    end
end

-- ---- Client -> server: AzerothCore addon command channel ----
local cmdCounter = 0
local function nextCounter()
    cmdCounter = (cmdCounter + 1) % 10000
    return string.format("%04d", cmdCounter)
end

-- Issue a server chat command (no leading dot) over the addon command channel.
function ns:SendCommand(command)
    SendAddonMessage(ns.CMD_PREFIX, "i" .. nextCounter() .. command, "WHISPER", UnitName("player"))
end

-- Submit a reforge. serverSlot is the EQUIPMENT_SLOT index (client INVSLOT - 1).
function ns:Reforge(serverSlot, fromOrdinal, toOrdinal, amount, currencyEntry)
    local from = ns.StatToken[fromOrdinal]
    local to = ns.StatToken[toOrdinal]
    if not from or not to then return end
    ns:SendCommand(string.format("reforge do %d %s %s %d %d", serverSlot, from, to, amount, currencyEntry or 0))
end

function ns:ClearReforge(serverSlot)
    ns:SendCommand(string.format("reforge clear %d", serverSlot))
end

function ns:Sync()
    ns:SendCommand("reforge sync")
end

-- ---- Event wiring ----
local frame = CreateFrame("Frame")
frame:RegisterEvent("CHAT_MSG_ADDON")
frame:RegisterEvent("PLAYER_LOGIN")
frame:SetScript("OnEvent", function(_, event, ...)
    if event == "CHAT_MSG_ADDON" then
        local prefix, msg = ...
        if prefix == ns.PREFIX and msg then
            decode(msg)
        elseif prefix == ns.CMD_PREFIX and msg then
            -- Command reply: 'm' carries a human-readable body (the reforge result message).
            local opcode = string.sub(msg, 1, 1)
            if opcode == "m" then
                DEFAULT_CHAT_FRAME:AddMessage("|cff66ccffReforge:|r " .. string.sub(msg, 6))
            end
        end
    elseif event == "PLAYER_LOGIN" then
        ReforgeDB = ReforgeDB or {}
        ns.db = ReforgeDB
        if RegisterAddonMessagePrefix then
            RegisterAddonMessagePrefix(ns.PREFIX)
            RegisterAddonMessagePrefix(ns.CMD_PREFIX)
        end
        if ns.CreatePanel then ns.CreatePanel() end
        ns:Sync()   -- ask the server to (re)push our state
    end
end)

-- ---- Slash command ----
SLASH_REFORGE1 = "/reforge"
SlashCmdList["REFORGE"] = function()
    if ns.TogglePanel then ns.TogglePanel() end
end
