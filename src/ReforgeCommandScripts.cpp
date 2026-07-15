#include "ReforgeAddon.h"
#include "ReforgeMgr.h"
#include "ReforgeStatMap.h"
#include "mod_reforge_loader.h"

#include "Chat.h"
#include "CommandScript.h"
#include "Item.h"
#include "Optional.h"
#include "Player.h"
#include "StringFormat.h"

using namespace Acore::ChatCommands;
using namespace Reforge;

namespace
{
    // Custom RBAC permission for the reforge command, registered by the module's db-auth SQL and
    // linked into the "Player Commands" role so ordinary players (and their client addon, which drives
    // this same command over the AzerothCore addon channel) may use it.
    constexpr uint32 REFORGE_RBAC_PERM = 100000;

    Item* ResolveEquipped(ChatHandler* handler, uint8 slot)
    {
        Player* player = handler->GetPlayer();
        if (!player || slot >= EQUIPMENT_SLOT_END)
        {
            handler->PSendSysMessage("Reforge: invalid equipment slot {}.", slot);
            return nullptr;
        }
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item)
            handler->PSendSysMessage("Reforge: nothing equipped in slot {}.", slot);
        return item;
    }
}

// GM- and player-facing command surface. Also the client addon's inbound API: the addon issues these
// exact commands over the built-in AzerothCore addon command channel (§11).
class reforge_commandscript : public CommandScript
{
public:
    reforge_commandscript() : CommandScript("reforge_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable reforgeTable =
        {
            { "do",    HandleReforgeDoCommand,    REFORGE_RBAC_PERM, Console::No },
            { "clear", HandleReforgeClearCommand, REFORGE_RBAC_PERM, Console::No },
            { "list",  HandleReforgeListCommand,  REFORGE_RBAC_PERM, Console::No },
            { "sync",  HandleReforgeSyncCommand,  REFORGE_RBAC_PERM, Console::No }
        };
        static ChatCommandTable commandTable =
        {
            { "reforge", reforgeTable }
        };
        return commandTable;
    }

    // .reforge do <slot> <from> <to> <amount> [currencyEntry]
    static bool HandleReforgeDoCommand(ChatHandler* handler, uint8 slot, std::string from, std::string to,
                                       uint32 amount, Optional<uint32> currency)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        Item* item = ResolveEquipped(handler, slot);
        if (!item)
            return true;

        std::optional<ItemStat> const fromStat = StatFromName(from);
        std::optional<ItemStat> const toStat = StatFromName(to);
        if (!fromStat || !toStat)
        {
            handler->PSendSysMessage("Reforge: unknown stat name. Use e.g. hit, crit, haste, spirit.");
            return true;
        }

        // Default to the first accepted currency when the client omits one.
        uint32 currencyEntry = 0;
        if (currency)
            currencyEntry = *currency;
        else if (!sReforgeMgr->Config().AcceptedCurrencies().empty())
            currencyEntry = sReforgeMgr->Config().AcceptedCurrencies().front().entry;

        std::string message;
        bool const ok = sReforgeMgr->ApplyReforge(player, item, *fromStat, *toStat, amount, currencyEntry, message);
        handler->PSendSysMessage("{}", message);
        if (ok)
            AddonPush::SendState(player);
        return true;
    }

    // .reforge clear <slot>
    static bool HandleReforgeClearCommand(ChatHandler* handler, uint8 slot)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        Item* item = ResolveEquipped(handler, slot);
        if (!item)
            return true;

        std::string message;
        bool const ok = sReforgeMgr->ClearReforge(player, item, message);
        handler->PSendSysMessage("{}", message);
        if (ok)
            AddonPush::SendState(player);
        return true;
    }

    // .reforge list
    static bool HandleReforgeListCommand(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;

        auto const reforges = sReforgeMgr->EquippedReforges(player);
        if (reforges.empty())
        {
            handler->PSendSysMessage("Reforge: none of your equipped items are reforged.");
            return true;
        }

        for (Addon::ItemReforge const& r : reforges)
            handler->PSendSysMessage("  slot {}: {} {} -> {}", r.slot, r.amount,
                StatName(static_cast<ItemStat>(r.from)), StatName(static_cast<ItemStat>(r.to)));
        return true;
    }

    // .reforge sync — re-push the addon state (the addon calls this on load / to refresh).
    static bool HandleReforgeSyncCommand(ChatHandler* handler)
    {
        Player* player = handler->GetPlayer();
        if (!player)
            return false;
        AddonPush::SendState(player);
        handler->PSendSysMessage("Reforge: state synced.");
        return true;
    }
};

void AddReforgeCommandScripts()
{
    new reforge_commandscript();
}
