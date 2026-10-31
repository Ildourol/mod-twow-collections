#pragma once

#include "Common.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <set>
#include <mutex>

enum CollectionType : uint8
{
    COLLECTION_TYPE_NONE  = 0,
    COLLECTION_TYPE_MOUNT = 1,
    COLLECTION_TYPE_PET   = 2,
    COLLECTION_TYPE_TOY   = 3,
    COLLECTION_TYPE_MAX   = 4
};

// Tortoise-WoW riding skill tiers
constexpr uint32 RIDING_SKILL_APPRENTICE = 75;
constexpr uint32 RIDING_SKILL_JOURNEYMAN = 150;

struct CollectionEntry
{
    uint32 itemId{0};
    uint32 spellId{0};
    CollectionType type{COLLECTION_TYPE_NONE};
};

#define TW_COLLECTIONS_ADDON_PREFIX "TW_COLLECT"
