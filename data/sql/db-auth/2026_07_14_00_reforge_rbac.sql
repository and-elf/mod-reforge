-- mod-reforge: RBAC permission for the `.reforge` command (id 100000), linked into every security
-- level's role (192 admin, 193 gm, 194 head-gm/…, 195 player) so ordinary players -- and their client
-- addon, which drives the same command over the AzerothCore addon channel -- may reforge, while GMs
-- keep access too. Referenced by REFORGE_RBAC_PERM in ReforgeCommandScripts.cpp.

DELETE FROM `rbac_permissions` WHERE `id` = 100000;
INSERT INTO `rbac_permissions` (`id`, `name`) VALUES
    (100000, 'Command: reforge');

DELETE FROM `rbac_linked_permissions` WHERE `linkedId` = 100000;
INSERT INTO `rbac_linked_permissions` (`id`, `linkedId`) VALUES
    (192, 100000),
    (193, 100000),
    (194, 100000),
    (195, 100000);
