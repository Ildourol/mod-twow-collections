-- ============================================================================
-- mod-twow-collections: Account-Wide Collection Table (tw_char / CharacterDatabase)
-- ============================================================================
-- Stores unlocked mounts, companion pets, and toys for an account.
-- Keyed by account_id so all characters on the same account share collections.

CREATE TABLE IF NOT EXISTS `account_collection` (
    `account_id` INT(10) UNSIGNED NOT NULL COMMENT 'accounts.id',
    `collection_type` TINYINT(3) UNSIGNED NOT NULL COMMENT '1=Mount, 2=Pet, 3=Toy',
    `item_id` INT(10) UNSIGNED NOT NULL DEFAULT 0 COMMENT 'item_template.entry',
    `spell_id` INT(10) UNSIGNED NOT NULL COMMENT 'spell_template.entry',
    `learned_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    `learned_by_guid` INT(10) UNSIGNED NOT NULL DEFAULT 0 COMMENT 'characters.guid of unlocking character',
    PRIMARY KEY (`account_id`, `collection_type`, `item_id`, `spell_id`),
    KEY `idx_account` (`account_id`),
    KEY `idx_type_spell` (`collection_type`, `spell_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci
  COMMENT='Account-wide unlocked mounts, vanity pets, and toys';
