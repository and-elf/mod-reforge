#ifndef MOD_REFORGE_SRC_REFORGEADDON_H
#define MOD_REFORGE_SRC_REFORGEADDON_H

class Player;

// §11 outbound transport: push RFRG state frames to a player's client addon over LANG_ADDON whispers.
// Server-push only (inbound is the built-in AzerothCore addon command channel → the `.reforge`
// command). The gossip / command handlers call SendState after a successful op so the UI refreshes.
namespace Reforge::AddonPush
{
    bool Enabled();

    void SendLogin(Player* player);   // HELLO + CFG + CUR + ITEM
    void SendState(Player* player);   // CFG + CUR + ITEM (refresh)
}

#endif // MOD_REFORGE_SRC_REFORGEADDON_H
