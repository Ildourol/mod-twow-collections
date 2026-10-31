#pragma once

#include "CollectionsCommon.h"

class CollectionsConfig
{
public:
    static CollectionsConfig& Instance();

    void LoadConfig();

    bool IsEnabled() const { return m_enabled; }
    bool IsAccountMountsEnabled() const { return m_accountMounts; }
    bool IsAccountPetsEnabled() const { return m_accountPets; }
    bool IsAccountToysEnabled() const { return m_accountToys; }
    bool IsMountAutoLearnEnabled() const { return m_mountAutoLearn; }
    bool IsMountLevelEnforced() const { return m_mountLevelEnforced; }
    bool IsRidingSkillEnforced() const { return m_ridingSkillEnforced; }
    bool IsShowMountsToLowLevel() const { return m_showMountsToLowLevel; }
    bool IsShowInSpellbookAtLowLevel() const { return m_showInSpellbookAtLowLevel; }
    bool IsMountClassLimited() const { return m_mountClassLimited; }
    bool IsMountRaceLimited() const { return m_mountRaceLimited; }
    bool IsAllowAllMounts() const { return m_allowAllMounts; }
    bool IsPetAutoLearnEnabled() const { return m_petAutoLearn; }
    bool IsToyAutoLearnEnabled() const { return m_toyAutoLearn; }
    bool IsConsumeItemEnabled() const { return m_consumeItem; }
    bool IsAnnounceEnabled() const { return m_announce; }
    bool IsCritterPetsEnabled() const { return m_critterPets; }
    bool IsLoggingEnabled() const { return m_logging; }

    std::set<uint32> const& GetExcludedSpellIds() const { return m_excludedSpellIds; }
    std::set<uint32> const& GetForcedEpicSpellIds() const { return m_forcedEpicSpellIds; }
    std::set<uint32> const& GetForcedNormalSpellIds() const { return m_forcedNormalSpellIds; }

private:
    CollectionsConfig() = default;

    bool m_enabled{true};
    bool m_accountMounts{true};
    bool m_accountPets{true};
    bool m_accountToys{true};
    bool m_mountAutoLearn{true};
    bool m_mountLevelEnforced{true};
    bool m_ridingSkillEnforced{true};
    bool m_showMountsToLowLevel{true};
    bool m_showInSpellbookAtLowLevel{true};
    bool m_mountClassLimited{true};
    bool m_mountRaceLimited{false};
    bool m_allowAllMounts{false};
    bool m_petAutoLearn{true};
    bool m_toyAutoLearn{true};
    bool m_consumeItem{true};
    bool m_announce{true};
    bool m_critterPets{true};
    bool m_logging{true};

    std::set<uint32> m_excludedSpellIds;
    std::set<uint32> m_forcedEpicSpellIds;
    std::set<uint32> m_forcedNormalSpellIds;
};

#define sCollectionsConfig CollectionsConfig::Instance()
