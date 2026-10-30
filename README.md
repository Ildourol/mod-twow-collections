# mod-twow-collections: Account-Wide Vanity Toybox, Mounts & Pets

A standalone, non-intrusive custom module for **Turtle WoW custom vMaNGOS** (`tortoise-wow` targeting client 1.18.1.7272 / 1.12.1).

---

## Features
- **Account-Wide Progression**: Mounts, companion pets, and novelty toys learned by any character on an account are automatically saved to `account_collection` and shared across all characters on that account.
- **Inventory Bag Space Saver**: Eliminates the need to carry dozens of mount whistles, pet carriers, and toy tokens in bags or bank slots.
- **Automatic Login Synchronization**: When an alt character logs in, eligible companion pets and novelty toys are automatically taught. Mounts are automatically taught when the character satisfies level (e.g. 40 / 60) and riding requirements.
- **Retroactive Inventory Scanner**: Veterans can use `.collection sync` to automatically scan existing spellbooks, equipped items, inventory bags, and bank vaults, backfilling all collected items into the account collection.
- **Native Lua 5.0 Client AddOn**: Includes `Turtle_Collections`, a 420×420 compact dark theme UI with search filtering, category tabs, draggable minimap button, and one-click summoning.
- **Companion Critter Petting**: Tracks critter petting interactions (`character_critter_pet`) and ensures summoned vanity creatures are classified as Critters (`type = 8`).
- **Core Immutability**: 100% encapsulated inside standalone module space. Zero changes to `tortoise-wow` core headers or `TortoiseBots`.

---

## In-Game Commands
| Command | Description |
|:---|:---|
| `.collection info` | Displays total mounts, pets, and toys unlocked across the account. |
| `.collection list mounts [page]` | Lists account-unlocked mounts and whether learned on this character. |
| `.collection list pets [page]` | Lists account-unlocked companion pets. |
| `.collection list toys [page]` | Lists account-unlocked novelty toys. |
| `.collection summon <spellId>` | Summons an unlocked mount or companion pet. |
| `.collection sync` | Retroactively scans bags, bank, and spells, registering all to account collection. |

---

## Client AddOn
The module comes bundled with `addon/Turtle_Collections`.
- Copy `addon/Turtle_Collections` to your client's `Interface/AddOns/` directory.
- Open in-game with `/collection`, `/collections`, `/toybox`, `/mounts`, or `/pets`.
- Supports one-click summoning and real-time synchronization with the server.

---

## Configuration (`conf/mod-twow-collections.conf.dist`)
Copy `conf/mod-twow-collections.conf.dist` to `conf/mod-twow-collections.conf` in your server directory:
- `Collections.Enable`: Enable or disable the module (Default: `1`).
- `Collections.AccountWide.Mounts`: Share mounts across account (Default: `1`).
- `Collections.AccountWide.Pets`: Share companion pets across account (Default: `1`).
- `Collections.AccountWide.Toys`: Share novelty toys across account (Default: `1`).
- `Collections.Mounts.AutoLearnOnLogin`: Auto-learn mounts on login once level and riding skill are met (Default: `1`).
- `Collections.Mounts.ShowToLowLevel`: Allow low-level characters to view all unlocked account mounts in collection browser (Default: `1`).
- `Collections.Mounts.EnforceLevelRequirements`: Enforce level 40/60 checks before auto-learning or summoning (Default: `1`).
- `Collections.Mounts.EnforceRidingSkill`: Enforce riding skill requirement before auto-learning or summoning (Default: `1`).
- `Collections.ConsumeItemOnLearn`: Destroy item upon learning to save space (Default: `1`).
- `Collections.AnnounceOnLearn`: Affirmative chat notifications (Default: `1`).

---

## Database Migrations
- `data/sql/character/001_account_collection.sql`: Account-wide collection table (`tw_char.account_collection`).
- `data/sql/character/002_character_critter_pet.sql`: Character critter petting table (`tw_char.character_critter_pet`).
- `data/sql/world/001_base_collections.sql`: Baseline `collection_mount`, `collection_pet`, `collection_toy`.
- `data/sql/world/002_mount_gaps_80456.sql`: Fix for Swift Blood Kodo (80456 -> 45044).
- `data/sql/world/003_companion_critters.sql`: Companion pets creature template classification.
