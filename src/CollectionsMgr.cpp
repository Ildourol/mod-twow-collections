#include "CollectionsMgr.h"
#include "CollectionsConfig.h"
#include "Objects/Player.h"
#include "Objects/Item.h"
#include "Objects/Bag.h"
#include "Objects/Creature.h"
#include "Objects/ItemPrototype.h"
#include "Chat/Chat.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "Spells/SpellMgr.h"
#include "Spells/SpellEntry.h"
#include "Spells/SpellDefines.h"
#include "SharedDefines.h"
#include "SpellAuraDefines.h"
#include "DBCStructure.h"
#include "DBCStores.h"

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
    m_spellToRequiredSkillRank.clear();

    // 1. Mounts (with required_skill_rank from item_template)
    QueryResult* mountRes = WorldDatabase.Query(
        "SELECT cm.itemId, cm.spellId, it.required_skill_rank "
        "FROM collection_mount cm "
        "LEFT JOIN item_template it ON cm.itemId = it.entry");

    if (mountRes)
    {
        do
        {
            Field* fields = mountRes->Fetch();
            uint32 itemId = fields[0].GetUInt32();
            uint32 spellId = fields[1].GetUInt32();
            uint32 reqSkillRank = fields[2].GetUInt32();

            m_mountSpells[itemId] = spellId;
            m_spellToMountItem[spellId] = itemId;
            if (reqSkillRank > 0)
                m_spellToRequiredSkillRank[spellId] = reqSkillRank;
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

    // 4. Dynamic scan of DBC spells for all mount spells across the game
    for (uint32 id = 1; id < sSpellMgr.GetMaxSpellId(); ++id)
    {
        SpellEntry const* spellInfo = sSpellMgr.GetSpellEntry(id);
        if (!spellInfo)
            continue;

        if (IsMountSpell(spellInfo))
        {
            if (m_spellToMountItem.find(id) == m_spellToMountItem.end())
            {
                m_spellToMountItem[id] = 0; // Dynamic mount without direct item template link
            }
        }
    }

    if (sCollectionsConfig.IsLoggingEnabled())
    {
        sLog.outString("[mod-twow-collections] Loaded %zu mounts (%zu with riding rank), %zu pets, %zu toys from WorldDatabase/DBC.",
            m_mountSpells.size(), m_spellToRequiredSkillRank.size(), m_petSpells.size(), m_toySpells.size());
    }
}

void CollectionsMgr::AutoBackfillAccountCollection(uint32 accountId)
{
    if (!accountId)
        return;

    // Discover and import all collection spells known by any character on this account
    std::unique_ptr<QueryResult> charRes(CharacterDatabase.PQuery(
        "SELECT guid, race, class FROM characters WHERE account = %u", accountId));

    if (!charRes)
        return;

    std::vector<uint32> guids;
    do
    {
        Field* fields = charRes->Fetch();
        guids.push_back(fields[0].GetUInt32());
    } while (charRes->NextRow());

    for (uint32 guid : guids)
    {
        std::unique_ptr<QueryResult> spellRes(CharacterDatabase.PQuery(
            "SELECT spell FROM character_spell WHERE guid = %u", guid));

        if (!spellRes)
            continue;

        do
        {
            uint32 spellId = spellRes->Fetch()[0].GetUInt32();
            uint32 itemId = 0;
            CollectionType type = GetCollectionTypeBySpell(spellId, itemId);

            if (type != COLLECTION_TYPE_NONE)
            {
                CharacterDatabase.PExecute(
                    "INSERT IGNORE INTO account_collection (account_id, collection_type, item_id, spell_id, learned_by_guid) "
                    "VALUES (%u, %u, %u, %u, %u)",
                    accountId, static_cast<uint32>(type), itemId, spellId, guid);
            }
        } while (spellRes->NextRow());
    }
}

void CollectionsMgr::LoadAccountCollection(uint32 accountId)
{
    if (!accountId)
        return;

    AutoBackfillAccountCollection(accountId);

    std::vector<CollectionEntry> entries;
    std::unique_ptr<QueryResult> res(CharacterDatabase.PQuery(
        "SELECT collection_type, item_id, spell_id FROM account_collection WHERE account_id = %u",
        accountId));

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

bool CollectionsMgr::IsMountSpell(SpellEntry const* spellInfo) const
{
    if (!spellInfo)
        return false;

    return spellInfo->Effect[0] == SPELL_EFFECT_APPLY_AURA &&
           spellInfo->EffectApplyAuraName[0] == SPELL_AURA_MOUNTED;
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

    SpellEntry const* spellInfo = sSpellMgr.GetSpellEntry(spellId);
    if (IsMountSpell(spellInfo))
    {
        outItemId = 0;
        return COLLECTION_TYPE_MOUNT;
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

bool CollectionsMgr::HasEpicSpeedInDbc(SpellEntry const* spellInfo) const
{
    if (!spellInfo)
        return false;

    for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (spellInfo->EffectApplyAuraName[i] == SPELL_AURA_MOD_INCREASE_MOUNTED_SPEED &&
            spellInfo->EffectBasePoints[i] >= 90)
        {
            return true;
        }
    }

    return false;
}

uint32 CollectionsMgr::GetRequiredRidingSkill(uint32 spellId, SpellEntry const* spellInfo) const
{
    auto const& forcedNormal = sCollectionsConfig.GetForcedNormalSpellIds();
    if (forcedNormal.find(spellId) != forcedNormal.end())
        return RIDING_SKILL_APPRENTICE;

    auto const& forcedEpic = sCollectionsConfig.GetForcedEpicSpellIds();
    if (forcedEpic.find(spellId) != forcedEpic.end())
        return RIDING_SKILL_JOURNEYMAN;

    auto skillItr = m_spellToRequiredSkillRank.find(spellId);
    if (skillItr != m_spellToRequiredSkillRank.end() && skillItr->second > 0)
        return (skillItr->second <= RIDING_SKILL_APPRENTICE) ? RIDING_SKILL_APPRENTICE : RIDING_SKILL_JOURNEYMAN;

    if (!spellInfo)
        return RIDING_SKILL_APPRENTICE;

    if (spellInfo->Custom & SPELL_CUSTOM_MOUNT_SPEED_100)
        return RIDING_SKILL_JOURNEYMAN;

    if (spellInfo->Custom & SPELL_CUSTOM_IGNORE_RIDING_SKILL_MOUNT_SPEED)
        return HasEpicSpeedInDbc(spellInfo) ? RIDING_SKILL_JOURNEYMAN : RIDING_SKILL_APPRENTICE;

    return HasEpicSpeedInDbc(spellInfo) ? RIDING_SKILL_JOURNEYMAN : RIDING_SKILL_APPRENTICE;
}

bool CollectionsMgr::IsAllowedForPlayerClass(uint32 spellId, Player const* player) const
{
    if (!player || !sCollectionsConfig.IsMountClassLimited())
        return true;

    SkillLineAbilityMapBounds bounds = sSpellMgr.GetSkillLineAbilityMapBoundsBySpellId(spellId);
    bool hasClassRestriction = false;
    bool playerMatches = false;

    for (auto itr = bounds.first; itr != bounds.second; ++itr)
    {
        SkillLineAbilityEntry const* ability = itr->second;
        if (!ability)
            continue;

        if (ability->classmask != 0)
        {
            hasClassRestriction = true;
            if (ability->classmask & player->GetClassMask())
                playerMatches = true;
        }
    }

    return !hasClassRestriction || playerMatches;
}

bool CollectionsMgr::IsAllowedForPlayerRace(uint32 spellId, uint32 itemId, Player const* player) const
{
    if (!player || !sCollectionsConfig.IsMountRaceLimited())
        return true;

    if (!itemId)
    {
        auto it = m_spellToMountItem.find(spellId);
        if (it != m_spellToMountItem.end())
            itemId = it->second;
    }

    if (!itemId)
        return true;

    ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
    if (!proto || proto->AllowableRace == 0 || proto->AllowableRace == 0xFFFFFFFF)
        return true;

    // Check if character's race matches the item's allowable race mask directly
    if (proto->AllowableRace & player->GetRaceMask())
        return true;

    // Faction alignment check
    Team playerTeam = Player::TeamForRace(player->GetRace());
    bool isAllianceItem = (proto->AllowableRace & RACEMASK_ALLIANCE) != 0;
    bool isHordeItem = (proto->AllowableRace & RACEMASK_HORDE) != 0;

    if (playerTeam == ALLIANCE && !isAllianceItem && isHordeItem)
        return false;
    if (playerTeam == HORDE && !isHordeItem && isAllianceItem)
        return false;

    // If item allows races within player's faction, allow it
    if (playerTeam == ALLIANCE && isAllianceItem)
        return true;
    if (playerTeam == HORDE && isHordeItem)
        return true;

    return true; // Default to allow if not explicitly belonging to opposing faction
}

bool CollectionsMgr::IsRidingSkillSpell(uint32 spellId, SpellEntry const* spellInfo) const
{
    if (!spellInfo)
        spellInfo = sSpellMgr.GetSpellEntry(spellId);

    if (!spellInfo)
        return false;

    for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (spellInfo->Effect[i] == SPELL_EFFECT_SKILL &&
            spellInfo->EffectMiscValue[i] == SKILL_RIDING)
        {
            return true;
        }
    }

    return false;
}

bool CollectionsMgr::HasRidingSkill(Player* player) const
{
    if (!player)
        return false;

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

    if (player->HasSpell(13819u) || player->HasSpell(5784u) ||
        player->HasSpell(23214u) || player->HasSpell(23161u))
    {
        return true;
    }

    return false;
}

bool CollectionsMgr::CanPlayerAddMountToSpellbook(Player* player, uint32 spellId) const
{
    if (!player)
        return false;

    if (sCollectionsConfig.IsAllowAllMounts())
        return true;

    auto const& excluded = sCollectionsConfig.GetExcludedSpellIds();
    if (excluded.find(spellId) != excluded.end())
        return false;

    if (!IsAllowedForPlayerClass(spellId, player))
        return false;

    uint32 itemId = 0;
    GetCollectionTypeBySpell(spellId, itemId);
    if (!IsAllowedForPlayerRace(spellId, itemId, player))
        return false;

    return true;
}

bool CollectionsMgr::CanPlayerRideMount(Player* player, uint32 spellId) const
{
    if (!CanPlayerAddMountToSpellbook(player, spellId))
        return false;

    if (sCollectionsConfig.IsAllowAllMounts())
        return true;

    SpellEntry const* spellInfo = sSpellMgr.GetSpellEntry(spellId);
    if (!spellInfo)
        return false;

    uint32 requiredRidingRank = GetRequiredRidingSkill(spellId, spellInfo);

    if (sCollectionsConfig.IsMountLevelEnforced())
    {
        if (spellInfo->baseLevel > 0 && player->GetLevel() < spellInfo->baseLevel)
            return false;

        // Swift Riding Turtle (spell 30174) requires level 18
        if (spellId == 30174 && player->GetLevel() < 18)
            return false;

        if (requiredRidingRank >= RIDING_SKILL_JOURNEYMAN && player->GetLevel() < 60)
            return false;
        else if (requiredRidingRank <= RIDING_SKILL_APPRENTICE && spellId != 30174 && player->GetLevel() < 40)
            return false;
    }

    if (sCollectionsConfig.IsRidingSkillEnforced() && spellId != 30174)
    {
        uint16 playerRiding = player->GetSkillValue(SKILL_RIDING);
        static const SkillType ridingSkills[] = {
            SKILL_RIDING_HORSE,
            SKILL_RIDING_WOLF,
            SKILL_RIDING_TIGER,
            SKILL_RIDING_RAM,
            SKILL_RIDING_RAPTOR,
            SKILL_RIDING_MECHANOSTRIDER,
            SKILL_RIDING_UNDEAD_HORSE,
            SKILL_RIDING_KODO
        };
        for (SkillType sk : ridingSkills)
        {
            uint16 val = player->GetSkillValue(sk);
            if (val > playerRiding)
                playerRiding = val;
        }

        // Paladin and Warlock class mount spells grant innate riding capability
        if (playerRiding == 0)
        {
            if (requiredRidingRank <= RIDING_SKILL_APPRENTICE && (player->HasSpell(13819u) || player->HasSpell(5784u)))
                playerRiding = RIDING_SKILL_APPRENTICE;
            else if (requiredRidingRank >= RIDING_SKILL_JOURNEYMAN && (player->HasSpell(23214u) || player->HasSpell(23161u)))
                playerRiding = RIDING_SKILL_JOURNEYMAN;
        }

        if (playerRiding < requiredRidingRank)
            return false;
    }

    return true;
}

uint32 CollectionsMgr::CleanseInvalidMounts(Player* player)
{
    if (!player)
        return 0;

    uint32 cleansedCount = 0;
    for (auto const& [spellId, itemId] : m_spellToMountItem)
    {
        if (player->HasSpell(spellId))
        {
            if (!CanPlayerAddMountToSpellbook(player, spellId))
            {
                player->RemoveSpell(spellId, false, false);
                ++cleansedCount;
            }
        }
    }

    return cleansedCount;
}

void CollectionsMgr::ScanKnownSpells(Player* player)
{
    if (!player || !player->GetSession())
        return;

    for (auto const& [spellId, playerSpell] : player->GetSpellMap())
    {
        if (playerSpell.state == PLAYERSPELL_REMOVED)
            continue;

        uint32 itemId = 0;
        CollectionType type = GetCollectionTypeBySpell(spellId, itemId);
        if (type != COLLECTION_TYPE_NONE)
        {
            AddToAccountCollection(player, type, itemId, spellId);
        }
    }
}

void CollectionsMgr::SyncPlayerSpells(Player* player)
{
    if (!player || !player->GetSession() || !sCollectionsConfig.IsEnabled())
        return;

    if (player->GetSession()->IsHeadless())
        return;

    thread_local bool isSyncing = false;
    if (isSyncing)
        return;

    isSyncing = true;

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
            bool canTeach = sCollectionsConfig.IsShowInSpellbookAtLowLevel()
                ? CanPlayerAddMountToSpellbook(player, entry.spellId)
                : CanPlayerRideMount(player, entry.spellId);

            if (canTeach)
            {
                if (!player->HasSpell(entry.spellId))
                    player->LearnSpell(entry.spellId, false);
            }
            else
            {
                if (!sCollectionsConfig.IsShowInSpellbookAtLowLevel() && player->HasSpell(entry.spellId))
                    player->RemoveSpell(entry.spellId, false, false);
            }
        }
    }

    isSyncing = false;
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
                    bool canTeach = (type != COLLECTION_TYPE_MOUNT) ||
                        (sCollectionsConfig.IsShowInSpellbookAtLowLevel()
                            ? CanPlayerAddMountToSpellbook(player, spellId)
                            : CanPlayerRideMount(player, spellId));

                    if (canTeach)
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
        if (entries[i].type == COLLECTION_TYPE_MOUNT)
        {
            if (!CanPlayerAddMountToSpellbook(player, entries[i].spellId))
                continue;

            // If low level display is disabled in addon browser, filter out mounts player cannot yet ride
            if (!sCollectionsConfig.IsShowMountsToLowLevel() && !CanPlayerRideMount(player, entries[i].spellId))
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
