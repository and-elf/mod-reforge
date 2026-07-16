#include "ReforgeAddon.h"
#include "ReforgeMgr.h"
#include "ReforgeStatMap.h"
#include "mod_reforge_loader.h"
#include "reforge/Charge.h"

#include "Chat.h"
#include "Creature.h"
#include "GossipDef.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptedGossip.h"
#include "ScriptMgr.h"
#include "StringFormat.h"

#include <unordered_map>

using namespace Reforge;

namespace
{
    constexpr uint32 REFORGE_NPCTEXT = 900100;   // npc_text row seeded by the module SQL

    enum GossipAction : uint32
    {
        ACTION_MAIN        = 1,
        ACTION_CLEAR       = 50,
        ACTION_ITEM_BASE   = 100,   // + equipment slot
        ACTION_FROM_BASE   = 200,   // + ItemStat ordinal
        ACTION_TO_BASE     = 300,   // + ItemStat ordinal
        ACTION_AMOUNT_BASE = 400,   // + amount-option index
        ACTION_CUR_BASE    = 500    // + currency index
    };

    // Transient per-player wizard state (slot/from/to/amount). Cleared implicitly on the next hello.
    struct Selection
    {
        uint8 slot = 0;
        ItemStat from = ItemStat::HitRating;
        ItemStat to = ItemStat::HitRating;
        uint32 amount = 0;
    };
    std::unordered_map<ObjectGuid, Selection> g_sel;

    std::string MoneyStr(uint32 copper)
    {
        return Acore::StringFormat("{}g {}s {}c", copper / 10000, (copper % 10000) / 100, copper % 100);
    }

    std::string CurrencyLabel(CurrencyCost const& c)
    {
        if (c.entry == 0)
            return Acore::StringFormat("Pay {}", MoneyStr(c.count));
        std::string name = Acore::StringFormat("item #{}", c.entry);
        if (ItemTemplate const* proto = sObjectMgr->GetItemTemplate(c.entry))
            name = proto->Name1;
        return Acore::StringFormat("Pay {}x {}", c.count, name);
    }

    void SendMain(Player* player, Creature* creature)
    {
        ClearGossipMenuFor(player);
        ServerReforgeConfig const& cfg = sReforgeMgr->Config();

        bool any = false;
        for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (!item)
                continue;

            StatBlock const block = BuildStatBlock(item->GetTemplate());
            bool hasSource = false;
            for (std::size_t i = 0; i < static_cast<std::size_t>(ItemStat::COUNT); ++i)
                if (cfg.IsLegalStat(static_cast<ItemStat>(i)) && block.Get(static_cast<ItemStat>(i)) > 0)
                    hasSource = true;
            if (!hasSource)
                continue;

            ItemTemplate const* proto = item->GetTemplate();
            std::string label = proto ? proto->Name1 : Acore::StringFormat("Item in slot {}", slot);
            if (sReforgeMgr->GetReforge(item->GetGUID()))
                label += " (reforged)";
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, label, GOSSIP_SENDER_MAIN, ACTION_ITEM_BASE + slot);
            any = true;
        }

        if (!any)
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "You have no reforgeable equipped items.", GOSSIP_SENDER_MAIN, ACTION_MAIN);

        SendGossipMenuFor(player, REFORGE_NPCTEXT, creature->GetGUID());
    }

    void SendItem(Player* player, Creature* creature, uint8 slot)
    {
        ClearGossipMenuFor(player);
        ServerReforgeConfig const& cfg = sReforgeMgr->Config();

        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item)
        {
            SendMain(player, creature);
            return;
        }
        g_sel[player->GetGUID()].slot = slot;

        StatBlock const block = BuildStatBlock(item->GetTemplate());
        if (sReforgeMgr->GetReforge(item->GetGUID()))
            AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "Remove current reforge", GOSSIP_SENDER_MAIN, ACTION_CLEAR);

        for (std::size_t i = 0; i < static_cast<std::size_t>(ItemStat::COUNT); ++i)
        {
            ItemStat const stat = static_cast<ItemStat>(i);
            uint32 const value = block.Get(stat);
            if (!cfg.IsLegalStat(stat) || value == 0)
                continue;
            AddGossipItemFor(player, GOSSIP_ICON_CHAT,
                Acore::StringFormat("Reduce {} ({})", StatName(stat), value), GOSSIP_SENDER_MAIN, ACTION_FROM_BASE + i);
        }

        AddGossipItemFor(player, GOSSIP_ICON_TALK, "< Back", GOSSIP_SENDER_MAIN, ACTION_MAIN);
        SendGossipMenuFor(player, REFORGE_NPCTEXT, creature->GetGUID());
    }

    void SendTo(Player* player, Creature* creature)
    {
        ClearGossipMenuFor(player);
        ServerReforgeConfig const& cfg = sReforgeMgr->Config();
        Selection const& sel = g_sel[player->GetGUID()];

        for (std::size_t i = 0; i < static_cast<std::size_t>(ItemStat::COUNT); ++i)
        {
            ItemStat const stat = static_cast<ItemStat>(i);
            if (!cfg.IsLegalStat(stat) || stat == sel.from)
                continue;
            AddGossipItemFor(player, GOSSIP_ICON_CHAT,
                Acore::StringFormat("... into {}", StatName(stat)), GOSSIP_SENDER_MAIN, ACTION_TO_BASE + i);
        }

        AddGossipItemFor(player, GOSSIP_ICON_TALK, "< Back", GOSSIP_SENDER_MAIN, ACTION_ITEM_BASE + sel.slot);
        SendGossipMenuFor(player, REFORGE_NPCTEXT, creature->GetGUID());
    }

    void SendAmount(Player* player, Creature* creature)
    {
        ClearGossipMenuFor(player);
        Selection const& sel = g_sel[player->GetGUID()];

        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, sel.slot);
        StatBlock const block = item ? BuildStatBlock(item->GetTemplate()) : StatBlock{};
        uint32 const cap = ReforgeCap(block.Get(sel.from), sReforgeMgr->Config().ReforgeMaxFraction());

        std::vector<uint32> const options = AmountOptions(cap);
        for (std::size_t i = 0; i < options.size(); ++i)
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG,
                Acore::StringFormat("Move {} {} -> {}", options[i], StatName(sel.from), StatName(sel.to)),
                GOSSIP_SENDER_MAIN, ACTION_AMOUNT_BASE + i);

        AddGossipItemFor(player, GOSSIP_ICON_TALK, "< Back", GOSSIP_SENDER_MAIN, ACTION_FROM_BASE + static_cast<uint32>(sel.from));
        SendGossipMenuFor(player, REFORGE_NPCTEXT, creature->GetGUID());
    }

    void SendCurrency(Player* player, Creature* creature)
    {
        ClearGossipMenuFor(player);
        std::vector<CurrencyCost> const& currencies = sReforgeMgr->Config().AcceptedCurrencies();
        for (std::size_t i = 0; i < currencies.size(); ++i)
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, CurrencyLabel(currencies[i]), GOSSIP_SENDER_MAIN, ACTION_CUR_BASE + i);

        AddGossipItemFor(player, GOSSIP_ICON_TALK, "< Cancel", GOSSIP_SENDER_MAIN, ACTION_MAIN);
        SendGossipMenuFor(player, REFORGE_NPCTEXT, creature->GetGUID());
    }
}

class reforge_npc : public CreatureScript
{
public:
    reforge_npc() : CreatureScript("reforge_npc") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (!sReforgeMgr->Config().Enabled())
            return false;
        g_sel.erase(player->GetGUID());
        SendMain(player, creature);
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        ChatHandler handler(player->GetSession());
        Selection& sel = g_sel[player->GetGUID()];

        if (action >= ACTION_CUR_BASE)
        {
            std::vector<CurrencyCost> const& currencies = sReforgeMgr->Config().AcceptedCurrencies();
            std::size_t const idx = action - ACTION_CUR_BASE;
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, sel.slot);
            std::string message = "Reforge: selection expired.";
            bool ok = false;
            if (item && idx < currencies.size())
                ok = sReforgeMgr->ApplyReforge(player, item, sel.from, sel.to, sel.amount, currencies[idx].entry, message);
            handler.PSendSysMessage("{}", message);
            if (ok)
                AddonPush::SendState(player);
            CloseGossipMenuFor(player);
            return true;
        }
        if (action >= ACTION_AMOUNT_BASE)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, sel.slot);
            StatBlock const block = item ? BuildStatBlock(item->GetTemplate()) : StatBlock{};
            uint32 const cap = ReforgeCap(block.Get(sel.from), sReforgeMgr->Config().ReforgeMaxFraction());
            std::vector<uint32> const options = AmountOptions(cap);
            std::size_t const idx = action - ACTION_AMOUNT_BASE;
            if (idx < options.size())
            {
                sel.amount = options[idx];
                SendCurrency(player, creature);
            }
            else
                SendMain(player, creature);
            return true;
        }
        if (action >= ACTION_TO_BASE)
        {
            sel.to = static_cast<ItemStat>(action - ACTION_TO_BASE);
            SendAmount(player, creature);
            return true;
        }
        if (action >= ACTION_FROM_BASE)
        {
            sel.from = static_cast<ItemStat>(action - ACTION_FROM_BASE);
            SendTo(player, creature);
            return true;
        }
        if (action >= ACTION_ITEM_BASE)
        {
            SendItem(player, creature, static_cast<uint8>(action - ACTION_ITEM_BASE));
            return true;
        }
        if (action == ACTION_CLEAR)
        {
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, sel.slot);
            std::string message;
            bool const ok = item && sReforgeMgr->ClearReforge(player, item, message);
            handler.PSendSysMessage("{}", ok ? message : "Reforge: nothing to clear.");
            if (ok)
                AddonPush::SendState(player);
            SendMain(player, creature);
            return true;
        }

        SendMain(player, creature);
        return true;
    }
};

void AddReforgeGossipScripts()
{
    new reforge_npc();
}
