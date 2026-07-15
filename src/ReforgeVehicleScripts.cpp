#include "ReforgeMgr.h"
#include "ReforgeStatMap.h"
#include "mod_reforge_loader.h"

#include "Item.h"
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
        if (mapped && *mapped == rf->from)
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
};

void AddReforgeVehicleScripts()
{
    new ReforgeVehicleScript();
}
