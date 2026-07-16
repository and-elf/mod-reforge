#include "ReforgeMgr.h"
#include "ReforgeStatMap.h"
#include "mod_reforge_loader.h"
#include "reforge/WeaponScale.h"

#include "Item.h"
#include "ItemTemplate.h"
#include "Player.h"
#include "ScriptMgr.h"
#include <algorithm>

using namespace Reforge;

// The enchant-slot stat vehicle (ARCHITECTURE §9). Two hooks, one per reforge direction:
//   - the item's native `from` stat is reduced as it is applied (val is int32&, so a clean subtract);
//   - the `to` stat is added by a prismatic-slot enchant whose per-item amount is injected here (the
//     shared enchant row only records which destination stat — amount is uint32& in this hook).
// Both fire during Player::_ApplyItemMods, so equip/unequip/login apply and reverse a reforge with no
// runtime DBC generation. State comes from the process-global reforge cache keyed by item GUID.
class ReforgeVehicleScript : public PlayerScript
{
public:
    ReforgeVehicleScript() : PlayerScript("ReforgeVehicleScript") { }

    void OnPlayerApplyItemModsBefore(Player* player, uint8 slot, bool /*apply*/, uint8 /*itemProtoStatNumber*/,
                                     uint32 statType, int32& val) override
    {
        if (!player)
            return;

        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item)
            return;

        std::optional<ReforgeMgr::ReforgeData> const rf = sReforgeMgr->GetReforge(item->GetGUID());
        if (!rf)
            return;

        std::optional<ItemStat> const mapped = FromItemModType(statType);
        if (!mapped)
            return;   // unmapped lines did not contribute to the budget: neither scaled nor reforged

        // Budget scaling (ARCHITECTURE §14): multiply every native, budget-contributing stat line by
        // the item's stored permille factor (1000 = identity). Only positive lines are part of the
        // budget; per-stat integer flooring is a close, safe approximation of the exact ScaleStatBlock.
        if (rf->scale != 1000 && val > 0)
            val = static_cast<int32>(static_cast<int64>(val) * static_cast<int64>(rf->scale) / 1000);

        // Stat-move reforge (§5): subtract the moved amount from the (now scaled) `from` stat line.
        // amount <= cap <= the scaled `from` value, so val stays >= 0 (clamped for the fraction==1 edge).
        if (*mapped == rf->from)
            val = std::max<int32>(0, val - static_cast<int32>(rf->amount));
    }

    void OnPlayerApplyEnchantmentItemModsBefore(Player* /*player*/, Item* item, EnchantmentSlot slot, bool /*apply*/,
                                                uint32 enchant_spell_id, uint32& enchant_amount) override
    {
        if (!item || slot != PRISMATIC_ENCHANTMENT_SLOT)
            return;

        std::optional<ReforgeMgr::ReforgeData> const rf = sReforgeMgr->GetReforge(item->GetGUID());
        if (!rf)
            return;

        // Our prismatic enchant's effect arg (spellid) is the destination ItemModType; inject the
        // per-item moved amount over the shared row's placeholder.
        if (enchant_spell_id == ToItemModType(rf->to))
            enchant_amount = rf->amount;
    }

    // Weapon-damage scaling on re-itemization (ARCHITECTURE §12, issue #7). Fires inside
    // Player::_ApplyWeaponDamage for each damage line, with mutable min/max references -- so a
    // re-itemized weapon's damage is rescaled server-side per item-instance, no client patch and no
    // ITEM_ENCHANTMENT_TYPE_DAMAGE row. Only weapons carrying a reforge row scale; the factor is a pure
    // function of the weapon's source level, the player's current level and config (no persisted state).
    void OnPlayerApplyWeaponDamage(Player* player, uint8 slot, ItemTemplate const* proto,
                                   float& minDamage, float& maxDamage, uint8 /*damageIndex*/) override
    {
        if (!player || !proto)
            return;

        ServerReforgeConfig const& cfg = sReforgeMgr->Config();
        if (!cfg.Enabled() || !cfg.WeaponScaleEnabled())
            return;

        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item)
            return;

        // Gate: only items that have been re-itemized (carry a reforge row) scale.
        if (!sReforgeMgr->GetReforge(item->GetGUID()))
            return;

        // Source = the weapon's own intended level (RequiredLevel, fall back to ItemLevel); target =
        // the player's current level (matches issue #8's target-level choice; see §12.3 reconciliation).
        uint32_t const fromLevel = proto->RequiredLevel > 0 ? proto->RequiredLevel : proto->ItemLevel;
        uint32_t const toLevel = player->GetLevel();

        WeaponDamage const scaled =
            ScaleWeaponDamage({ minDamage, maxDamage }, fromLevel, toLevel, cfg);
        minDamage = static_cast<float>(scaled.min);
        maxDamage = static_cast<float>(scaled.max);
    }
};

void AddReforgeVehicleScripts()
{
    new ReforgeVehicleScript();
}
