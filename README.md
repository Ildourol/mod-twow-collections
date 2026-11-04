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

These steps assume you have a working [TortoiseWoW](https://github.com/Penqle/tortoise-wow) source checkout, initialized core databases, and the server data files for your realm. Use a core revision compatible with this module. Run all build and SQL commands from the core source root, the directory containing the core's `CMakeLists.txt`.


### 1. Add the module and build on Linux

Use Git, CMake, a C++17 compiler, and the core's build dependencies. On Debian or Ubuntu, a typical starting point is:

```bash
sudo apt update
sudo apt install git cmake build-essential libace-dev default-libmysqlclient-dev libssl-dev zlib1g-dev
```

Use your distribution's `mysql` or `mariadb` command-line client if importing SQL manually. Keep any additional dependency and CMake settings required by your core and other enabled modules.

```bash
cd /path/to/tortoise-wow
git clone https://github.com/Ildourol/mod-twow-collections.git modules/mod-twow-collections
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PWD/server-stage" -DMODULE_MOD_TWOW_COLLECTIONS=static
cmake --build build --parallel
cmake --install build
```

### 2. Add the module and build on Windows

Install Git for Windows, CMake 3.21 or later, and Visual Studio 2022 or its Build Tools with **Desktop development with C++** and a Windows SDK. Open PowerShell with Git and CMake on `PATH`. Use an x64 build and the dependency setup of your working core.

```powershell
Set-Location 'C:\src\tortoise-wow'
git clone https://github.com/Ildourol/mod-twow-collections.git modules/mod-twow-collections
cmake -S . -B build-vs -G "Visual Studio 17 2022" -A x64 "-DCMAKE_INSTALL_PREFIX=$PWD/server-stage" -DMODULE_MOD_TWOW_COLLECTIONS=static
cmake --build build-vs --config Release --parallel
cmake --install build-vs --config Release
```

Check each command for errors before continuing. Both builds include `mangosd`. If the configure output reports an unused module switch, verify the source placement before building. The two build sections are platform alternatives. If you already have a module checkout, use it in place of the clone. A full directory copy, a Linux symlink, or a Windows directory junction works as long as `modules/mod-twow-collections/src/` is directly below the module directory. The module directory name determines the CMake switch and loader symbol. If reusing a build directory, keep its generator and architecture and preserve the switches for your other modules. Reconfigure after adding the module and verify CMake lists it as static.

The staged world server is normally `server-stage/bin/mangosd` on Linux and `server-stage/mangosd.exe` on Windows. Stop the running world server before deploying the rebuilt executable and any required runtime libraries or module assets. Preserve the live configuration and data files. The paths below assume the standard staged layout. For an existing deployment, use the directory containing the active `mangosd.conf`, including a custom path passed via `-c`.

### 3. Set up the database

Back up the affected databases. The examples use `tw_char` and `tw_world` as database names. Substitute the names from the active `CharacterDatabase.Info` and `WorldDatabase.Info` settings as applicable, plus your database user and any connection options.

Choose 1 import method. For the core auto-updater, verify that `Database.AutoUpdate.Enabled` and `Database.AutoUpdate.SortByName` are enabled, that `Database.AutoUpdate.AllowedModules` includes `mod-twow-collections` (or uses `all`), and that the runtime host can access the module checkout at the source path embedded by the build. The updater uses the configured `CharUpdateName` and `WorldUpdateName` under `data/sql/` (normally `character` and `world`). Check the first startup log for applied updates. If importing manually, exclude this module from the updater allowlist to avoid replaying the same files. Preserve other modules that use automatic updates.

**Manual import on Linux (Bash)**

```bash
mysql -u root -p tw_char < "modules/mod-twow-collections/data/sql/character/001_account_collection.sql"
mysql -u root -p tw_char < "modules/mod-twow-collections/data/sql/character/002_character_critter_pet.sql"
mysql -u root -p tw_world < "modules/mod-twow-collections/data/sql/world/001_base_collections.sql"
mysql -u root -p tw_world < "modules/mod-twow-collections/data/sql/world/002_mount_gaps_80456.sql"
mysql -u root -p tw_world < "modules/mod-twow-collections/data/sql/world/003_companion_critters.sql"
```

**Manual import on Windows (PowerShell)**

Run `Get-Command mysql.exe` first. If it is unavailable, install the MySQL or MariaDB command-line client and add its `bin` directory to the current PowerShell session. Replace the example path below with your installation path. Use `mariadb.exe` in the import commands if that is the client you installed. Run imports in the order shown and stop if any command reports an error. `cmd.exe` handles input redirection so the SQL file reaches the client directly.

```powershell
# Only needed when mysql.exe is installed but missing from PATH
# $env:Path = 'C:\Program Files\MySQL\MySQL Server 8.0\bin;' + $env:Path
cmd.exe /d /c 'mysql.exe -u root -p tw_char < modules\mod-twow-collections\data\sql\character\001_account_collection.sql'
if ($LASTEXITCODE -ne 0) { throw "Module SQL import failed" }
cmd.exe /d /c 'mysql.exe -u root -p tw_char < modules\mod-twow-collections\data\sql\character\002_character_critter_pet.sql'
if ($LASTEXITCODE -ne 0) { throw "Module SQL import failed" }
cmd.exe /d /c 'mysql.exe -u root -p tw_world < modules\mod-twow-collections\data\sql\world\001_base_collections.sql'
if ($LASTEXITCODE -ne 0) { throw "Module SQL import failed" }
cmd.exe /d /c 'mysql.exe -u root -p tw_world < modules\mod-twow-collections\data\sql\world\002_mount_gaps_80456.sql'
if ($LASTEXITCODE -ne 0) { throw "Module SQL import failed" }
cmd.exe /d /c 'mysql.exe -u root -p tw_world < modules\mod-twow-collections\data\sql\world\003_companion_critters.sql'
if ($LASTEXITCODE -ne 0) { throw "Module SQL import failed" }
```

### 4. Deploy and enable the configuration

Create `mod-twow-collections.conf` inside the `modules/` subdirectory beside the active `mangosd.conf`. For a first install using the example staging directory, the following commands copy the shipped template. If an active module config already exists, retain it and merge new settings from the template.

**Linux**

```bash
mkdir -p server-stage/etc/modules
cp -n "modules/mod-twow-collections/conf/mod-twow-collections.conf.dist" "server-stage/etc/modules/mod-twow-collections.conf"
```

**Windows (PowerShell)**

```powershell
New-Item -ItemType Directory -Force 'server-stage/modules' | Out-Null
if (-not (Test-Path 'server-stage/modules/mod-twow-collections.conf')) {
    Copy-Item 'modules/mod-twow-collections/conf/mod-twow-collections.conf.dist' 'server-stage/modules/mod-twow-collections.conf'
}
```

Keep the template's section header and confirm `Collections.Enable = 1` in the active file. Start the rebuilt server with your normal launcher and active `mangosd.conf`. Check the startup log for module configuration, database, and dependency errors.

### 5. Install the client addon

Exit the Turtle WoW client and copy the full `addon/Turtle_Collections` folder to the client's `Interface/AddOns/` directory. The `Turtle_Collections.toc` file must be directly inside `Interface/AddOns/Turtle_Collections/`. For example, from the core source root:

```bash
mkdir -p "/path/to/TurtleWoW/Interface/AddOns"
cp -R "modules/mod-twow-collections/addon/Turtle_Collections" "/path/to/TurtleWoW/Interface/AddOns/"
```

```powershell
$AddOns = 'C:\Games\TurtleWoW\Interface\AddOns'
New-Item -ItemType Directory -Force $AddOns | Out-Null
Copy-Item -Recurse -Force 'modules/mod-twow-collections/addon/Turtle_Collections' $AddOns
```

Restart the client and enable the addon from the character selection screen.

### Verify the installation

Run `.collection info` on a test character and open Turtle Collections. Check the startup log for missing world catalogues or character tables if the UI is empty.

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
