#include "CollectionsConfig.h"
#include "Config/Config.h"
#include "Log.h"
#include <sstream>

namespace
{
    void ParseSpellIdSet(std::string const& configValue, std::set<uint32>& outSet)
    {
        outSet.clear();
        if (configValue == "0" || configValue.empty())
            return;

        std::istringstream stream(configValue);
        std::string token;
        while (std::getline(stream, token, ','))
        {
            try
            {
                uint32 spellId = static_cast<uint32>(std::stoul(token));
                if (spellId != 0)
                    outSet.insert(spellId);
            }
            catch (...)
            {
                // Ignore malformed entries
            }
        }
    }
}

CollectionsConfig& CollectionsConfig::Instance()
{
    static CollectionsConfig instance;
    return instance;
}

void CollectionsConfig::LoadConfig()
{
    m_enabled = sConfig.GetBoolDefault("Collections.Enable", true);
    m_accountMounts = sConfig.GetBoolDefault("Collections.AccountWide.Mounts", true);
    m_accountPets = sConfig.GetBoolDefault("Collections.AccountWide.Pets", true);
    m_accountToys = sConfig.GetBoolDefault("Collections.AccountWide.Toys", true);
    m_mountAutoLearn = sConfig.GetBoolDefault("Collections.Mounts.AutoLearnOnLogin", true);
    m_mountLevelEnforced = sConfig.GetBoolDefault("Collections.Mounts.EnforceLevelRequirements", true);
    m_ridingSkillEnforced = sConfig.GetBoolDefault("Collections.Mounts.EnforceRidingSkill", true);
    m_showMountsToLowLevel = sConfig.GetBoolDefault("Collections.Mounts.ShowToLowLevel", true);
    m_showInSpellbookAtLowLevel = sConfig.GetBoolDefault("Collections.Mounts.ShowInSpellbookAtLowLevel", true);
    m_mountClassLimited = sConfig.GetBoolDefault("Collections.Mounts.LimitClass", true);
    m_mountRaceLimited = sConfig.GetBoolDefault("Collections.Mounts.LimitRace", false);
    m_allowAllMounts = sConfig.GetBoolDefault("Collections.Mounts.AllowAllMounts", false);

    ParseSpellIdSet(sConfig.GetStringDefault("Collections.Mounts.ExcludedSpellIDs", ""), m_excludedSpellIds);
    ParseSpellIdSet(sConfig.GetStringDefault("Collections.Mounts.EpicSpellIDs", ""), m_forcedEpicSpellIds);
    ParseSpellIdSet(sConfig.GetStringDefault("Collections.Mounts.NormalSpellIDs", ""), m_forcedNormalSpellIds);

    m_petAutoLearn = sConfig.GetBoolDefault("Collections.Pets.AutoLearnOnLogin", true);
    m_toyAutoLearn = sConfig.GetBoolDefault("Collections.Toys.AutoLearnOnLogin", true);
    m_consumeItem = sConfig.GetBoolDefault("Collections.ConsumeItemOnLearn", true);
    m_announce = sConfig.GetBoolDefault("Collections.AnnounceOnLearn", true);
    m_critterPets = sConfig.GetBoolDefault("Collections.CritterPets.Enable", true);
    m_logging = sConfig.GetBoolDefault("Collections.Logging", true);

    if (m_logging)
    {
        sLog.outString("[mod-twow-collections] Configuration loaded. Enabled: %u, Mounts: %u (ShowToLowLevel: %u, SpellbookLowLevel: %u, LimitClass: %u, LimitRace: %u), Pets: %u, Toys: %u",
            m_enabled, m_accountMounts, m_showMountsToLowLevel, m_showInSpellbookAtLowLevel, m_mountClassLimited, m_mountRaceLimited, m_accountPets, m_accountToys);
    }
}
