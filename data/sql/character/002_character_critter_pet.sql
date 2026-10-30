-- ============================================================================
-- mod-twow-collections: Character Critter Pet Records (tw_char / CharacterDatabase)
-- ============================================================================
-- Tracks every critter patted by character guid.

CREATE TABLE IF NOT EXISTS `character_critter_pet` (
    `guid`         INT(10) UNSIGNED NOT NULL DEFAULT 0
                   COMMENT 'characters.guid',
    `spawn`        INT(10) UNSIGNED NOT NULL DEFAULT 0
                   COMMENT 'creature guid: a tw_world.creature row, or a house critter',
    `entry`        INT(10) UNSIGNED NOT NULL DEFAULT 0
                   COMMENT 'creature_template.entry; the chart counts its name',
    `first_pet_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (`guid`, `spawn`),
    KEY `entry` (`entry`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci
  COMMENT='The critter chart: every critter spawn a character has patted';
