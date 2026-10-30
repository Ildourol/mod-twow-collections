#include "CollectionsCommon.h"
#include "CollectionsConfig.h"
#include "CollectionsMgr.h"
#include "ScriptObjects.h"
#include "Objects/Player.h"
#include "Objects/Item.h"
#include "Objects/Creature.h"
#include "Chat/Chat.h"
#include "Opcodes.h"
#include "Log.h"
#include "Spells/SpellMgr.h"
#include "Spells/Spell.h"

// -----------------------------------------------------------------------------
// 1. Player Lifecycle Script
// -----------------------------------------------------------------------------
class CollectionsPlayerScript : public PlayerScript
{
public:
    CollectionsPlayerScript()
        : PlayerScript("CollectionsPlayerScript",
            {
                PLAYERHOOK_ON_LOGIN,
                PLAYERHOOK_ON_LOGOUT,
                PLAYERHOOK_ON_LEVEL_CHANGED,
                PLAYERHOOK_ON_LEARN_SPELL,
                PLAYERHOOK_ON_ADDON_MESSAGE,
                PLAYERHOOK_ON_TEXT_EMOTE,
                PLAYERHOOK_ON_SPELL_CAST
            })
    {
    }

    void OnSpellCast(Player* player, Spell* spell, bool /*skipCheck*/) override
    {
        if (!sCollectionsConfig.IsEnabled() || !player || !spell || !spell->m_spellInfo)
            return;

        if (player->GetSession() && player->GetSession()->IsHeadless())
            return;

        uint32 spellId = spell->m_spellInfo->Id;
        uint32 dummyItem = 0;
        CollectionType type = sCollectionsMgr.GetCollectionTypeBySpell(spellId, dummyItem);
        if (type == COLLECTION_TYPE_MOUNT)
        {
            if (!sCollectionsMgr.CanPlayerLearnMount(player, spellId))
            {
                spell->cancel();
                player->RemoveAurasDueToSpell(spellId);
                player->Unmount();
                ChatHandler(player).SendSysMessage("|cffff0000You do not meet the level or riding requirements to summon this mount.|r");
            }
        }
    }

    void OnLogin(Player* player) override
    {
        if (!sCollectionsConfig.IsEnabled() || !player || !player->GetSession())
            return;

        if (player->GetSession()->IsHeadless())
            return;

        uint32 accountId = player->GetSession()->GetAccountId();
        sCollectionsMgr.LoadAccountCollection(accountId);
        sCollectionsMgr.ScanKnownSpells(player);
        sCollectionsMgr.SyncPlayerSpells(player);
        sCollectionsMgr.SendAddonSync(player, COLLECTION_TYPE_NONE);
    }

    void OnLogout(Player* player) override
    {
        if (!player || !player->GetSession())
            return;

        sCollectionsMgr.UnloadAccountCollection(player->GetSession()->GetAccountId());
    }

    void OnLevelChanged(Player* player, uint8 /*oldLevel*/) override
    {
        if (!sCollectionsConfig.IsEnabled() || !player || !player->GetSession())
            return;

        if (player->GetSession()->IsHeadless())
            return;

        sCollectionsMgr.SyncPlayerSpells(player);
        sCollectionsMgr.SendAddonSync(player, COLLECTION_TYPE_MOUNT);
    }

    void OnLearnSpell(Player* player, uint32 spellId) override
    {
        if (!sCollectionsConfig.IsEnabled() || !player || !player->GetSession() || !spellId)
            return;

        uint32 itemId = 0;
        CollectionType type = sCollectionsMgr.GetCollectionTypeBySpell(spellId, itemId);
        if (type != COLLECTION_TYPE_NONE)
        {
            sCollectionsMgr.AddToAccountCollection(player, type, itemId, spellId);
        }

        // If player learned a riding skill or class mount, sync eligible mounts
        if (spellId == 33388 || spellId == 33391 || // Apprentice / Journeyman Riding
            spellId == 824 || spellId == 826 || spellId == 828 || spellId == 830 || // Racial riding spells
            spellId == 10861 || spellId == 10907 || spellId == 10967 || spellId == 18995 ||
            spellId == 13819 || spellId == 23214 || // Paladin class mounts
            spellId == 5784 || spellId == 23161)    // Warlock class mounts
        {
            sCollectionsMgr.SyncPlayerSpells(player);
            sCollectionsMgr.SendAddonSync(player, COLLECTION_TYPE_MOUNT);
        }
    }

    bool OnAddonMessage(Player* from, std::string const& msg) override
    {
        if (!sCollectionsConfig.IsEnabled() || !from || !from->GetSession())
            return false;

        if (from->GetSession()->IsHeadless())
            return false;

        if (msg.rfind("TW_COLLECT\t", 0) != 0)
            return false;

        std::string payload = msg.substr(11); // After "TW_COLLECT\t"
        size_t tab = payload.find('\t');
        std::string action = (tab != std::string::npos) ? payload.substr(0, tab) : payload;
        std::string rest = (tab != std::string::npos) ? payload.substr(tab + 1) : "";

        if (action == "QUERY")
        {
            CollectionType type = COLLECTION_TYPE_NONE;
            if (!rest.empty())
                type = static_cast<CollectionType>(std::atoi(rest.c_str()));
            sCollectionsMgr.SendAddonSync(from, type);
            return true;
        }
        else if (action == "SUMMON")
        {
            uint32 spellId = std::strtoul(rest.c_str(), nullptr, 10);
            if (spellId && sCollectionsMgr.HasAccountSpell(from->GetSession()->GetAccountId(), COLLECTION_TYPE_NONE, spellId))
            {
                uint32 dummyItem = 0;
                CollectionType type = sCollectionsMgr.GetCollectionTypeBySpell(spellId, dummyItem);
                if (type == COLLECTION_TYPE_MOUNT && !sCollectionsMgr.CanPlayerLearnMount(from, spellId))
                {
                    ChatHandler(from).SendSysMessage("|cffff0000You do not meet the level or riding requirements to summon this mount.|r");
                    return true;
                }
                from->CastSpell(from, spellId, false);
            }
            return true;
        }
        else if (action == "SYNC")
        {
            sCollectionsMgr.ScanAndImportCharacterInventory(from);
            sCollectionsMgr.SyncPlayerSpells(from);
            sCollectionsMgr.SendAddonSync(from, COLLECTION_TYPE_NONE);
            return true;
        }

        return true;
    }

    void OnTextEmote(Player* player, uint32 textEmote, uint32 /*emoteNum*/, ObjectGuid guid) override
    {
        // TEXTEMOTE_PAT = 75
        if (!sCollectionsConfig.IsCritterPetsEnabled() || !player || textEmote != 75)
            return;

        if (guid.IsCreature())
        {
            if (Creature* creature = player->GetMap()->GetCreature(guid))
            {
                // Check if creature is a critter (type == 8)
                if (creature->GetCreatureInfo() && creature->GetCreatureInfo()->type == 8)
                    sCollectionsMgr.RecordCritterPet(player, creature);
            }
        }
    }
};

// -----------------------------------------------------------------------------
// 2. AllItemScript Hook (Intercept Collection Item Use)
// -----------------------------------------------------------------------------
class CollectionsItemScript : public AllItemScript
{
public:
    CollectionsItemScript() : AllItemScript("CollectionsItemScript") {}

    bool CanItemUse(Player* player, Item* item, SpellCastTargets& /*targets*/) override
    {
        if (!sCollectionsConfig.IsEnabled() || !player || !item || !player->GetSession())
            return false;

        uint32 itemId = item->GetEntry();
        uint32 spellId = 0;
        CollectionType type = sCollectionsMgr.GetCollectionTypeByItem(itemId, spellId);
        if (type == COLLECTION_TYPE_NONE || spellId == 0)
            return false;

        if (player->HasSpell(spellId))
        {
            player->GetSession()->SendNotification("You already have this in your collection.");
            return true; // Handled, prevent consuming another item
        }

        if (type == COLLECTION_TYPE_MOUNT && !sCollectionsMgr.CanPlayerLearnMount(player, spellId))
        {
            player->GetSession()->SendNotification("You do not meet the level or riding requirements for this mount.");
            return true;
        }

        player->LearnSpell(spellId, false);
        sCollectionsMgr.AddToAccountCollection(player, type, itemId, spellId);

        if (sCollectionsConfig.IsConsumeItemEnabled())
            player->DestroyItemCount(itemId, 1, true);

        return true; // Short-circuit core handling
    }
};

// -----------------------------------------------------------------------------
// 3. Network Ingress ServerScript (Secondary Fallback)
// -----------------------------------------------------------------------------
class CollectionsServerScript : public ServerScript
{
public:
    CollectionsServerScript()
        : ServerScript("CollectionsServerScript", { SERVERHOOK_CAN_PACKET_RECEIVE })
    {
    }

    bool CanPacketReceive(WorldSession* session, WorldPacket const& packet) override
    {
        if (!sCollectionsConfig.IsEnabled() || !session || !session->GetPlayer())
            return true;

        if (session->IsHeadless())
            return true;

        if (packet.GetOpcode() == CMSG_MESSAGECHAT)
        {
            try
            {
                WorldPacket copy = packet;
                copy.rpos(0);

                uint32 type = 0;
                uint32 language = 0;
                copy >> type >> language;

                if (language == LANG_ADDON)
                {
                    std::string msg;
                    if (type == CHAT_MSG_WHISPER)
                    {
                        std::string to;
                        copy >> to >> msg;
                    }
                    else if (type == CHAT_MSG_CHANNEL)
                    {
                        std::string channel;
                        copy >> channel >> msg;
                    }
                    else
                    {
                        copy >> msg;
                    }

                    if (msg.rfind("TW_COLLECT\t", 0) == 0)
                    {
                        Player* player = session->GetPlayer();
                        std::string payload = msg.substr(11); // After "TW_COLLECT\t"
                        size_t tab = payload.find('\t');
                        std::string action = (tab != std::string::npos) ? payload.substr(0, tab) : payload;
                        std::string rest = (tab != std::string::npos) ? payload.substr(tab + 1) : "";

                        if (action == "QUERY")
                        {
                            CollectionType cType = COLLECTION_TYPE_NONE;
                            if (!rest.empty())
                                cType = static_cast<CollectionType>(std::atoi(rest.c_str()));
                            sCollectionsMgr.SendAddonSync(player, cType);
                        }
                        else if (action == "SUMMON")
                        {
                            uint32 spellId = std::strtoul(rest.c_str(), nullptr, 10);
                            if (spellId && sCollectionsMgr.HasAccountSpell(player->GetSession()->GetAccountId(), COLLECTION_TYPE_NONE, spellId))
                            {
                                uint32 dummyItem = 0;
                                CollectionType colType = sCollectionsMgr.GetCollectionTypeBySpell(spellId, dummyItem);
                                if (colType == COLLECTION_TYPE_MOUNT && !sCollectionsMgr.CanPlayerLearnMount(player, spellId))
                                {
                                    ChatHandler(player).SendSysMessage("|cffff0000You do not meet the level or riding requirements to summon this mount.|r");
                                }
                                else
                                {
                                    player->CastSpell(player, spellId, false);
                                }
                            }
                        }
                        else if (action == "SYNC")
                        {
                            sCollectionsMgr.ScanAndImportCharacterInventory(player);
                            sCollectionsMgr.SyncPlayerSpells(player);
                            sCollectionsMgr.SendAddonSync(player, COLLECTION_TYPE_NONE);
                        }

                        return false; // Suppress core chat processing
                    }
                }
            }
            catch (...)
            {
                return true;
            }
        }
        return true;
    }
};

// -----------------------------------------------------------------------------
// 4. Command Script (.collection)
// -----------------------------------------------------------------------------
class CollectionsCommandScript : public AllCommandScript
{
public:
    CollectionsCommandScript() : AllCommandScript("CollectionsCommandScript") {}

    bool CanExecuteCommand(ChatHandler* handler, char const* cmd, char const* argsIn) override
    {
        if (!cmd || !handler)
            return true;

        if (strcmp(cmd, "collection") != 0 && strcmp(cmd, "collections") != 0)
            return true;

        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
        {
            handler->SendSysMessage("This command can only be used in-game by a player.");
            return false;
        }

        std::string rest = argsIn ? argsIn : "";
        auto const takeWord = [&rest]() -> std::string
        {
            size_t const b = rest.find_first_not_of(' ');
            if (b == std::string::npos) { rest.clear(); return std::string(); }
            size_t const e = rest.find(' ', b);
            std::string word = rest.substr(b, e == std::string::npos ? std::string::npos : e - b);
            rest = e == std::string::npos ? std::string() : rest.substr(e + 1);
            return word;
        };

        std::string const sub = takeWord();

        if (sub.empty() || sub == "info")
        {
            uint32 accountId = player->GetSession()->GetAccountId();
            auto mounts = sCollectionsMgr.GetAccountEntries(accountId, COLLECTION_TYPE_MOUNT);
            auto pets   = sCollectionsMgr.GetAccountEntries(accountId, COLLECTION_TYPE_PET);
            auto toys   = sCollectionsMgr.GetAccountEntries(accountId, COLLECTION_TYPE_TOY);

            handler->SendSysMessage("|cffffcc00=== Account Vanity Collections ===|r");
            handler->PSendSysMessage("Mounts Unlocked: |cff00ff00%zu|r", mounts.size());
            handler->PSendSysMessage("Companion Pets:  |cff00ff00%zu|r", pets.size());
            handler->PSendSysMessage("Toys & Novelties:|cff00ff00%zu|r", toys.size());
            handler->SendSysMessage("Type |cffffcc00.collection list [mounts|pets|toys]|r or |cffffcc00.collection sync|r.");
            return false;
        }
        else if (sub == "sync")
        {
            handler->SendSysMessage("|cffffcc00Scanning character spellbook, bags, and bank for collections...|r");
            sCollectionsMgr.ScanAndImportCharacterInventory(player, handler);
            sCollectionsMgr.SyncPlayerSpells(player);
            sCollectionsMgr.SendAddonSync(player, COLLECTION_TYPE_NONE);
            return false;
        }
        else if (sub == "query")
        {
            std::string const catStr = takeWord();
            CollectionType type = COLLECTION_TYPE_NONE;
            if (catStr == "1" || catStr == "mounts" || catStr == "mount")
                type = COLLECTION_TYPE_MOUNT;
            else if (catStr == "2" || catStr == "pets" || catStr == "pet")
                type = COLLECTION_TYPE_PET;
            else if (catStr == "3" || catStr == "toys" || catStr == "toy")
                type = COLLECTION_TYPE_TOY;

            sCollectionsMgr.SendAddonSync(player, type);
            return false;
        }
        else if (sub == "list")
        {
            std::string const cat = takeWord();
            CollectionType type = COLLECTION_TYPE_MOUNT;
            if (cat == "pets" || cat == "pet")
                type = COLLECTION_TYPE_PET;
            else if (cat == "toys" || cat == "toy")
                type = COLLECTION_TYPE_TOY;

            uint32 accountId = player->GetSession()->GetAccountId();
            auto entries = sCollectionsMgr.GetAccountEntries(accountId, type);

            char const* title = (type == COLLECTION_TYPE_MOUNT) ? "Mounts" :
                                (type == COLLECTION_TYPE_PET)   ? "Pets" : "Toys";

            handler->PSendSysMessage("|cffffcc00=== Account %s Collection (%zu unlocked) ===|r", title, entries.size());

            size_t count = 0;
            for (auto const& entry : entries)
            {
                std::string name = "Unknown";
                if (SpellEntry const* spellInfo = sSpellMgr.GetSpellEntry(entry.spellId))
                    name = spellInfo->SpellName[0];

                bool isKnown = player->HasSpell(entry.spellId);
                handler->PSendSysMessage("[%u] %s - %s", entry.spellId, name.c_str(),
                    isKnown ? "|cff00ff00Learned|r" : "|cffff0000Not Learned|r");

                if (++count >= 20)
                {
                    handler->SendSysMessage("... [showing first 20 entries]");
                    break;
                }
            }
            return false;
        }
        else if (sub == "summon")
        {
            std::string const spellStr = takeWord();
            uint32 spellId = std::strtoul(spellStr.c_str(), nullptr, 10);
            if (!spellId)
            {
                handler->SendSysMessage("Usage: .collection summon <spellId>");
                return false;
            }

            uint32 accountId = player->GetSession()->GetAccountId();
            if (!sCollectionsMgr.HasAccountSpell(accountId, COLLECTION_TYPE_NONE, spellId))
            {
                handler->SendSysMessage("This spell is not unlocked in your account collection.");
                return false;
            }

            uint32 dummyItem = 0;
            CollectionType type = sCollectionsMgr.GetCollectionTypeBySpell(spellId, dummyItem);
            if (type == COLLECTION_TYPE_MOUNT)
            {
                if (!sCollectionsMgr.CanPlayerLearnMount(player, spellId))
                {
                    handler->SendSysMessage("|cffff0000You do not meet the level or riding requirements to summon this mount.|r");
                    return false;
                }
            }

            player->CastSpell(player, spellId, false);
            return false;
        }
        else
        {
            handler->SendSysMessage("|cffffcc00mod-twow-collections Commands:|r");
            handler->SendSysMessage("  .collection info - View collection totals");
            handler->SendSysMessage("  .collection list [mounts|pets|toys] - List unlocked collection items");
            handler->SendSysMessage("  .collection summon <spellId> - Summon an unlocked mount or pet");
            handler->SendSysMessage("  .collection sync - Retroactively import all inventory and known spells");
            return false;
        }

        return false;
    }
};

// -----------------------------------------------------------------------------
// Script Loader Instantiation
// -----------------------------------------------------------------------------
void AddCollectionsScriptHooks()
{
    new CollectionsPlayerScript();
    new CollectionsItemScript();
    new CollectionsServerScript();
    new CollectionsCommandScript();
}
