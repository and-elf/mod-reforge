-- mod-reforge: level/rarity budget scaling (ARCHITECTURE §12, issues #8/#6). Adds the per-item stat
-- budget scale factor, in PERMILLE (1000 = identity / no scaling). The reforge normalises the item's
-- budget to the reforging player's level + quality; the stat vehicle multiplies every native stat line
-- by this factor at equip time. Existing rows keep the identity factor (no behaviour change).

ALTER TABLE `character_item_reforge`
    ADD COLUMN `scale` INT UNSIGNED NOT NULL DEFAULT 1000 AFTER `amount`;
