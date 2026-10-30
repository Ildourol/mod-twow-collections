#include "CollectionsMgr.h"
#include "CollectionsConfig.h"
#include "Objects/Player.h"
#include "Objects/Item.h"
#include "Objects/Bag.h"
#include "Objects/Creature.h"
#include "Chat/Chat.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "Spells/SpellMgr.h"

CollectionsMgr& CollectionsMgr::Instance()
{
    static CollectionsMgr instance;
    return instance;
}

void CollectionsMgr::Initialize()
{
    sCollectionsConfig.LoadConfig();
    LoadWorldCollections();
}

void CollectionsMgr::LoadWorldCollections()
{
    m_mountSpells.clear();
    m_petSpells.clear();
    m_toySpells.clear();
    m_spellToMountItem.clear();
    m_spellToPetItem.clear();
    m_spellToToyItem.clear();

    // 1. Mounts
    QueryResult* mountRes = WorldDatabase.Query("SELECT itemId, spellId FROM collection_mount");
    if (mountRes)
    {
        do
        {
            Field* fields = mountRes->Fetch();
            uint32 itemId = fields[0].GetUInt32();
            uint32 spellId = fields[1].GetUInt32();
            m_mountSpells[itemId] = spellId;
            m_spellToMountItem[spellId] = itemId;
        } while (mountRes->NextRow());
        delete mountRes;
    }

    // 2. Companion Pets
    QueryResult* petRes = WorldDatabase.Query("SELECT itemId, spellId FROM collection_pet");
    if (petRes)
    {
        do
        {
            Field* fields = petRes->Fetch();
            uint32 itemId = fields[0].GetUInt32();
            uint32 spellId = fields[1].GetUInt32();
            m_petSpells[itemId] = spellId;
            m_spellToPetItem[spellId] = itemId;
        } while (petRes->NextRow());
        delete petRes;
    }

    // 3. Toys
    QueryResult* toyRes = WorldDatabase.Query("SELECT itemId, spellId FROM collection_toy");
    if (toyRes)
    {
        do
        {
            Field* fields = toyRes->Fetch();
            uint32 itemId = fields[0].GetUInt32();
            uint32 spellId = fields[1].GetUInt32();
            m_toySpells[itemId] = spellId;
            m_spellToToyItem[spellId] = itemId;
        } while (toyRes->NextRow());
        delete toyRes;
    }

    if (sCollectionsConfig.IsLoggingEnabled())
    {
        sLog.outString("[mod-twow-collections] Loaded %zu mounts, %zu pets, %zu toys from WorldDatabase.",
            m_mountSpells.size(), m_petSpells.size(), m_toySpells.size());
    }
}

void CollectionsMgr::LoadAccountCollection(uint32 accountId)
{
    if (!accountId)
        return;

    std::vector<CollectionEntry> entries;
    QueryResult* res = CharacterDatabase.PQuery(
        "SELECT collection_type, item_id, spell_id FROM account_collection WHERE account_id = %u",
        accountId);

    if (res)
    {
        do
        {
            Field* fields = res->Fetch();
            CollectionEntry entry;
            entry.type = static_cast<CollectionType>(fields[0].GetUInt8());
            entry.itemId = fields[1].GetUInt32();
            entry.spellId = fields[2].GetUInt32();
            entries.push_back(entry);
        } while (res->NextRow());
        delete res;
    }

    std::lock_guard<std::mutex> lock(m_cacheLock);
    m_accountCollections[accountId] = std::move(entries);
}

void CollectionsMgr::UnloadAccountCollection(uint32 accountId)
{
    std::lock_guard<std::mutex> lock(m_cacheLock);
    m_accountCollections.erase(accountId);
}

std::vector<CollectionEntry> CollectionsMgr::GetAccountEntries(uint32 accountId, CollectionType type) const
{
    std::lock_guard<std::mutex> lock(m_cacheLock);
    auto it = m_accountCollections.find(accountId);
    if (it == m_accountCollections.end())
        return {};

    if (type == COLLECTION_TYPE_NONE)
        return it->second;

    std::vector<CollectionEntry> filtered;
    for (auto const& entry : it->second)
    {
        if (entry.type == type)
            filtered.push_back(entry);
    }
    return filtered;
}

bool CollectionsMgr::HasAccountSpell(uint32 accountId, CollectionType type, uint32 spellId) const
{
    std::lock_guard<std::mutex> lock(m_cacheLock);
    auto it = m_accountCollections.find(accountId);
    if (it == m_accountCollections.end())
        return false;

    for (auto const& entry : it->second)
    {
        if (entry.spellId == spellId && (type == COLLECTION_TYPE_NONE || entry.type == type))
            return true;
    }
    return false;
}

CollectionType CollectionsMgr::GetCollectionTypeByItem(uint32 itemId, uint32& outSpellId) const
{
    if (auto it = m_mountSpells.find(itemId); it != m_mountSpells.end())
    {
        outSpellId = it->second;
        return COLLECTION_TYPE_MOUNT;
    }
    if (auto it = m_petSpells.find(itemId); it != m_petSpells.end())
    {
        outSpellId = it->second;
        return COLLECTION_TYPE_PET;
    }
    if (auto it = m_toySpells.find(itemId); it != m_toySpells.end())
    {
        outSpellId = it->second;
        return COLLECTION_TYPE_TOY;
    }
    outSpellId = 0;
    return COLLECTION_TYPE_NONE;
}

CollectionType CollectionsMgr::GetCollectionTypeBySpell(uint32 spellId, uint32& outItemId) const
{
    if (auto it = m_spellToMountItem.find(spellId); it != m_spellToMountItem.end())
    {
        outItemId = it->second;
        return COLLECTION_TYPE_MOUNT;
    }
    if (auto it = m_spellToPetItem.find(spellId); it != m_spellToPetItem.end())
    {
        outItemId = it->second;
        return COLLECTION_TYPE_PET;
    }
    if (auto it = m_spellToToyItem.find(spellId); it != m_spellToToyItem.end())
    {
        outItemId = it->second;
        return COLLECTION_TYPE_TOY;
    }
    outItemId = 0;
    return COLLECTION_TYPE_NONE;
}

bool CollectionsMgr::AddToAccountCollection(Player* player, CollectionType type, uint32 itemId, uint32 spellId)
{
    if (!player || !player->GetSession() || !spellId)
        return false;

    uint32 accountId = player->GetSession()->GetAccountId();

    {
        std::lock_guard<std::mutex> lock(m_cacheLock);
        auto& list = m_accountCollections[accountId];
        for (auto const& existing : list)
        {
            if (existing.spellId == spellId && existing.type == type)
                return false; // Already unlocked
        }
        CollectionEntry entry;
        entry.type = type;
        entry.itemId = itemId;
        entry.spellId = spellId;
        list.push_back(entry);
    }

    CharacterDatabase.PExecute(
        "INSERT IGNORE INTO account_collection (account_id, collection_type, item_id, spell_id, learned_by_guid) "
        "VALUES (%u, %u, %u, %u, %u)",
        accountId, static_cast<uint32>(type), itemId, spellId, player->GetGUIDLow());

    if (sCollectionsConfig.IsAnnounceEnabled() && !player->GetSession()->IsHeadless())
    {
        std::string name = "Unknown";
        if (ItemPrototype const* proto = itemId ? sObjectMgr.GetItemPrototype(itemId) : nullptr)
            name = proto->Name1;
        else if (SpellEntry const* spellInfo = sSpellMgr.GetSpellEntry(spellId))
            name = spellInfo->SpellName[0];

        char const* typeStr = (type == COLLECTION_TYPE_MOUNT) ? "Mount" :
                              (type == COLLECTION_TYPE_PET)   ? "Companion Pet" : "Toy";

        std::string msg = "|cff00ff00[Collections]|r Added " + name + " to your Account " + typeStr + " Collection!";
        ChatHandler(player).SendSysMessage(msg.c_str());
    }

    SendAddonSync(player, type);
    return true;
}

bool CollectionsMgr::HasRidingSkill(Player* player) const
{
    if (!player)
        return false;

    // Check unified riding skill as well as all racial / faction riding skills
    if (player->GetSkillValue(SKILL_RIDING) > 0 ||
        player->GetSkillValue(SKILL_RIDING_HORSE) > 0 ||
        player->GetSkillValue(SKILL_RIDING_WOLF) > 0 ||
        player->GetSkillValue(SKILL_RIDING_TIGER) > 0 ||
        player->GetSkillValue(SKILL_RIDING_RAM) > 0 ||
        player->GetSkillValue(SKILL_RIDING_RAPTOR) > 0 ||
        player->GetSkillValue(SKILL_RIDING_MECHANOSTRIDER) > 0 ||
        player->GetSkillValue(SKILL_RIDING_UNDEAD_HORSE) > 0 ||
        player->GetSkillValue(SKILL_RIDING_KODO) > 0)
    {
        return true;
    }

    // Paladin and Warlock class mount spells grant innate riding capability
    if (player->HasSpell(13819u) || player->HasSpell(5784u) ||
        player->HasSpell(23214u) || player->HasSpell(23161u))
    {
        return true;
    }

    return false;
}

bool CollectionsMgr::CanPlayerLearnMount(Player* player, uint32 spellId) const
{
    if (!player)
        return false;

    if (sCollectionsConfig.IsMountLevelEnforced())
    {
        SpellEntry const* spellInfo = sSpellMgr.GetSpellEntry(spellId);
        if (spellInfo)
        {
            if (spellInfo->baseLevel > 0 && player->GetLevel() < spellInfo->baseLevel)
                return false;

            // Swift Riding Turtle (spell 30174) requires level 18
            if (spellId == 30174 && player->GetLevel() < 18)
                return false;

            // Check speed threshold: 100% epic mounts require level 60, 60% standard mounts require level 40
            bool isFastMount = false;
            for (int i = 0; i < 3; ++i)
            {
                if (spellInfo->EffectApplyAuraName[i] == SPELL_AURA_MOD_INCREASE_MOUNTED_SPEED &&
                    spellInfo->EffectBasePoints[i] >= 99)
                {
                    isFastMount = true;
                    break;
                }
            }

            if (isFastMount && player->GetLevel() < 60)
                return false;
            else if (!isFastMount && spellId != 30174 && player->GetLevel() < 40)
                return false;
        }
    }

    if (sCollectionsConfig.IsRidingSkillEnforced() && spellId != 30174)
    {
        if (!HasRidingSkill(player))
            return false;
    }

    return true;
}

void CollectionsMgr::ScanKnownSpells(Player* player)
{
    if (!player || !player->GetSession())
        return;

    for (auto const& [spellId, itemId] : m_spellToMountItem)
    {
        if (player->HasSpell(spellId))
            AddToAccountCollection(player, COLLECTION_TYPE_MOUNT, itemId, spellId);
    }

    for (auto const& [spellId, itemId] : m_spellToPetItem)
    {
        if (player->HasSpell(spellId))
            AddToAccountCollection(player, COLLECTION_TYPE_PET, itemId, spellId);
    }

    for (auto const& [spellId, itemId] : m_spellToToyItem)
    {
        if (player->HasSpell(spellId))
            AddToAccountCollection(player, COLLECTION_TYPE_TOY, itemId, spellId);
    }
}

void CollectionsMgr::SyncPlayerSpells(Player* player)
{
    if (!player || !player->GetSession() || !sCollectionsConfig.IsEnabled())
        return;

    if (player->GetSession()->IsHeadless())
        return;

    uint32 accountId = player->GetSession()->GetAccountId();
    std::vector<CollectionEntry> entries = GetAccountEntries(accountId);

    for (auto const& entry : entries)
    {
        if (entry.type == COLLECTION_TYPE_PET && sCollectionsConfig.IsAccountPetsEnabled() && sCollectionsConfig.IsPetAutoLearnEnabled())
        {
            if (!player->HasSpell(entry.spellId))
                player->LearnSpell(entry.spellId, false);
        }
        else if (entry.type == COLLECTION_TYPE_TOY && sCollectionsConfig.IsAccountToysEnabled() && sCollectionsConfig.IsToyAutoLearnEnabled())
        {
            if (!player->HasSpell(entry.spellId))
                player->LearnSpell(entry.spellId, false);
        }
        else if (entry.type == COLLECTION_TYPE_MOUNT && sCollectionsConfig.IsAccountMountsEnabled() && sCollectionsConfig.IsMountAutoLearnEnabled())
        {
            if (CanPlayerLearnMount(player, entry.spellId))
            {
                if (!player->HasSpell(entry.spellId))
                    player->LearnSpell(entry.spellId, false);
            }
        }
    }
}

void CollectionsMgr::ScanAndImportCharacterInventory(Player* player, ChatHandler* handler)
{
    if (!player || !player->GetSession())
        return;

    uint32 importedCount = 0;

    // 1. Scan known spells against collection lookup
    for (auto const& [spellId, itemId] : m_spellToMountItem)
    {
        if (player->HasSpell(spellId))
        {
            if (AddToAccountCollection(player, COLLECTION_TYPE_MOUNT, itemId, spellId))
                ++importedCount;
        }
    }

    for (auto const& [spellId, itemId] : m_spellToPetItem)
    {
        if (player->HasSpell(spellId))
        {
            if (AddToAccountCollection(player, COLLECTION_TYPE_PET, itemId, spellId))
                ++importedCount;
        }
    }

    for (auto const& [spellId, itemId] : m_spellToToyItem)
    {
        if (player->HasSpell(spellId))
        {
            if (AddToAccountCollection(player, COLLECTION_TYPE_TOY, itemId, spellId))
                ++importedCount;
        }
    }

    // 2. Helper lambda to inspect and consume collection items
    auto checkAndImportItem = [&](Item* item)
    {
        if (!item)
            return;

        uint32 itemId = item->GetEntry();
        uint32 spellId = 0;
        CollectionType type = GetCollectionTypeByItem(itemId, spellId);
        if (type != COLLECTION_TYPE_NONE && spellId != 0)
        {
            if (AddToAccountCollection(player, type, itemId, spellId))
            {
                ++importedCount;
                if (!player->HasSpell(spellId))
                {
                    if (type != COLLECTION_TYPE_MOUNT || CanPlayerLearnMount(player, spellId))
                        player->LearnSpell(spellId, false);
                }

                if (sCollectionsConfig.IsConsumeItemEnabled())
                    player->DestroyItemCount(itemId, 1, true);
            }
        }
    };

    // Scan backpack (slots 23..38)
    for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        checkAndImportItem(player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot));

    // Scan bags in bag slots (19..22)
    for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
    {
        if (Bag* bag = dynamic_cast<Bag*>(player->GetItemByPos(INVENTORY_SLOT_BAG_0, bagSlot)))
        {
            for (uint8 s = 0; s < bag->GetBagSize(); ++s)
                checkAndImportItem(bag->GetItemByPos(s));
        }
    }

    // Scan bank slots (39..62)
    for (uint8 slot = BANK_SLOT_ITEM_START; slot < BANK_SLOT_ITEM_END; ++slot)
        checkAndImportItem(player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot));

    // Scan bank bags (63..68)
    for (uint8 bagSlot = BANK_SLOT_BAG_START; bagSlot < BANK_SLOT_BAG_END; ++bagSlot)
    {
        if (Bag* bag = dynamic_cast<Bag*>(player->GetItemByPos(INVENTORY_SLOT_BAG_0, bagSlot)))
        {
            for (uint8 s = 0; s < bag->GetBagSize(); ++s)
                checkAndImportItem(bag->GetItemByPos(s));
        }
    }

    if (handler)
    {
        std::string reply = "|cff00ff00[Collections]|r Synchronization complete. " +
                            std::to_string(importedCount) + " collection entries added.";
        handler->SendSysMessage(reply.c_str());
    }
}

void CollectionsMgr::SendAddonMessage(Player* player, std::string const& prefix, std::string const& text)
{
    if (!player || !player->GetSession() || player->GetSession()->IsHeadless())
        return;

    player->SendAddonMessage(prefix, text);
}

void CollectionsMgr::SendAddonSync(Player* player, CollectionType type)
{
    if (!player || !player->GetSession() || player->GetSession()->IsHeadless())
        return;

    uint32 accountId = player->GetSession()->GetAccountId();
    std::vector<CollectionEntry> entries = GetAccountEntries(accountId, type);

    // Chunk payloads into 200-byte segments for 254-byte Vanilla addon wire safety
    std::string chunk;
    for (size_t i = 0; i < entries.size(); ++i)
    {
        // If low level display is disabled, filter out mounts the player cannot yet ride
        if (entries[i].type == COLLECTION_TYPE_MOUNT && !sCollectionsConfig.IsShowMountsToLowLevel())
        {
            if (!CanPlayerLearnMount(player, entries[i].spellId))
                continue;
        }

        std::string token = std::to_string(static_cast<uint32>(entries[i].type)) + ":" +
                            std::to_string(entries[i].itemId) + ":" +
                            std::to_string(entries[i].spellId) + ",";

        if (chunk.length() + token.length() > 200)
        {
            SendAddonMessage(player, TW_COLLECTIONS_ADDON_PREFIX, "DATA\t" + chunk);
            chunk.clear();
        }
        chunk += token;
    }

    if (!chunk.empty())
        SendAddonMessage(player, TW_COLLECTIONS_ADDON_PREFIX, "DATA\t" + chunk);

    SendAddonMessage(player, TW_COLLECTIONS_ADDON_PREFIX, "SYNC_DONE");
}

void CollectionsMgr::RecordCritterPet(Player* player, Creature* creature)
{
    if (!sCollectionsConfig.IsCritterPetsEnabled() || !player || !creature)
        return;

    CharacterDatabase.PExecute(
        "INSERT IGNORE INTO character_critter_pet (guid, spawn, entry) VALUES (%u, %u, %u)",
        player->GetGUIDLow(), creature->GetGUIDLow(), creature->GetEntry());
}
