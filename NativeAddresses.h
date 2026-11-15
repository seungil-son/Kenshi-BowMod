// Generated from the installed paired KenshiLib 1.0.65 RVA tables.
#pragma once
#include <Windows.h>
#include <kenshi/Kenshi.h>
namespace BowNative {
enum Entry { GetStat, GetStatRef, Accuracy, Reload, XP, Load, Save, Destroy, Setup, UpdateStats, Line, AnimData, Idle, Manual, Shoot, AnimUpdate, RunLayer, RunLayerExt, Details, RaceXPLine, LinePlain, FriendlyFire, StopAnimation, AppearanceTransforms, AppearanceForceTransforms, BonePosition, EquipCreated, EquipDestroyed, AmmoVisibility, StartStumble, RagdollCallback, PlayAction, DiagnosticGuiUpdate, DiagnosticLabelUpdate, DiagnosticLabelDestroy, SkillGroups, DataPostProcess, ObjectName, HudStats, FormatStat, HudLine };
static const unsigned int gog[]={0x883310,0x8829e0,0x4349e0,0x884530,0x8c4c80,0x64af20,0x64a170,0x6404f0,0x8b8740,0x8a9260,0x6fd7c0,0x5c4ee0,0x51eab0,0x5b7550,0x439fb0,0x5b6140,0x5b7340,0x5b7780,0x8afaa0,0x8a97d0,0x6fd780,0x43fb80,0x5b36c0,0x52bba0,0x52bc20,0x43fbe0,0x43c090,0x434b70,0x435ce0,0x51fd10,0x5cb810,0x51e330,0x6e91e0,0x6e9b60,0x6e1b50,0x201ce48,0x6c2ab0,0xd3ca0,0x890970,0x8a8810,0x6fe650};
static const unsigned int steam[]={0x883bf0,0x8832c0,0x434dc0,0x884e10,0x8c5560,0x64ac10,0x649e60,0x6401e0,0x8b9020,0x8a9b40,0x6fde60,0x5c4bd0,0x51e7a0,0x5b7240,0x43a390,0x5b5e30,0x5b7030,0x5b7470,0x8b0380,0x8aa0b0,0x6fde20,0x43ff60,0x5b33b0,0x52b890,0x52b910,0x43ffc0,0x43c470,0x434f50,0x4360c0,0x51fa00,0x5cb500,0x51e020,0x6e98d0,0x6ea250,0x6e2240,0x201ee38,0x6c31a0,0xd3ca0,0x891250,0x8a90f0,0x6fecf0};
inline intptr_t Address(Entry entry) {
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
