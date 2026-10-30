-- ============================================================================
-- mod-twow-collections: Mount Collection Gap Fix (tw_world / WorldDatabase)
-- ============================================================================
-- Fixes Swift Blood Kodo (Item 80456 -> Spell 45044)
-- Ported from comfycraft-source/sql/custom/039_collection_mount_gaps.sql

DELETE FROM `collection_mount` WHERE `itemId` = 80456;
INSERT INTO `collection_mount` (`itemId`, `spellId`) VALUES
  (80456, 45044); -- Swift Blood Kodo -> "Swift Blood Kodo"
