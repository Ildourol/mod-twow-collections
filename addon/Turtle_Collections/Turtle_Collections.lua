-- ============================================================================
-- Turtle Collections: Account-Wide Vanity Collection Client Interface
-- Strict Lua 5.0 Architecture (Turtle WoW 1.18.1 / Vanilla 1.12.1)
-- Account-Wide Mounts, Pets, and Toys Browser & Summoner
-- ============================================================================

local _table_insert = table.insert
local tinsert = function(t, a, b)
    if b ~= nil then _table_insert(t, a, b)
    else _table_insert(t, table.getn(t) + 1, a) end
end

-- Initialize SavedVariables
if not TurtleCollectionsDB then
    TurtleCollectionsDB = {
        mounts = {},
        pets = {},
        toys = {},
        minimapPos = 210,
        minimapVisible = true,
        lastSync = 0
    }
end

local TC = {
    activeTab = 1, -- 1: Mounts, 2: Pets, 3: Toys
    currentPage = 1,
    itemsPerPage = 10,
    searchFilter = "",
    displayList = {},
    rawEntries = {},
    knownSpells = {},
    lastCommandTime = 0,
    suppressCommandError = false,
    lastSync = 0
}

-- Register frame for standard ESC key closing
tinsert(UISpecialFrames, "TurtleCollectionsFrame")

-- -----------------------------------------------------------------------------
-- 1. Silent Chat Filter for .collection whispers and system errors
-- -----------------------------------------------------------------------------
local origChatFrame_OnEvent = ChatFrame_OnEvent
ChatFrame_OnEvent = function(event)
    if (event == "CHAT_MSG_WHISPER_INFORM" or event == "CHAT_MSG_WHISPER") and type(arg1) == "string" and string.find(arg1, "^%.collection%s*") then
        return
    end
    if event == "CHAT_MSG_SYSTEM" and type(arg1) == "string" and TC.suppressCommandError then
        local now = GetTime()
        if (now - (TC.lastCommandTime or 0)) < 2.0 then
            local lower = string.lower(arg1)
            if string.find(lower, "no such command") or string.find(lower, "cette commande") or string.find(lower, "unknown command") then
                return
            end
        end
    end
    origChatFrame_OnEvent(event)
end

for i = 1, 7 do
    local cf = getglobal("ChatFrame" .. i)
    if cf and cf.AddMessage then
        local origAdd = cf.AddMessage
        cf.AddMessage = function(this, msg, r, g, b, id)
            if type(msg) == "string" then
                if string.find(msg, "^%.collection%s*") then
                    return
                end
                if TC.suppressCommandError then
                    local now = GetTime()
                    if (now - (TC.lastCommandTime or 0)) < 2.0 then
                        local lower = string.lower(msg)
                        if string.find(lower, "no such command") or string.find(lower, "cette commande") or string.find(lower, "unknown command") then
                            return
                        end
                    end
                end
            end
            origAdd(this, msg, r, g, b, id)
        end
    end
end

-- -----------------------------------------------------------------------------
-- 2. Client-to-Server Communication Helper
-- In Vanilla 1.12.1 / Turtle WoW 1.18.1, SendAddonMessage only supports
-- PARTY, RAID, GUILD, BATTLEGROUND. Passing "WHISPER" throws "unknown addon chat type".
-- Self-whispers to .collection work reliably in solo, party, and guild contexts.
-- -----------------------------------------------------------------------------
local function SendServerCommand(payload)
    if SendChatMessage then
        TC.lastCommandTime = GetTime()
        TC.suppressCommandError = true
        SendChatMessage(".collection " .. payload, "WHISPER", nil, UnitName("player"))
    end
end

local function RequestCollectionSync()
    TC.lastSync = GetTime()
    SendServerCommand("query " .. TC.activeTab)
end

local function HasRidingSkill()
    local numSkills = (GetNumSkillLines and GetNumSkillLines()) or 0
    for i = 1, numSkills do
        local skillName, _, _, skillRank = GetSkillLineInfo(i)
        if skillName and string.find(string.lower(skillName), "riding") then
            return (skillRank and skillRank > 0)
        end
    end
    if TC.knownSpells then
        if TC.knownSpells["Summon Warhorse"] or TC.knownSpells["Summon Charger"] or
           TC.knownSpells["Summon Felsteed"] or TC.knownSpells["Summon Dreadsteed"] then
            return true
        end
    end
    return false
end

local function GetMountRequiredLevel(item)
    if not item then return 40 end
    if TC_CATALOG and item.spellId and TC_CATALOG[item.spellId] and TC_CATALOG[item.spellId].level then
        return TC_CATALOG[item.spellId].level
    end
    if item.spellId == 30174 then -- Swift Riding Turtle
        return 18
    end
    local lowerName = string.lower(item.name or "")
    if string.find(lowerName, "swift") or string.find(lowerName, "armored") or string.find(lowerName, "deathcharger") or string.find(lowerName, "great") then
        return 60
    end
    return 40
end

local function SummonCollectionItem(entry)
    if not entry then return end

    -- If known in spellbook, cast by name
    if entry.name and TC.knownSpells and TC.knownSpells[entry.name] then
        CastSpellByName(entry.name)
        return
    end

    -- Otherwise send summon request to server
    if entry.spellId and entry.spellId > 0 then
        SendServerCommand("summon " .. entry.spellId)
    end
end

-- -----------------------------------------------------------------------------
-- 3. Safe Spell and Item Info Resolver
-- -----------------------------------------------------------------------------
local function GetSafeItemOrSpellInfo(itemId, spellId)
    local name, icon = nil, nil

    -- 1. Query client item cache
    if itemId and itemId > 0 and GetItemInfo then
        name, _, _, _, _, _, _, _, icon = GetItemInfo(itemId)
    end

    -- 2. Lookup in embedded catalog by spellId
    if (not name or not icon) and TC_CATALOG and spellId and TC_CATALOG[spellId] then
        if not name then
            name = TC_CATALOG[spellId].name
        end
        if not icon and TC_CATALOG[spellId].icon then
            icon = TC_CATALOG[spellId].icon
        end
    end

    -- 3. Lookup in embedded catalog by itemId
    if (not name or not icon) and TC_CATALOG_BY_ITEM and itemId and TC_CATALOG_BY_ITEM[itemId] then
        if not name then
            name = TC_CATALOG_BY_ITEM[itemId].name
        end
        if not icon and TC_CATALOG_BY_ITEM[itemId].icon then
            icon = TC_CATALOG_BY_ITEM[itemId].icon
        end
    end

    -- 4. Fallback name
    if not name then
        name = "Collection #" .. (spellId or itemId or 0)
    end

    -- 5. Category-appropriate fallback icons
    if not icon then
        local cat = (TC_CATALOG and spellId and TC_CATALOG[spellId] and TC_CATALOG[spellId].type) or TC.activeTab
        if cat == 1 then
            icon = "Interface\\Icons\\Ability_Mount_MechaStrider"
        elseif cat == 2 then
            icon = "Interface\\Icons\\Spell_Shadow_SummonFelHunter"
        else
            icon = "Interface\\Icons\\INV_Box_01"
        end
    end

    if icon and not string.find(string.lower(icon), "^interface") then
        icon = "Interface\\Icons\\" .. icon
    end

    return name, icon
end

-- -----------------------------------------------------------------------------
-- 4. Wire Protocol Token Parser
-- -----------------------------------------------------------------------------
local function ParseCollectionToken(token)
    -- format: type:itemId:spellId
    local s, e, cType, itemId, spellId = string.find(token, "(%d+):(%d+):(%d+)")
    if cType and itemId and spellId then
        return tonumber(cType), tonumber(itemId), tonumber(spellId)
    end
    return nil, nil, nil
end

local function FilterEntries()
    TC.displayList = {}
    local entries = TC.rawEntries
    local query = string.lower(TC.searchFilter)

    for i = 1, table.getn(entries) do
        local entry = entries[i]
        if entry.type == TC.activeTab then
            local spellName = entry.name or ("Spell #" .. entry.spellId)
            if query == "" or string.find(string.lower(spellName), query, 1, true) then
                tinsert(TC.displayList, entry)
            end
        end
    end

    local total = table.getn(TC.displayList)
    local maxPages = math.floor((total + TC.itemsPerPage - 1) / TC.itemsPerPage)
    if maxPages < 1 then maxPages = 1 end
    if TC.currentPage > maxPages then TC.currentPage = maxPages end
end

local function UpdateUI()
    if not TurtleCollectionsFrame or not TurtleCollectionsFrame:IsVisible() then
        return
    end

    FilterEntries()

    local total = table.getn(TC.displayList)
    local maxPages = math.floor((total + TC.itemsPerPage - 1) / TC.itemsPerPage)
    if maxPages < 1 then maxPages = 1 end

    local startIndex = (TC.currentPage - 1) * TC.itemsPerPage + 1
    local tabTitle = "Mounts"
    if TC.activeTab == 2 then tabTitle = "Companion Pets"
    elseif TC.activeTab == 3 then tabTitle = "Novelty Toys" end

    TurtleCollectionsFrameTitle:SetText("Collections - " .. tabTitle .. " (" .. total .. ")")
    TurtleCollectionsFramePageText:SetText("Page " .. TC.currentPage .. " / " .. maxPages)

    for i = 1, TC.itemsPerPage do
        local row = getglobal("TurtleCollectionsRow" .. i)
        local idx = startIndex + i - 1

        if idx <= total then
            local item = TC.displayList[idx]
            row.entry = item

            local nameText = getglobal("TurtleCollectionsRow" .. i .. "Name")
            local iconTex = getglobal("TurtleCollectionsRow" .. i .. "Icon")
            local actionBtn = getglobal("TurtleCollectionsRow" .. i .. "Action")
            local statusText = getglobal("TurtleCollectionsRow" .. i .. "Status")

            -- Refresh name/icon dynamically if item was previously uncached
            if not item.name or string.find(item.name, "^Item #") or not item.icon or string.find(item.icon, "INV_Misc_QuestionMark") then
                local resolvedName, resolvedIcon = GetSafeItemOrSpellInfo(item.itemId, item.spellId)
                item.name = resolvedName
                item.icon = resolvedIcon
            end

            nameText:SetText(item.name)
            iconTex:SetTexture(item.icon or "Interface\\Icons\\INV_Misc_QuestionMark")

            if item.type == 1 then -- Mount
                local charLevel = UnitLevel("player")
                local reqLevel = GetMountRequiredLevel(item)
                local hasRiding = HasRidingSkill()
                if item.spellId == 30174 then -- Swift Riding Turtle
                    hasRiding = true
                end
                local isKnown = TC.knownSpells and TC.knownSpells[item.name]
                local canUse = (charLevel >= reqLevel) and hasRiding

                if actionBtn then
                    actionBtn:Show()
                    if canUse then
                        actionBtn:Enable()
                        actionBtn:SetText("Use")
                    else
                        actionBtn:Disable()
                        actionBtn:SetText("Use")
                    end
                end

                if charLevel < reqLevel then
                    statusText:SetText("|cffff8000Req Lvl " .. reqLevel .. "|r")
                elseif not hasRiding then
                    statusText:SetText("|cffff8000Req Riding|r")
                elseif isKnown then
                    statusText:SetText("|cff00ff00Learned|r")
                else
                    statusText:SetText("|cff00ff00Unlocked|r")
                end
            elseif item.type == 2 then -- Pet
                if actionBtn then
                    actionBtn:Show()
                    actionBtn:Enable()
                    actionBtn:SetText("Summon")
                end
                statusText:SetText("|cff00ff00Unlocked|r")
            elseif item.type == 3 then -- Toy
                if actionBtn then
                    actionBtn:Show()
                    actionBtn:Enable()
                    actionBtn:SetText("Use")
                end
                statusText:SetText("|cff00ff00Unlocked|r")
            end
            row:Show()
        else
            row:Hide()
        end
    end
end

-- -----------------------------------------------------------------------------
-- 5. Minimap Button (31x31 Standard Circular Minimap Frame)
-- -----------------------------------------------------------------------------
local minimapButton = CreateFrame("Button", "TurtleCollectionsMinimapButton", Minimap)
minimapButton:SetFrameStrata("MEDIUM")
minimapButton:SetFrameLevel(8)
minimapButton:SetWidth(31)
minimapButton:SetHeight(31)
minimapButton:RegisterForClicks("LeftButtonUp", "RightButtonUp")
minimapButton:RegisterForDrag("LeftButton")

local mmOverlay = minimapButton:CreateTexture(nil, "OVERLAY")
mmOverlay:SetWidth(53)
mmOverlay:SetHeight(53)
mmOverlay:SetTexture("Interface\\Minimap\\MiniMap-TrackingBorder")
mmOverlay:SetPoint("TOPLEFT", minimapButton, "TOPLEFT", 0, 0)

local mmIcon = minimapButton:CreateTexture(nil, "BACKGROUND")
mmIcon:SetWidth(20)
mmIcon:SetHeight(20)
mmIcon:SetTexture("Interface\\Icons\\INV_Box_01") -- Iconic Vanity Toybox / Collection Chest
mmIcon:SetPoint("CENTER", minimapButton, "CENTER", 0, 1)
mmIcon:SetTexCoord(0.07, 0.93, 0.07, 0.93)

local function UpdateMinimapButtonPosition()
    local pos = 210
    if TurtleCollectionsDB and TurtleCollectionsDB.minimapPos then
        pos = TurtleCollectionsDB.minimapPos
    end
    local angle = math.rad(pos)
    minimapButton:ClearAllPoints()
    minimapButton:SetPoint("CENTER", Minimap, "CENTER", 80 * math.cos(angle), 80 * math.sin(angle))
end

minimapButton:SetScript("OnDragStart", function()
    this:SetScript("OnUpdate", function()
        local mx, my = Minimap:GetCenter()
        local scale = Minimap:GetEffectiveScale()
        local px, py = GetCursorPosition()
        px, py = px / scale, py / scale
        TurtleCollectionsDB.minimapPos = math.deg(math.atan2(py - my, px - mx))
        UpdateMinimapButtonPosition()
    end)
end)

minimapButton:SetScript("OnDragStop", function()
    this:SetScript("OnUpdate", nil)
end)

minimapButton:SetScript("OnClick", function()
    TurtleCollections_Toggle()
end)

minimapButton:SetScript("OnEnter", function()
    GameTooltip:SetOwner(this, "ANCHOR_LEFT")
    GameTooltip:AddLine("Turtle Collections")
    GameTooltip:AddLine("Account-Wide Toybox, Mounts & Pets", 0.7, 0.7, 0.7)
    GameTooltip:AddLine("Left-Click: Toggle Collections Window", 1, 1, 1)
    GameTooltip:AddLine("Drag: Reposition Minimap Button", 1, 1, 1)
    GameTooltip:Show()
end)

minimapButton:SetScript("OnLeave", function()
    GameTooltip:Hide()
end)

-- -----------------------------------------------------------------------------
-- 6. Local Collections Scanner (Spellbook & Bags for Mounts, Pets, and Toys)
-- -----------------------------------------------------------------------------
local function ScanLocalCollections()
    TC.knownSpells = {}
    -- 1. Scan spellbook
    local total = 0
    local numTabs = (GetNumSpellTabs and GetNumSpellTabs()) or 0
    for t = 1, numTabs do
        local _, _, offset, numSpells = GetSpellTabInfo(t)
        if offset and numSpells then
            local endIdx = offset + numSpells
            if endIdx > total then
                total = endIdx
            end
        end
    end

    if total > 0 then
        for i = 1, total do
            local spellName, spellRank = GetSpellName(i, BOOKTYPE_SPELL)
            local spellIcon = GetSpellTexture(i, BOOKTYPE_SPELL)
            if spellName then
                TC.knownSpells[spellName] = true
            end
            if spellName and spellIcon then
                local isMount = string.find(spellIcon, "Ability_Mount") or string.find(spellIcon, "Spell_Nature_Swiftness") or string.find(spellIcon, "Spell_Fire_SoulBurn")
                local isPet = string.find(spellIcon, "INV_Box") or string.find(spellIcon, "Spell_Shadow_Summon") or string.find(spellIcon, "Ability_Hunter_Pet")
                local matchedType = nil
                local matchedItemId = 0
                local matchedSpellId = 0
                local matchedIcon = spellIcon

                -- Check against catalog for exact match
                if TC_CATALOG then
                    for sId, catData in pairs(TC_CATALOG) do
                        if catData.name == spellName then
                            matchedType = catData.type
                            matchedItemId = catData.item or 0
                            matchedSpellId = sId
                            if catData.icon then matchedIcon = catData.icon end
                            break
                        end
                    end
                end

                if not matchedType then
                    if isMount then matchedType = 1
                    elseif isPet then matchedType = 2 end
                end

                if matchedType then
                    local found = false
                    for k = 1, table.getn(TC.rawEntries) do
                        if TC.rawEntries[k].name == spellName and TC.rawEntries[k].type == matchedType then
                            found = true
                            break
                        end
                    end
                    if not found then
                        tinsert(TC.rawEntries, {
                            type = matchedType,
                            itemId = matchedItemId,
                            spellId = matchedSpellId,
                            name = spellName,
                            icon = matchedIcon
                        })
                    end
                end
            end
        end
    end

    -- 2. Scan player bags for toys, mounts, and pets
    if GetContainerNumSlots and GetContainerItemLink and TC_CATALOG_BY_ITEM then
        for bag = 0, 4 do
            local numSlots = GetContainerNumSlots(bag) or 0
            for slot = 1, numSlots do
                local link = GetContainerItemLink(bag, slot)
                if link then
                    local _, _, itemIdStr = string.find(link, "item:(%d+)")
                    if itemIdStr then
                        local iId = tonumber(itemIdStr)
                        local catEntry = TC_CATALOG_BY_ITEM[iId]
                        if catEntry then
                            local found = false
                            for k = 1, table.getn(TC.rawEntries) do
                                if TC.rawEntries[k].itemId == iId or (catEntry.spellId and catEntry.spellId > 0 and TC.rawEntries[k].spellId == catEntry.spellId) then
                                    found = true
                                    break
                                end
                            end
                            if not found then
                                tinsert(TC.rawEntries, {
                                    type = catEntry.type,
                                    itemId = iId,
                                    spellId = catEntry.spellId or 0,
                                    name = catEntry.name,
                                    icon = catEntry.icon
                                })
                            end
                        end
                    end
                end
            end
        end
    end
end

-- -----------------------------------------------------------------------------
-- 7. Network Event Ingress Frame
-- -----------------------------------------------------------------------------
local eventFrame = CreateFrame("Frame")
eventFrame:RegisterEvent("PLAYER_LOGIN")
eventFrame:RegisterEvent("PLAYER_ENTERING_WORLD")
eventFrame:RegisterEvent("VARIABLES_LOADED")
eventFrame:RegisterEvent("CHAT_MSG_ADDON")

eventFrame:SetScript("OnEvent", function()
    if event == "PLAYER_LOGIN" or event == "PLAYER_ENTERING_WORLD" then
        ScanLocalCollections()
        RequestCollectionSync()
        UpdateMinimapButtonPosition()
    elseif event == "VARIABLES_LOADED" then
        if not TurtleCollectionsDB then
            TurtleCollectionsDB = {
                mounts = {},
                pets = {},
                toys = {},
                minimapPos = 210,
                minimapVisible = true,
                lastSync = 0
            }
        end
        UpdateMinimapButtonPosition()
    elseif event == "CHAT_MSG_ADDON" then
        local prefix = arg1
        local message = arg2

        if prefix == "TW_COLLECT" and message then
            if string.find(message, "^DATA\t") then
                local dataStr = string.sub(message, 6)
                for token in string.gfind(dataStr, "[^,]+") do
                    local cType, itemId, spellId = ParseCollectionToken(token)
                    if cType and (spellId or itemId) then
                        local found = false
                        for i = 1, table.getn(TC.rawEntries) do
                            if (spellId and spellId > 0 and TC.rawEntries[i].spellId == spellId and TC.rawEntries[i].type == cType) or
                               (itemId and itemId > 0 and TC.rawEntries[i].itemId == itemId and TC.rawEntries[i].type == cType) then
                                found = true
                                break
                            end
                        end
                        if not found then
                            local sName, sIcon = GetSafeItemOrSpellInfo(itemId, spellId)
                            tinsert(TC.rawEntries, {
                                type = cType,
                                itemId = itemId,
                                spellId = spellId,
                                name = sName,
                                icon = sIcon
                            })
                        end
                    end
                end
            elseif message == "SYNC_DONE" then
                UpdateUI()
            end
        end
    end
end)

-- -----------------------------------------------------------------------------
-- 8. Global UI Handlers (Display-Only)
-- -----------------------------------------------------------------------------
function TurtleCollections_OnLoad()
    tinsert(UISpecialFrames, "TurtleCollectionsFrame")
end

function TurtleCollections_Toggle()
    if TurtleCollectionsFrame:IsVisible() then
        TurtleCollectionsFrame:Hide()
    else
        TurtleCollectionsFrame:Show()
        ScanLocalCollections()
        local now = GetTime()
        if (now - (TC.lastSync or 0)) > 15 then
            RequestCollectionSync()
        end
        UpdateUI()
    end
end

function TurtleCollections_SetTab(tabId)
    TC.activeTab = tabId
    TC.currentPage = 1
    ScanLocalCollections()
    local now = GetTime()
    if (now - (TC.lastSync or 0)) > 15 then
        RequestCollectionSync()
    end
    UpdateUI()
end

function TurtleCollections_PrevPage()
    if TC.currentPage > 1 then
        TC.currentPage = TC.currentPage - 1
        UpdateUI()
    end
end

function TurtleCollections_NextPage()
    local total = table.getn(TC.displayList)
    local maxPages = math.floor((total + TC.itemsPerPage - 1) / TC.itemsPerPage)
    if maxPages < 1 then maxPages = 1 end

    if TC.currentPage < maxPages then
        TC.currentPage = TC.currentPage + 1
        UpdateUI()
    end
end

function TurtleCollections_OnRowAction(rowIndex)
    local idx = (TC.currentPage - 1) * TC.itemsPerPage + rowIndex
    local item = TC.displayList[idx]
    if not item then return end

    if item.type == 1 then -- Mount
        local charLevel = UnitLevel("player")
        local reqLevel = GetMountRequiredLevel(item)
        local hasRiding = HasRidingSkill()
        if item.spellId == 30174 then
            hasRiding = true
        end

        if charLevel < reqLevel then
            if UIErrorsFrame then
                UIErrorsFrame:AddMessage("You must reach level " .. reqLevel .. " to use this mount.", 1.0, 0.1, 0.1, 1.0, UIERRORS_HOLD_TIME)
            end
            return
        end
        if not hasRiding then
            if UIErrorsFrame then
                UIErrorsFrame:AddMessage("You must learn riding skill to use this mount.", 1.0, 0.1, 0.1, 1.0, UIERRORS_HOLD_TIME)
            end
            return
        end

        SummonCollectionItem(item)
    else
        SummonCollectionItem(item)
    end
end

function TurtleCollections_OnRowClick(rowIndex)
    TurtleCollections_OnRowAction(rowIndex)
end

function TurtleCollections_OnRowEnter(rowIndex)
    local row = getglobal("TurtleCollectionsRow" .. rowIndex)
    if row and row.entry and GameTooltip then
        GameTooltip:SetOwner(row, "ANCHOR_RIGHT")
        GameTooltip:AddLine(row.entry.name, 1, 1, 1)
        local catName = "Mount"
        if row.entry.type == 2 then catName = "Companion Pet"
        elseif row.entry.type == 3 then catName = "Novelty Toy" end
        GameTooltip:AddLine(catName, 0.7, 0.7, 0.7)

        if row.entry.type == 1 then
            local reqLevel = GetMountRequiredLevel(row.entry)
            local charLevel = UnitLevel("player")
            local hasRiding = HasRidingSkill()
            if row.entry.spellId == 30174 then
                hasRiding = true
            end

            if charLevel < reqLevel then
                GameTooltip:AddLine("Requires Level " .. reqLevel, 1, 0.5, 0)
            elseif not hasRiding then
                GameTooltip:AddLine("Requires Riding Skill", 1, 0.5, 0)
            else
                GameTooltip:AddLine("Unlocked - Click to Summon", 0, 1, 0)
            end
        else
            GameTooltip:AddLine("Unlocked - Click to Use", 0, 1, 0)
        end
        GameTooltip:Show()
    end
end

function TurtleCollections_OnRowLeave()
    if GameTooltip then
        GameTooltip:Hide()
    end
end

function TurtleCollections_OnSearchChanged(text)
    TC.searchFilter = text or ""
    TC.currentPage = 1
    UpdateUI()
end

-- -----------------------------------------------------------------------------
-- 9. Slash Commands
-- -----------------------------------------------------------------------------
SLASH_TURTLECOLLECTIONS1 = "/collection"
SLASH_TURTLECOLLECTIONS2 = "/collections"
SLASH_TURTLECOLLECTIONS3 = "/toybox"
SLASH_TURTLECOLLECTIONS4 = "/pets"
SLASH_TURTLECOLLECTIONS5 = "/mounts"

SlashCmdList["TURTLECOLLECTIONS"] = function(msg)
    local cmd = string.lower(msg or "")
    if cmd == "sync" then
        SendServerCommand("sync")
        DEFAULT_CHAT_FRAME:AddMessage("|cff00ff00[Collections]|r Sync requested from server.")
    elseif cmd == "minimap" then
        if minimapButton:IsVisible() then
            minimapButton:Hide()
            TurtleCollectionsDB.minimapVisible = false
            DEFAULT_CHAT_FRAME:AddMessage("|cff00ff00[Collections]|r Minimap button hidden.")
        else
            minimapButton:Show()
            TurtleCollectionsDB.minimapVisible = true
            UpdateMinimapButtonPosition()
            DEFAULT_CHAT_FRAME:AddMessage("|cff00ff00[Collections]|r Minimap button shown.")
        end
    elseif cmd == "mounts" or cmd == "mount" then
        TC.activeTab = 1
        TurtleCollections_Toggle()
    elseif cmd == "pets" or cmd == "pet" then
        TC.activeTab = 2
        TurtleCollections_Toggle()
    elseif cmd == "toys" or cmd == "toy" then
        TC.activeTab = 3
        TurtleCollections_Toggle()
    else
        TurtleCollections_Toggle()
    end
end
