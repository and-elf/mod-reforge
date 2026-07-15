#include "ReforgeAddon.h"
#include "ReforgeMgr.h"
#include "mod_reforge_loader.h"
#include "reforge/Protocol.h"

#include "Chat.h"
#include "Log.h"
#include "Player.h"
#include "ScriptMgr.h"

using namespace Reforge;

namespace
{
    // Server -> client addon data goes out as a LANG_ADDON *whisper* (never CHAT_MSG_ADDON, which
    // crashes a receiving 3.3.5a client). The client routes LANG_ADDON whispers to the addon's
    // CHAT_MSG_ADDON handler and never shows them in chat. Frames are bounded to MaxFrame by design.
    void Send(Player* player, std::string const& frame)
    {
        if (!player || frame.empty() || frame.size() > Addon::MaxFrame)
            return;

        WorldPacket data;
        ChatHandler::BuildChatPacket(data, CHAT_MSG_WHISPER, LANG_ADDON, player->GetGUID(), player->GetGUID(), frame, 0);
        player->SendDirectMessage(&data);
    }

    void SendCurrencies(Player* player)
    {
        bool truncated = false;
        std::string const frame = Addon::EncodeCurrencies(sReforgeMgr->Config().AcceptedCurrencies(), truncated);
        if (truncated)
            LOG_WARN("module.reforge.addon", "Currency list truncated to fit the addon frame for {}.", player->GetName());
        Send(player, frame);
    }

    void SendConfig(Player* player)
    {
        Addon::ConfigFrame cfg;
        cfg.maxFractionPermille = static_cast<uint16_t>(sReforgeMgr->Config().ReforgeMaxFraction() * 1000.0);
        Send(player, Addon::EncodeConfig(cfg));
    }

    void SendItems(Player* player)
    {
        bool truncated = false;
        std::string const frame = Addon::EncodeItems(sReforgeMgr->EquippedReforges(player), truncated);
        Send(player, frame);
    }
}

namespace Reforge::AddonPush
{
    bool Enabled()
    {
        return sReforgeMgr->Config().Enabled() && sReforgeMgr->Config().AddonEnabled();
    }

    void SendLogin(Player* player)
    {
        if (!Enabled() || !player)
            return;

        Addon::HelloFrame hello;
        hello.enabled = sReforgeMgr->Config().Enabled();
        Send(player, Addon::EncodeHello(hello));

        SendConfig(player);
        SendCurrencies(player);
        SendItems(player);
    }

    void SendState(Player* player)
    {
        if (!Enabled() || !player)
            return;

        SendConfig(player);
        SendCurrencies(player);
        SendItems(player);
    }
}

class ReforgeAddonPlayerScript : public PlayerScript
{
public:
    ReforgeAddonPlayerScript() : PlayerScript("ReforgeAddonPlayerScript") { }

    void OnPlayerLogin(Player* player) override
    {
        AddonPush::SendLogin(player);
    }
};

void AddReforgeAddonScripts()
{
    new ReforgeAddonPlayerScript();
}
