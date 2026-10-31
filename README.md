# mod-twow-collections: Account-Wide Vanity Toybox, Mounts & Pets

A standalone, non-intrusive custom module for **Turtle WoW custom vMaNGOS** (`tortoise-wow` targeting client 1.18.1.7272 / 1.12.1).

---

## Features
- **Account-Wide Progression**: Mounts, companion pets, and novelty toys learned by any character on an account are automatically saved to `account_collection` and shared across all characters on that account.
- **Inventory Bag Space Saver**: Eliminates the need to carry dozens of mount whistles, pet carriers, and toy tokens in bags or bank slots.
- **Dual-Tier Riding Skill Verification**: Enforces Apprentice Riding (rank 75) for 60% mounts and Journeyman Riding (rank 150) for 100% epic mounts. Level 60 characters cannot summon or learn epic mounts without training rank 150 riding from a trainer.
- **Class Mount Isolation**: Checks DBC `classmask` to ensure Paladin Warhorse/Charger and Warlock Felsteed/Dreadsteed remain exclusive to their respective classes.
- **Faction & Race Protection**: Gated by faction alignment so Alliance characters cannot automatically learn Horde racial mounts (Wolves, Kodos, Undead Horses) and vice versa (configurable).
- **Dynamic Riding Trainer Detection**: Automatically detects when a player trains Riding Skill from any trainer spell via `SPELL_EFFECT_SKILL` & `SKILL_RIDING` and instantly syncs eligible mounts.
- **Live Runtime Reload**: Built with `WorldScript` listening to `WORLDHOOK_ON_AFTER_CONFIG_LOAD` so `.reload config` updates settings and exclusions instantly without restarting `mangosd`.
- **Low-Level Mount Visibility**: Low-level characters can view all unlocked account mounts in the collection browser by default, with summoning strictly locked until level and riding prerequisites are met.
- **Retroactive Inventory Scanner & Cleanse**: Veterans can use `.collection sync` to automatically scan existing spellbooks, equipped items, inventory bags, and bank vaults. Use `.collection cleanse` to unlearn any invalid cross-class or cross-faction mounts.
- **Native Lua 5.0 Client AddOn**: Includes `Turtle_Collections`, a 420×420 compact dark theme UI with search filtering, category tabs, draggable minimap button, and one-click summoning.
- **Companion Critter Petting**: Tracks critter petting interactions (`character_critter_pet`) and ensures summoned vanity creatures are classified as Critters (`type = 8`).
- **The Unediting Doctrine**: 100% encapsulated inside standalone module space. Zero changes to `tortoise-wow` core headers or `TortoiseBots`.

---

## Installation

### 1. Server Core Integration & Build
1. Place or clone `mod-twow-collections` into your `tortoise-wow/modules/` directory:
   ```bash
   cd /path/to/tortoise-wow/modules
   git clone https://github.com/Ildourol/mod-twow-collections.git
   ```
2. Re-run CMake generation so the module is automatically discovered via `mod-twow-collections.cmake`:
   ```bash
   cmake -B build -S .
   ```
3. Rebuild your `mangosd` server binary:
   ```bash
   cmake --build build --target mangosd --config Release
   ```

### 2. Database Migrations
Apply the SQL schemas and catalogs to your MySQL / MariaDB databases in the following order:

* **Character Database** (`tw_char` / `characters`):
  ```bash
  mysql -u root -p tw_char < data/sql/character/001_account_collection.sql
  mysql -u root -p tw_char < data/sql/character/002_character_critter_pet.sql
  ```
* **World Database** (`tw_world` / `world`):
  ```bash
  mysql -u root -p tw_world < data/sql/world/001_base_collections.sql
  mysql -u root -p tw_world < data/sql/world/002_mount_gaps_80456.sql
  mysql -u root -p tw_world < data/sql/world/003_companion_critters.sql
  ```

### 3. Server Configuration
1. Copy the distributed template configuration file to your server runtime configuration folder:
   ```bash
   cp conf/mod-twow-collections.conf.dist /path/to/server/etc/mod-twow-collections.conf
   ```
2. Review and adjust configuration parameters in `mod-twow-collections.conf` if desired (all defaults are pre-calibrated for immediate gameplay):
   - `Collections.Enable = 1`
   - `Collections.Mounts.LimitClass = 1`
   - `Collections.Mounts.LimitRace = 1`
   - `Collections.Mounts.ShowToLowLevel = 1`
   - `Collections.Mounts.AutoLearnOnLogin = 1`

### 4. Client AddOn Installation
1. Copy the `addon/Turtle_Collections` folder directly into your Turtle WoW game client's AddOn directory:
   ```text
   World of Warcraft/Interface/AddOns/Turtle_Collections
   ```
2. Verify the directory structure:
   ```text
   Interface/AddOns/Turtle_Collections/
   ├── Turtle_Collections.toc
   ├── Turtle_Collections_Data.lua
   ├── Turtle_Collections.xml
   └── Turtle_Collections.lua
   ```
3. Launch the game client, ensure **Turtle Collections** is checked in the **AddOns** menu at the Character Select screen, and log into your character.

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
| `.collection cleanse` | Removes invalid class or cross-faction mounts from character spellbook. |

---

## Client AddOn Usage
The module comes bundled with `addon/Turtle_Collections`:
- Open in-game via the circular minimap launcher button (draggable around the minimap rim) or with slash commands:
  - `/collection` or `/collections`
  - `/mounts`
  - `/pets`
  - `/toybox`
- **Browsing**: Filter collection items by category (Mounts, Companion Pets, Vanity Toys) or by name with the live search box.
- **Gating Indications**: Unlocked mounts display status cleanly (`Req Lvl X`, `Req Riding 75`, `Req Riding 150`, or `Class Locked`).
- **Summoning**: Click **Use** on any eligible item to summon or activate it directly.

---

## Configuration (`mod-twow-collections.conf`)

| Setting | Default | Description |
|:---|:---:|:---|
| `Collections.Enable` | `1` | Master toggle enabling or disabling the Collections module. |
| `Collections.AccountWide.Mounts` | `1` | Shares learned mounts across all characters on the account. |
| `Collections.AccountWide.Pets` | `1` | Shares companion pets across all characters on the account. |
| `Collections.AccountWide.Toys` | `1` | Shares novelty toys across all characters on the account. |
| `Collections.Mounts.AutoLearnOnLogin` | `1` | Automatically teaches unlocked mounts on login if level and riding skill are met. |
| `Collections.Mounts.ShowToLowLevel` | `1` | Enables low-level characters to view unlocked account mounts in the client AddOn collection browser (`/collection`). |
| `Collections.Mounts.ShowInSpellbookAtLowLevel` | `1` | Enables low-level characters to view unlocked account mounts in their native game spellbook (`P` key). |
| `Collections.Mounts.EnforceLevelRequirements` | `1` | Enforces character level checks (e.g. 40/60) before auto-learning or summoning mounts. |
| `Collections.Mounts.EnforceRidingSkill` | `1` | Enforces riding skill rank requirement (75 vs 150) before auto-learning or summoning mounts. |
| `Collections.Mounts.LimitClass` | `1` | Enforces class restrictions (Paladin/Warlock mounts only for their class). |
| `Collections.Mounts.LimitRace` | `1` | Enforces faction alignment (Alliance characters do not receive Horde racial mounts). |
| `Collections.Mounts.AllowAllMounts` | `0` | Bypasses all riding and level gating checks. |
| `Collections.Mounts.ExcludedSpellIDs` | `0` | Comma-separated spell IDs to exclude from account mount sharing. |
| `Collections.Mounts.EpicSpellIDs` | `0` | Override list of spell IDs forced to require Journeyman Riding (150). |
| `Collections.Mounts.NormalSpellIDs` | `0` | Override list of spell IDs forced to require Apprentice Riding (75). |
| `Collections.ConsumeItemOnLearn` | `1` | Consumes the physical inventory item when a collection item is first learned. |
| `Collections.AnnounceOnLearn` | `1` | Broadcasts an affirmative chat notification when a new collection item is unlocked. |

---

## Database Schemas & Baseline Catalogs
- `data/sql/character/001_account_collection.sql`: Account-wide collection table (`tw_char.account_collection`).
- `data/sql/character/002_character_critter_pet.sql`: Character critter petting table (`tw_char.character_critter_pet`).
- `data/sql/world/001_base_collections.sql`: Baseline catalogs for 365 mounts (`collection_mount`), 188 pets (`collection_pet`), and 72 toys (`collection_toy`).
- `data/sql/world/002_mount_gaps_80456.sql`: Fix for Swift Blood Kodo (80456 -> 45044).
- `data/sql/world/003_companion_critters.sql`: Companion pets creature template classification.
