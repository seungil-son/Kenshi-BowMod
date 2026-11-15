# BowMod for Kenshi

BowMod adds six bows, two arrow sizes, bow crafting, and an independent Archery skill. This source snapshot matches revision 38, including the left-hand grip and running bowstring fixes. The native plugin targets 64-bit Kenshi 1.0.65 on Steam and GOG and requires [RE_Kenshi](https://github.com/BFrizzleFoShizzle/RE_Kenshi).

## Build and test locally

1. Install the MSVC 2010 x64 toolchain and Windows SDK 7.1. Obtain Boost 1.60.0 and the [KenshiLib SDK used by this build](https://github.com/KenshiReclaimer/KenshiLib/tree/1ce4a163739bbdf17254259039d7aa74fcc11cc0). The SDK folder must contain Include and Libraries, with KenshiLib.lib and OgreMain_x64.lib in Libraries.
2. Build from PowerShell. Use -MsvcRoot and -WindowsSdk if your toolchain is installed in different folders.

   ~~~powershell
   .\build.ps1 -KenshiLib 'C:\SDKs\KenshiLib' -Boost 'C:\SDKs\boost_1_60_0'
   ~~~

3. Stage a local test copy using a complete revision 38 BowMod installation or an extracted copy of that release. If using a Workshop copy, make sure it contains the updated male and female skeletons before staging. The script copies the required art assets and combines them with this repository's FCS data and your rebuilt DLL. The result is dist/BowMod.

   ~~~powershell
   .\stage.ps1 -BaseMod 'C:\path\to\existing\BowMod'
   ~~~

4. With Kenshi closed, put the staged BowMod folder under Kenshi/mods and enable BowMod.mod. Back up any existing copy before replacing it. Test with only one copy of BowMod enabled.

## What to edit

| Change | File or tool |
| --- | --- |
| Animation selection, arrow placement, and bowstring visibility | BowRuntime.cpp, ArrowAttachment.h, RunningAttachment.h |
| Bow skill, experience, and HUD | BowSkills.cpp, BowProgression.h |
| Item stats, vendors, and research | Open the staged BowMod.mod in the Forgotten Construction Set. Copy the edited .mod back to package/BowMod. |
| Localization | Edit work/bow_translations.py, run python work/generate_localization.py, then rebuild. For changes to the base English item names or descriptions, also update those records in FCS. |
| Animation clips | Import the male and female skeletons from character/ in the staged mod into Blender 5.0 using Lucius Kenshi IO Tool 1.3.2. Keep the clip names when exporting. Check left-hand grip at intermediate frames as well as keyframes. |

The checked-in NativeAddresses.h is already generated. If you need to regenerate it, run python work/generate_addresses.py --game <Kenshi directory> --kenshilib <SDK directory> with matching RE_Kenshi RVA tables for Steam and GOG. Check the generated diff before building.

For a race that uses a vanilla skeleton, add the existing BowMod animations entry to the race's animation files list in an FCS compatibility patch. For a custom rig, adapt the clips to that skeleton and link a new ANIMATION_FILE entry for the adapted files. Load the patch after both mods. [Lo-Fi Games animation guide](https://www.lofigames.com/phpBB3/viewtopic.php?f=11&t=13868)

## Credits and licensing

The BowMod plugin source is GPL-3.0-or-later. See LICENSE. The source license does not grant rights to third-party game or art assets. Use the art from your installed copy for local testing. Check the original terms before distributing modified assets.

- Attack animation reference: [Mixamo](https://www.mixamo.com/). The aiming run was made for BowMod.
- Ooyumi was used as a reference during development of aiming, firing and bowstring behavior.
- Bow model source: [10 Bows and Cross Bows on Sketchfab](https://sketchfab.com/3d-models/10-bows-and-cross-bows-1774a072f1cd45bf9c0875c1a23919a7).
