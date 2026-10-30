#include "CollectionsConfig.h"
#include "Config/Config.h"
#include "Log.h"

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
    m_petAutoLearn = sConfig.GetBoolDefault("Collections.Pets.AutoLearnOnLogin", true);
    m_toyAutoLearn = sConfig.GetBoolDefault("Collections.Toys.AutoLearnOnLogin", true);
    m_consumeItem = sConfig.GetBoolDefault("Collections.ConsumeItemOnLearn", true);
    m_announce = sConfig.GetBoolDefault("Collections.AnnounceOnLearn", true);
    m_critterPets = sConfig.GetBoolDefault("Collections.CritterPets.Enable", true);
    m_logging = sConfig.GetBoolDefault("Collections.Logging", true);

    if (m_logging)
    {
        sLog.outString("[mod-twow-collections] Configuration loaded. Enabled: %u, Mounts: %u, Pets: %u, Toys: %u",
            m_enabled, m_accountMounts, m_accountPets, m_accountToys);
    }
}
