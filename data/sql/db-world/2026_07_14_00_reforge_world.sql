-- mod-reforge: the enchant-slot stat vehicle pool + the Reforge NPC.
--
-- Enchant pool (ARCHITECTURE §9): one custom SpellItemEnchantment per destination ItemModType,
-- ID = Reforge.EnchantBase (default 900200) + ItemModType. Each is a single STAT effect (Effect_1 = 5
-- = ITEM_ENCHANTMENT_TYPE_STAT) whose EffectArg_1 is the destination ItemModType. EffectPointsMin_1 is
-- a placeholder (1): the per-item moved amount is injected at apply time by the module's
-- OnPlayerApplyEnchantmentItemModsBefore hook, so a single shared row serves every reforged item.
-- Loaded into sSpellItemEnchantmentStore at startup via the DBC-from-DB path; no client patch needed.

DELETE FROM `spellitemenchantment_dbc` WHERE `ID` BETWEEN 900200 AND 900299;
INSERT INTO `spellitemenchantment_dbc` (`ID`, `Effect_1`, `EffectPointsMin_1`, `EffectArg_1`, `Name_Lang_enUS`, `Name_Lang_Mask`) VALUES
    (900203, 5, 1, 3,  'Reforged: Agility',       16712190),
    (900204, 5, 1, 4,  'Reforged: Strength',      16712190),
    (900205, 5, 1, 5,  'Reforged: Intellect',     16712190),
    (900206, 5, 1, 6,  'Reforged: Spirit',        16712190),
    (900207, 5, 1, 7,  'Reforged: Stamina',       16712190),
    (900212, 5, 1, 12, 'Reforged: Defense',       16712190),
    (900213, 5, 1, 13, 'Reforged: Dodge',         16712190),
    (900214, 5, 1, 14, 'Reforged: Parry',         16712190),
    (900215, 5, 1, 15, 'Reforged: Block',         16712190),
    (900231, 5, 1, 31, 'Reforged: Hit',           16712190),
    (900232, 5, 1, 32, 'Reforged: Crit',          16712190),
    (900235, 5, 1, 35, 'Reforged: Resilience',    16712190),
    (900236, 5, 1, 36, 'Reforged: Haste',         16712190),
    (900237, 5, 1, 37, 'Reforged: Expertise',     16712190),
    (900238, 5, 1, 38, 'Reforged: Attack Power',  16712190),
    (900243, 5, 1, 43, 'Reforged: MP5',           16712190),
    (900244, 5, 1, 44, 'Reforged: Armor Pen',     16712190),
    (900245, 5, 1, 45, 'Reforged: Spell Power',   16712190);

-- The Reforge NPC (entry Reforge.NpcEntry, default 900100). Friendly, gossip-only; the C++ gossip
-- script `reforge_npc` drives the reforge wizard for players without the client addon.
DELETE FROM `creature_template` WHERE `entry` = 900100;
INSERT INTO `creature_template` (`entry`, `name`, `subname`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `unit_class`, `type`, `RegenHealth`, `HealthModifier`, `DamageModifier`, `AIName`, `ScriptName`) VALUES
    (900100, 'Reforger', 'Reforging', 80, 80, 35, 1, 1, 7, 1, 1, 1, '', 'reforge_npc');

DELETE FROM `creature_template_model` WHERE `CreatureID` = 900100;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`) VALUES
    (900100, 0, 10045, 1, 1);

DELETE FROM `npc_text` WHERE `ID` = 900100;
INSERT INTO `npc_text` (`ID`, `text0_0`) VALUES
    (900100, 'Not quite the stats you wanted? I can shift a portion of one of an item''s stats into another -- for a price.');
