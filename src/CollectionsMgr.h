#pragma once

#include "CollectionsCommon.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <optional>

class Player;
class Creature;
class ChatHandler;
struct SpellEntry;

class CollectionsMgr
{
public:
    static CollectionsMgr& Instance();

    void Initialize();
    void LoadWorldCollections();

    void LoadAccountCollection(uint32 accountId);
    void UnloadAccountCollection(uint32 accountId);
    void AutoBackfillAccountCollection(uint32 accountId);

    bool AddToAccountCollection(Player* player, CollectionType type, uint32 itemId, uint32 spellId);
    bool HasAccountSpell(uint32 accountId, CollectionType type, uint32 spellId) const;

    CollectionType GetCollectionTypeByItem(uint32 itemId, uint32& outSpellId) const;
    CollectionType GetCollectionTypeBySpell(uint32 spellId, uint32& outItemId) const;

    void ScanKnownSpells(Player* player);
    void SyncPlayerSpells(Player* player);
    void ScanAndImportCharacterInventory(Player* player, ChatHandler* handler = nullptr);
    bool HasRidingSkill(Player* player) const;

    bool CanPlayerAddMountToSpellbook(Player* player, uint32 spellId) const;
    bool CanPlayerRideMount(Player* player, uint32 spellId) const;
    bool CanPlayerLearnMount(Player* player, uint32 spellId) const { return CanPlayerRideMount(player, spellId); }

    uint32 GetRequiredRidingSkill(uint32 spellId, SpellEntry const* spellInfo) const;
    bool IsAllowedForPlayerClass(uint32 spellId, Player const* player) const;
    bool IsAllowedForPlayerRace(uint32 spellId, uint32 itemId, Player const* player) const;
    bool IsMountSpell(SpellEntry const* spellInfo) const;
    bool IsRidingSkillSpell(uint32 spellId, SpellEntry const* spellInfo) const;
    bool HasEpicSpeedInDbc(SpellEntry const* spellInfo) const;
    uint32 CleanseInvalidMounts(Player* player);

    void SendAddonSync(Player* player, CollectionType type);
    void SendAddonMessage(Player* player, std::string const& prefix, std::string const& text);
    void RecordCritterPet(Player* player, Creature* creature);

    std::vector<CollectionEntry> GetAccountEntries(uint32 accountId, CollectionType type = COLLECTION_TYPE_NONE) const;

private:
    CollectionsMgr() = default;

    std::unordered_map<uint32, uint32> m_mountSpells;
    std::unordered_map<uint32, uint32> m_petSpells;
    std::unordered_map<uint32, uint32> m_toySpells;

    std::unordered_map<uint32, uint32> m_spellToMountItem;
    std::unordered_map<uint32, uint32> m_spellToPetItem;
    std::unordered_map<uint32, uint32> m_spellToToyItem;

    std::unordered_map<uint32, uint32> m_spellToRequiredSkillRank;

    mutable std::mutex m_cacheLock;
    std::unordered_map<uint32, std::vector<CollectionEntry>> m_accountCollections;
};

#define sCollectionsMgr CollectionsMgr::Instance()
