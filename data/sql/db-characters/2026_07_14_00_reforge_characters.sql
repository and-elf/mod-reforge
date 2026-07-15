-- mod-reforge: per-item reforge state (ARCHITECTURE §9). One row per reforged item, keyed by the
-- item_instance GUID. This is the single source of truth for the moved amount; the item's prismatic
-- enchant slot only records which destination stat. `stat_from`/`stat_to` are Reforge::ItemStat
-- ordinals. Loaded into the reforge cache at startup.

CREATE TABLE IF NOT EXISTS `character_item_reforge` (
    `item_guid` INT UNSIGNED NOT NULL,
    `stat_from` TINYINT UNSIGNED NOT NULL,
    `stat_to` TINYINT UNSIGNED NOT NULL,
    `amount` INT UNSIGNED NOT NULL,
    PRIMARY KEY (`item_guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
