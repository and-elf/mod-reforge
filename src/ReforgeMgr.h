#ifndef MOD_REFORGE_SRC_REFORGEMGR_H
#define MOD_REFORGE_SRC_REFORGEMGR_H

#include "ServerReforgeConfig.h"
#include "reforge/Protocol.h"
#include "reforge/Stats.h"
#include "ObjectGuid.h"
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class Item;
class Player;

namespace Reforge
{
    // Server-side reforge state + operations. Singleton (sReforgeMgr). Holds the config snapshot and a
    // per-item reforge cache keyed by item GUID counter, backed by `character_item_reforge`. Never
    // stores a live Player*/Item*: every op takes them from the caller and resolves state by GUID.
    //
    // Validation is delegated to the pure core (Reforge::ApplyReforge) so the NPC, command and addon
    // paths share identical rules; this class only wires items/currency/persistence/the stat vehicle.
    class ReforgeMgr
    {
    public:
        struct ReforgeData
        {
            ItemStat from = ItemStat::Spirit;
            ItemStat to = ItemStat::HitRating;
            uint32_t amount = 0;
            uint32_t scale = 1000;   // level/rarity budget scale factor in permille (1000 = identity, §14)
        };

        static ReforgeMgr* instance();

        void LoadConfig();                 // snapshot Reforge.* from sConfigMgr (startup + reload)
        void LoadFromDB();                 // fill the cache from character_item_reforge (startup)
        ServerReforgeConfig const& Config() const { return _config; }

        // The reforge on the item with this GUID, or nullopt. Used by the apply hooks.
        std::optional<ReforgeData> GetReforge(ObjectGuid itemGuid) const;

        // Apply a player-directed reforge, paying with currency `currencyEntry` (0 = gold). Returns
        // true on success; on failure returns false with a human-readable reason in `outMsg`. Nothing
        // is charged or persisted unless the whole operation succeeds.
        bool ApplyReforge(Player* player, Item* item, ItemStat from, ItemStat to, uint32_t amount,
                          uint32_t currencyEntry, std::string& outMsg);

        // Remove the reforge from an item (free). Returns false with a reason if it was not reforged.
        bool ClearReforge(Player* player, Item* item, std::string& outMsg);

        // The active reforges on the player's equipped items, for the §11 addon ITEM frame.
        std::vector<Addon::ItemReforge> EquippedReforges(Player* player) const;

    private:
        ReforgeMgr() = default;

        // Refresh an equipped item's mods so a cache/enchant change takes effect immediately. Drives
        // both stat-vehicle hooks (proto stats + prismatic enchant) via a remove/re-apply.
        void Reapply(Player* player, Item* item, bool wasEquipped) const;

        ServerReforgeConfig _config;
        std::unordered_map<uint32_t, ReforgeData> _cache;   // key = item GUID counter
    };
}

#define sReforgeMgr Reforge::ReforgeMgr::instance()

#endif // MOD_REFORGE_SRC_REFORGEMGR_H
