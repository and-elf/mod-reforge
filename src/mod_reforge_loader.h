#ifndef MOD_REFORGE_SRC_LOADER_H
#define MOD_REFORGE_SRC_LOADER_H

// Per-feature script registration entrypoints, invoked by Addmod_reforgeScripts().
void AddReforgeSetupScripts();     // WorldScript: config snapshot + reforge cache load
void AddReforgeVehicleScripts();   // PlayerScript: the enchant-slot stat vehicle (§9)
void AddReforgeGossipScripts();    // Reforge NPC gossip (works without the client addon)
void AddReforgeCommandScripts();   // `.reforge …` — also the client addon's inbound API
void AddReforgeAddonScripts();     // WorldScript/PlayerScript: LANG_ADDON state pushes (§11)

#endif // MOD_REFORGE_SRC_LOADER_H
