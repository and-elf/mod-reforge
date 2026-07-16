#include "ReforgeMgr.h"
#include "ReforgeStatMap.h"
#include "reforge/Charge.h"
#include "reforge/Currency.h"
#include "reforge/Reitemize.h"

#include "DatabaseEnv.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "StringFormat.h"

namespace Reforge
{
    namespace
    {
        std::string MoneyStr(uint32_t copper)
        {
            uint32_t const gold = copper / 10000;
            uint32_t const silver = (copper % 10000) / 100;
            uint32_t const bronze = copper % 100;
            return Acore::StringFormat("{}g {}s {}c", gold, silver, bronze);
        }

        std::string ItemName(uint32_t entry)
        {
            if (ItemTemplate const* proto = sObjectMgr->GetItemTemplate(entry))
                return proto->Name1;
            return Acore::StringFormat("item #{}", entry);
        }

        // When Reforge.RequireNpc is on, a reforge must be done near a Reforge NPC (the gossip path is
        // there by definition; the addon/command path is gated here). Off => reforge from anywhere.
        bool NearReforger(Player* player, ServerReforgeConfig const& cfg)
        {
            // Only probe for the NPC when the gate is on (preserves the short-circuit); the pure
            // ReforgeAllowedHere decides from the two booleans.
            bool const npcInRange = cfg.RequireNpc()
                && player->FindNearestCreature(cfg.NpcEntry(), cfg.NpcRange()) != nullptr;
            return ReforgeAllowedHere(cfg.RequireNpc(), npcInRange);
        }
    }

    ReforgeMgr* ReforgeMgr::instance()
    {
        static ReforgeMgr mgr;
        return &mgr;
    }

    void ReforgeMgr::LoadConfig()
    {
        _config.Load();
    }

    void ReforgeMgr::LoadFromDB()
    {
        _cache.clear();

        QueryResult result = CharacterDatabase.Query(
            "SELECT `item_guid`, `stat_from`, `stat_to`, `amount` FROM `character_item_reforge`");
        if (!result)
        {
            LOG_INFO("server.loading", ">> Loaded 0 item reforges.");
            return;
        }

        uint32_t loaded = 0;
        do
        {
            Field* fields = result->Fetch();
            uint32_t const guid = fields[0].Get<uint32>();
            uint8_t const from = fields[1].Get<uint8>();
            uint8_t const to = fields[2].Get<uint8>();
            uint32_t const amount = fields[3].Get<uint32>();

            if (from >= static_cast<uint8_t>(ItemStat::COUNT) || to >= static_cast<uint8_t>(ItemStat::COUNT) || amount == 0)
                continue;

            _cache[guid] = { static_cast<ItemStat>(from), static_cast<ItemStat>(to), amount };
            ++loaded;
        } while (result->NextRow());

        LOG_INFO("server.loading", ">> Loaded {} item reforges.", loaded);
    }

    std::optional<ReforgeMgr::ReforgeData> ReforgeMgr::GetReforge(ObjectGuid itemGuid) const
    {
        auto const it = _cache.find(itemGuid.GetCounter());
        if (it == _cache.end())
            return std::nullopt;
        return it->second;
    }

    void ReforgeMgr::Reapply(Player* player, Item* item, bool wasEquipped) const
    {
        if (player && wasEquipped)
            player->_ApplyItemMods(item, item->GetSlot(), true);
    }

    bool ReforgeMgr::ApplyReforge(Player* player, Item* item, ItemStat from, ItemStat to, uint32_t amount,
                                  uint32_t currencyEntry, std::string& outMsg)
    {
        if (!_config.Enabled())
        {
            outMsg = "Reforging is disabled on this realm.";
            return false;
        }
        if (!player || !item)
        {
            outMsg = "No item to reforge.";
            return false;
        }
        if (!NearReforger(player, _config))
        {
            outMsg = "You must be near a Reforger to do that.";
            return false;
        }

        ItemTemplate const* proto = item->GetTemplate();
        if (_config.IsItemBlocked(proto))
        {
            outMsg = "This item cannot be reforged.";
            return false;
        }

        StatBlock const block = BuildStatBlock(proto);
        ItemChassis const chassis = BuildChassis(proto);

        // Nice, specific messages first; the pure core is the final authority (§5).
        if (!_config.IsLegalStat(from))
        {
            outMsg = Acore::StringFormat("You cannot reforge away from {}.", StatName(from));
            return false;
        }
        if (block.Get(from) == 0)
        {
            outMsg = Acore::StringFormat("This item has no {} to reforge.", StatName(from));
            return false;
        }
        if (from == to)
        {
            outMsg = "Pick two different stats.";
            return false;
        }
        if (!_config.IsLegalStat(to))
        {
            outMsg = Acore::StringFormat("{} is not a legal reforge destination.", StatName(to));
            return false;
        }

        uint32_t const cap = ReforgeCap(block.Get(from), _config.ReforgeMaxFraction());
        if (amount == 0 || amount > cap)
        {
            outMsg = Acore::StringFormat("You may move at most {} {} (= {}% of {}).",
                cap, StatName(from), static_cast<uint32_t>(_config.ReforgeMaxFraction() * 100.0), StatName(from));
            return false;
        }

        // Final authority: identical rule set for every path. Qualify ApplyReforge so it resolves to
        // the free function, not this member.
        ReforgeOp const move{ from, to, amount };
        if (!::Reforge::ApplyReforge(block, move, chassis, _config).has_value())
        {
            outMsg = "That reforge is not allowed.";
            return false;
        }

        // Resolve the chosen currency and verify funds BEFORE charging anything. The decision (accepted?
        // affordable?) is the pure core's (PlanCharge); the adapter only reads the balance and, on Ok,
        // performs the matching deduction.
        bool const isGold = currencyEntry == 0;
        uint64 const playerHas = isGold ? player->GetMoney() : player->GetItemCount(currencyEntry);
        ChargePlan const plan = PlanCharge(_config.AcceptedCurrencies(), currencyEntry, playerHas);
        if (plan.status == ChargeStatus::NotAccepted)
        {
            outMsg = "That is not an accepted currency here.";
            return false;
        }
        if (plan.status == ChargeStatus::Insufficient)
        {
            outMsg = isGold
                ? Acore::StringFormat("Reforging costs {}.", MoneyStr(plan.amount))
                : Acore::StringFormat("Reforging costs {}x {}.", plan.amount, ItemName(currencyEntry));
            return false;
        }

        // Charge.
        if (isGold)
            player->ModifyMoney(-static_cast<int32>(plan.amount));
        else
            player->DestroyItemCount(currencyEntry, plan.amount, true);

        // Apply the stat vehicle: remove old mods, update cache + prismatic marker, re-apply.
        uint32_t const key = item->GetGUID().GetCounter();
        bool const equipped = item->IsEquipped();
        if (equipped)
            player->_ApplyItemMods(item, item->GetSlot(), false);

        _cache[key] = { from, to, amount };
        item->SetEnchantment(PRISMATIC_ENCHANTMENT_SLOT, _config.EnchantBase() + ToItemModType(to), 0, 0);

        Reapply(player, item, equipped);

        CharacterDatabase.Execute(
            "REPLACE INTO `character_item_reforge` (`item_guid`, `stat_from`, `stat_to`, `amount`) VALUES ({}, {}, {}, {})",
            key, static_cast<uint32>(from), static_cast<uint32>(to), amount);
        item->SetState(ITEM_CHANGED, player);

        outMsg = Acore::StringFormat("Reforged {} {} into {}.", amount, StatName(from), StatName(to));
        return true;
    }

    bool ReforgeMgr::ClearReforge(Player* player, Item* item, std::string& outMsg)
    {
        if (!player || !item)
        {
            outMsg = "No item selected.";
            return false;
        }
        if (!NearReforger(player, _config))
        {
            outMsg = "You must be near a Reforger to do that.";
            return false;
        }

        uint32_t const key = item->GetGUID().GetCounter();
        if (!_cache.count(key))
        {
            outMsg = "That item is not reforged.";
            return false;
        }

        bool const equipped = item->IsEquipped();
        if (equipped)
            player->_ApplyItemMods(item, item->GetSlot(), false);

        _cache.erase(key);
        item->SetEnchantment(PRISMATIC_ENCHANTMENT_SLOT, 0, 0, 0);

        Reapply(player, item, equipped);

        CharacterDatabase.Execute("DELETE FROM `character_item_reforge` WHERE `item_guid` = {}", key);
        item->SetState(ITEM_CHANGED, player);

        outMsg = "Reforge removed.";
        return true;
    }

    std::vector<Addon::ItemReforge> ReforgeMgr::EquippedReforges(Player* player) const
    {
        std::vector<Addon::ItemReforge> out;
        if (!player)
            return out;

        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (!item)
                continue;
            if (std::optional<ReforgeData> const rf = GetReforge(item->GetGUID()))
                out.push_back({ slot, static_cast<uint8_t>(rf->from), static_cast<uint8_t>(rf->to), rf->amount });
        }
        return out;
    }
}
