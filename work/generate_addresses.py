import argparse, struct, re, json
from pathlib import Path

parser = argparse.ArgumentParser(description="Generate paired Kenshi 1.0.65 native addresses.")
parser.add_argument("--game", required=True, type=Path, help="Kenshi install directory")
parser.add_argument("--kenshilib", required=True, type=Path, help="KenshiLib SDK directory")
args = parser.parse_args()
work = Path(__file__).resolve().parent
root = work.parent
game = args.game
headers = args.kenshilib / "Include/kenshi"
gog=[v[0] for v in struct.iter_unpack('<I',(game/'RE_Kenshi/RVAs/GOG_1.0.65.br').read_bytes())]
steam=[v[0] for v in struct.iter_unpack('<I',(game/'RE_Kenshi/RVAs/Steam_1.0.65.br').read_bytes())]
requests={
 'HudStats':('CharStats','getGUIDataForMainInfo'),
 'FormatStat':('CharStats','formatWholeStatStringWithBonuses'),
 'HudLine':('DatapanelGUI','setLineStatInfo'),
 'ObjectName':('RootObjectBase','_NV_getName'),
 'DataPostProcess':('GameDataManager','postProcessingTheDatas'), 'GetStat':('CharStats','getStat'), 'GetStatRef':('CharStats','getStatRef'),
 'Accuracy':('CharStats','getRangedAccuracyMult'),'Reload':('CharStats','getReloadSkill'),
 'XP':('CharStats','xpGeneral'),'Load':('CharStats','updateStats'),'Save':('CharStats','serialise'),
 'Destroy':('CharStats','_DESTRUCTOR'),'Setup':('CharacterStatsWindow','setupStats'),
 'UpdateStats':('CharacterStatsWindow','updateStats'),
 'Line':('DatapanelGUI','setLine'), 'AnimData':('AnimationClass','_NV_getAnimationData'),
 'Idle':('AnimationClass','_NV_getIdleAnimation'),'Manual':('AnimationClass','runAnimation_manualTiming'),
 'Shoot':('GunClass','shoot'),'AnimUpdate':('AnimationClass','_NV_update'),
 'RunLayer':('AnimationClass','runAnimation'),'RunLayerExt':('AnimationClass','runAnimation'),
 'Details':('Character','updateGUIStatsDetails'),'RaceXPLine':('Character','_printRaceXPBonusLine'),
 'LinePlain':('DatapanelGUI','setLine'),'FriendlyFire':('CharStats','getRangedFriendlyFireAvoidanceChance'),
 'StopAnimation':('AnimationClass','stopAnimation'),
 'AppearanceTransforms':('Appearance','updateAnimationTransforms'),
 'AppearanceForceTransforms':('Appearance','forceUpdateAnimationTransforms'),
 'BonePosition':('Character','getBoneWorldPosition'),
 'EquipCreated':('Gear','_NV_createItemEntityCallback_Equipping'),
 'EquipDestroyed':('Gear','_NV_destroyItemEntityCallback_Equipping'),
 'AmmoVisibility':('GunClass','updatePartVisibilities'),
 'StartStumble':('AnimationClass','startStumble'),
 'RagdollCallback':('Character','_NV_postRagdollCallback'),
 'PlayAction':('AnimationClass','playAction'),
 'DiagnosticGuiUpdate':('ForgottenGUI','update'),
 'DiagnosticLabelUpdate':('ScreenLabel','_NV_update'),
 'DiagnosticLabelDestroy':('ScreenLabel','_DESTRUCTOR')}
items=[]
for label,(cls,method) in requests.items():
 path=next(p for p in headers.rglob('*.h') if p.stem==cls)
 lines=[l for l in path.read_text().splitlines() if re.search(r'\b'+method+r'\(',l) and 'RVA =' in l]
 if label=='FormatStat':lines=[l for l in lines if 'StatsEnumerated' in l]
 if label=='AnimData':lines=[l for l in lines if 'std::string' in l]
 if label=='DiagnosticLabelUpdate':lines=lines[:1]
 if label=='DiagnosticLabelDestroy':lines=lines[:1]
 if label=='StopAnimation':lines=[l for l in lines if 'std::string' in l]
 if label=='PlayAction':lines=[l for l in lines if 'AnimationData* anim' in l]
 if label=='Line':lines=[l for l in lines if 'keyValue' in l and 'skinA' in l]
 if label=='LinePlain':lines=[l for l in lines if 'const std::string& s1, const std::string& s2, int category, bool last, bool keyVisible' in l and 'keyValue' not in l]
 if label=='RunLayer':lines=[l for l in lines if 'AnimationLayerEnum lay' in l and 'const std::string& ext' not in l]
 if label=='RunLayerExt':lines=[l for l in lines if 'AnimationLayerEnum lay' in l and 'const std::string& ext' in l]
 assert len(lines)==1,(label,lines)
 g=int(re.search(r'RVA = (0x[\dA-Fa-f]+)',lines[0])[1],16)
 indices=[i for i,v in enumerate(gog) if v==g];assert indices
 targets={steam[i] for i in indices};assert len(targets)==1
 items.append((label,g,targets.pop()))
# Static vector: GOG header RVA; Steam verified in both setupStats/updateStats
# and BuildStats push_back references (0x8af019 -> 0x201ee38).
items.append(('SkillGroups',0x201ce48,0x201ee38))
text='// Generated from the installed paired KenshiLib 1.0.65 RVA tables.\n#pragma once\n#include <Windows.h>\n#include <kenshi/Kenshi.h>\nnamespace BowNative {\n'
text+='enum Entry { '+', '.join(i[0] for i in items)+' };\n'
text+='static const unsigned int gog[]={'+','.join(hex(i[1]) for i in items)+'};\n'
text+='static const unsigned int steam[]={'+','.join(hex(i[2]) for i in items)+'};\n'
text+='''inline intptr_t Address(Entry entry) {
    KenshiLib::BinaryVersion v=KenshiLib::GetKenshiVersion();
    if(v.GetVersion()!="1.0.65") return 0;
    unsigned int offset=0;
    if(v.GetPlatform()==KenshiLib::BinaryVersion::STEAM) offset=steam[entry];
    else if(v.GetPlatform()==KenshiLib::BinaryVersion::GOG) offset=gog[entry];
    if(!offset) return 0;
    unsigned char* base=reinterpret_cast<unsigned char*>(GetModuleHandleA(NULL));
    IMAGE_DOS_HEADER* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    IMAGE_NT_HEADERS64* nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    if(offset>=nt->OptionalHeader.SizeOfImage) return 0;
    return reinterpret_cast<intptr_t>(base+offset);
}
}
'''
(root/'NativeAddresses.h').write_text(text)
(work/'native_addresses.json').write_text(json.dumps(items,indent=2))
