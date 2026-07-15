#include "ReforgeMgr.h"
#include "mod_reforge_loader.h"

#include "ScriptMgr.h"

using namespace Reforge;

// Lifecycle wiring: snapshot config on load/reload, and fill the reforge cache once the world DB is up.
class ReforgeSetupWorldScript : public WorldScript
{
public:
    ReforgeSetupWorldScript() : WorldScript("ReforgeSetupWorldScript") { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        sReforgeMgr->LoadConfig();
    }

    void OnStartup() override
    {
        sReforgeMgr->LoadFromDB();
    }
};

void AddReforgeSetupScripts()
{
    new ReforgeSetupWorldScript();
}
