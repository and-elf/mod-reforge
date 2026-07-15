--[[
  Reforge -- UI panel (/reforge).

  Pick an equipped item, a stat to reduce (from), a stat to gain (to), an amount bounded by the
  server's reforge cap, and a currency from the server's accepted list (the "any currency" choice).
  Submits `.reforge do …` over the AzerothCore addon command channel (see Comms.lua). Reads
  ns.state, which the server pushes and refreshes on every reforge.
]]

local ADDON, ns = ...

local panel
local sel = { slot = nil, from = 7, to = 8, currencyIndex = 1 }   -- default hit -> crit

-- Money helper: copper -> "Xg Ys Zc".
local function money(copper)
    return string.format("%dg %ds %dc", math.floor(copper / 10000), math.floor((copper % 10000) / 100), copper % 100)
end

-- Equipped items as { serverSlot, label }, scanning client INVSLOT 1..19 (serverSlot = invSlot - 1).
local function equippedItems()
    local out = {}
    for invSlot = 1, 19 do
        local link = GetInventoryItemLink("player", invSlot)
        if link then
            out[#out + 1] = { serverSlot = invSlot - 1, label = link }
        end
    end
    return out
end

local function currencyLabel(c)
    if not c then return "(none)" end
    if c.entry == 0 then return "Pay " .. money(c.count) end
    local name = GetItemInfo(c.entry) or ("item #" .. c.entry)
    return string.format("Pay %dx %s", c.count, name)
end

-- ---- Dropdown builders (3.3.5a UIDropDownMenu) ----
local function buildStatDropdown(dd, getter, setter)
    UIDropDownMenu_Initialize(dd, function()
        for ordinal = 0, 17 do
            local info = UIDropDownMenu_CreateInfo()
            info.text = ns.StatName[ordinal]
            info.func = function()
                setter(ordinal)
                UIDropDownMenu_SetSelectedValue(dd, ordinal)
                UIDropDownMenu_SetText(dd, ns.StatName[ordinal])
                if ns.RefreshPanel then ns.RefreshPanel() end
            end
            info.checked = (getter() == ordinal)
            UIDropDownMenu_AddButton(info)
        end
    end)
    UIDropDownMenu_SetSelectedValue(dd, getter())
    UIDropDownMenu_SetText(dd, ns.StatName[getter()])
end

local function buildCurrencyDropdown(dd)
    UIDropDownMenu_Initialize(dd, function()
        local list = ns.state.currencies or {}
        for i, c in ipairs(list) do
            local info = UIDropDownMenu_CreateInfo()
            info.text = currencyLabel(c)
            info.func = function()
                sel.currencyIndex = i
                UIDropDownMenu_SetSelectedValue(dd, i)
                UIDropDownMenu_SetText(dd, currencyLabel(c))
            end
            info.checked = (sel.currencyIndex == i)
            UIDropDownMenu_AddButton(info)
        end
    end)
    local list = ns.state.currencies or {}
    if sel.currencyIndex > #list then sel.currencyIndex = 1 end
    UIDropDownMenu_SetSelectedValue(dd, sel.currencyIndex)
    UIDropDownMenu_SetText(dd, currencyLabel(list[sel.currencyIndex]))
end

local function buildItemDropdown(dd)
    UIDropDownMenu_Initialize(dd, function()
        for _, it in ipairs(equippedItems()) do
            local info = UIDropDownMenu_CreateInfo()
            info.text = it.label
            info.func = function()
                sel.slot = it.serverSlot
                UIDropDownMenu_SetSelectedValue(dd, it.serverSlot)
                UIDropDownMenu_SetText(dd, it.label)
                if ns.RefreshPanel then ns.RefreshPanel() end
            end
            info.checked = (sel.slot == it.serverSlot)
            UIDropDownMenu_AddButton(info)
        end
    end)
    if sel.slot ~= nil then UIDropDownMenu_SetSelectedValue(dd, sel.slot) end
end

function ns.CreatePanel()
    if panel then return end

    panel = CreateFrame("Frame", "ReforgePanel", UIParent)
    panel:SetSize(360, 320)
    panel:SetPoint("CENTER")
    panel:SetMovable(true)
    panel:EnableMouse(true)
    panel:RegisterForDrag("LeftButton")
    panel:SetScript("OnDragStart", panel.StartMoving)
    panel:SetScript("OnDragStop", panel.StopMovingOrSizing)
    panel:SetBackdrop({
        bgFile = "Interface/Tooltips/UI-Tooltip-Background",
        edgeFile = "Interface/Tooltips/UI-Tooltip-Border",
        tile = true, tileSize = 16, edgeSize = 16,
        insets = { left = 4, right = 4, top = 4, bottom = 4 },
    })
    panel:SetBackdropColor(0, 0, 0, 0.9)
    panel:Hide()

    local title = panel:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    title:SetPoint("TOP", 0, -12)
    title:SetText("Reforge")

    local close = CreateFrame("Button", nil, panel, "UIPanelCloseButton")
    close:SetPoint("TOPRIGHT", 0, 0)

    local function label(text, anchor, y)
        local fs = panel:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
        fs:SetPoint("TOPLEFT", anchor, "TOPLEFT", 12, y)
        fs:SetText(text)
        return fs
    end

    label("Item", panel, -34)
    local itemDD = CreateFrame("Frame", "ReforgeItemDD", panel, "UIDropDownMenuTemplate")
    itemDD:SetPoint("TOPLEFT", 0, -50)
    UIDropDownMenu_SetWidth(itemDD, 300)

    label("Reduce", panel, -84)
    local fromDD = CreateFrame("Frame", "ReforgeFromDD", panel, "UIDropDownMenuTemplate")
    fromDD:SetPoint("TOPLEFT", 0, -100)
    UIDropDownMenu_SetWidth(fromDD, 120)

    label("Into", panel, -84)
    local toDD = CreateFrame("Frame", "ReforgeToDD", panel, "UIDropDownMenuTemplate")
    toDD:SetPoint("TOPLEFT", 160, -100)
    UIDropDownMenu_SetWidth(toDD, 120)

    label("Amount", panel, -134)
    local amountBox = CreateFrame("EditBox", "ReforgeAmountBox", panel, "InputBoxTemplate")
    amountBox:SetSize(80, 20)
    amountBox:SetPoint("TOPLEFT", 60, -138)
    amountBox:SetAutoFocus(false)
    amountBox:SetNumeric(true)
    amountBox:SetText("0")

    local capText = panel:CreateFontString(nil, "OVERLAY", "GameFontDisableSmall")
    capText:SetPoint("LEFT", amountBox, "RIGHT", 8, 0)

    label("Currency", panel, -168)
    local curDD = CreateFrame("Frame", "ReforgeCurrencyDD", panel, "UIDropDownMenuTemplate")
    curDD:SetPoint("TOPLEFT", 0, -184)
    UIDropDownMenu_SetWidth(curDD, 300)

    local status = panel:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    status:SetPoint("TOPLEFT", 14, -226)
    status:SetPoint("TOPRIGHT", -14, -226)
    status:SetJustifyH("LEFT")

    local reforgeBtn = CreateFrame("Button", nil, panel, "UIPanelButtonTemplate")
    reforgeBtn:SetSize(120, 24)
    reforgeBtn:SetPoint("BOTTOMLEFT", 16, 16)
    reforgeBtn:SetText("Reforge")
    reforgeBtn:SetScript("OnClick", function()
        if sel.slot == nil then status:SetText("|cffff8888Pick an item first.|r"); return end
        local list = ns.state.currencies or {}
        local currency = list[sel.currencyIndex]
        ns:Reforge(sel.slot, sel.from, sel.to, tonumber(amountBox:GetText()) or 0, currency and currency.entry or 0)
    end)

    local clearBtn = CreateFrame("Button", nil, panel, "UIPanelButtonTemplate")
    clearBtn:SetSize(120, 24)
    clearBtn:SetPoint("BOTTOMRIGHT", -16, 16)
    clearBtn:SetText("Remove reforge")
    clearBtn:SetScript("OnClick", function()
        if sel.slot == nil then status:SetText("|cffff8888Pick an item first.|r"); return end
        ns:ClearReforge(sel.slot)
    end)

    -- Refresh: rebuild dropdowns + the cap hint + current-reforge status from ns.state.
    function ns.RefreshPanel()
        if not panel:IsShown() then return end
        buildItemDropdown(itemDD)
        buildStatDropdown(fromDD, function() return sel.from end, function(v) sel.from = v end)
        buildStatDropdown(toDD, function() return sel.to end, function(v) sel.to = v end)
        buildCurrencyDropdown(curDD)

        local frac = (ns.state.config and ns.state.config.maxFraction) or 0.40
        capText:SetText(string.format("max %d%% of the stat", math.floor(frac * 100 + 0.5)))

        if sel.slot ~= nil and ns.state.items[sel.slot] then
            local r = ns.state.items[sel.slot]
            status:SetText(string.format("|cff66ccffCurrent:|r %d %s -> %s", r.amount,
                ns.StatName[r.from] or "?", ns.StatName[r.to] or "?"))
        elseif sel.slot ~= nil then
            status:SetText("Not reforged.")
        else
            status:SetText("Pick an equipped item.")
        end
    end

    -- Re-render when fresh state arrives.
    ns:On("items", ns.RefreshPanel)
    ns:On("currencies", ns.RefreshPanel)
    ns:On("config", ns.RefreshPanel)

    panel:SetScript("OnShow", function() ns:Sync(); ns.RefreshPanel() end)
end

function ns.TogglePanel()
    if not panel then ns.CreatePanel() end
    if panel:IsShown() then panel:Hide() else panel:Show() end
end
