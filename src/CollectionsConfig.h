#pragma once

#include "Common.h"

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
    bool IsPetAutoLearnEnabled() const { return m_petAutoLearn; }
    bool IsToyAutoLearnEnabled() const { return m_toyAutoLearn; }
    bool IsConsumeItemEnabled() const { return m_consumeItem; }
    bool IsAnnounceEnabled() const { return m_announce; }
    bool IsCritterPetsEnabled() const { return m_critterPets; }
    bool IsLoggingEnabled() const { return m_logging; }

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
    bool m_petAutoLearn{true};
    bool m_toyAutoLearn{true};
    bool m_consumeItem{true};
    bool m_announce{true};
    bool m_critterPets{true};
    bool m_logging{true};
};

#define sCollectionsConfig CollectionsConfig::Instance()
