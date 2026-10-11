# Wii U

Status: synced
sigil identifies Wii U WUA archives and collects and restores Cemu save folders, per account and shared by every account.

## Identification

| Slug | Platform | Inputs | `title_id` example | `usage` | Status |
|---|---|---|---|---|---|
| `wiiu` | Wii U | `.wua` | `10143500` (last 8 of folder name) | folder-split (`save_id` is `<high>/<low>`) | |

See [Identification](../identification.md) for the result fields. A `.wua` is a ZArchive; [Containers](../containers.md) covers how sigil reads it.

**Wii U splits its 16 hex digits.** WUA archives carry a top-level folder
named `00050000<8 hex>_v0`. The full 16 hex is the formal title ID, and
Cemu keeps the save at `mlc01/usr/save/<high>/<low>/`, both halves with
`{:08x}`. Sigil emits the low 8 as `title_id`, the full 16 as
`raw_serial`, and `save_id` as `<high>/<low>` in lowercase with `usage`
folder-split, the same shape as 3DS and Wii: `00050000/1010ec00` for
Mario Kart 8. A game is `00050000` and a demo `00050002`; an archive
bundling a game with its update (`0005000E`) or DLC (`0005000C`) names
the game's, and with a game and a demo, the game's. The filename fallback
reads the category from a bracketed 16-hex id, giving an update's or
DLC's to its game; a bare 8-hex code has no category and stays the code
alone, with `usage` folder-exact.

## Save layouts

| Layout | Files (role, option) | Shared | Verified |
|---|---|---|---|
| `cemu` | folders per profile: `mlc01/usr/save/{category}/{save_id}/user/{profile}/` account, `.../user/common/` and `.../meta/` device | | emulator source; on device (Cemu on Android) |

`{category}` and `{save_id}` are the two halves of a `save_id` of
`<high>/<low>`. A `save_id` of the code alone, from the filename fallback
or a result persisted before Wii U carried its category, takes
`00050000`. The layout lists `mlc01/usr/save/00050000`,
`mlc01/usr/save/00050002` and `mlc01/usr/save/system/act`, which
`sigil_save_layout_subdirs()` names.

Cemu keeps a game's saves per user profile, and also keeps saves every profile on the device shares. [Save units](../save-units.md#profiles) has the rules for picking a profile.

| Layout | Account | Device | Profile list |
|---|---|---|---|
| `cemu` | `mlc01/usr/save/{category}/{save_id}/user/{profile}/` | `user/common/` and `meta/` beside it | `mlc01/usr/save/system/act/{profile}/account.dat` (`PersistentId`, `MiiName`) |

A profile id is named as its save folder is, such as `80000001`. The root is the emulator's base folder, the one holding `mlc01/`.

A Cemu unit holds `<low>/meta/`, `<low>/user/account/` and
`<low>/user/common/`, the category left out so the request's own places
it; an
older unit naming the account folder by its id restores as the account
save.

The `meta/meta.xml` in each save folder declares `common_save_size` and
`account_save_size`, and they predict where the save sits: a game that
declares a common size and no account size keeps its whole save in
`user/common/`, and the reverse keeps it under the account. Checked
against live Cemu saves.

## Sync

On a layout
with profiles the unit is a zip of the game's save folders under the names
above, and `companions` is `SIGIL_ERR_INVALID_ARG`.

On restore, Cemu's `meta/`, which
Cemu writes again by itself, is skipped instead of failing with `SIGIL_ERR_NO_TARGET` when no folder of the game takes it.
[Sync](../sync.md) has the rest of collect and restore.

## Open items

1. Read `meta.xml` from a Wii U WUA in the corpus and confirm the `common_save_size` and `account_save_size` fields.
