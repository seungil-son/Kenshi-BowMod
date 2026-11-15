// SPDX-License-Identifier: GPL-3.0-or-later
// Development build: requires in-game integration validation before release.
#include <Windows.h>
#include <float.h>
#include <map>
#include <algorithm>
#include <sstream>
#include "BowText.h"
#include "BowProgression.h"
#include "BowLocalization.h"
#include <Debug.h>
#include <core/Functions.h>
#include "NativeAddresses.h"
#include <kenshi/CharStats.h>
#include <kenshi/Character.h>
#include <kenshi/Gear.h>
#include <kenshi/GameData.h>
#include <kenshi/MedicalSystem.h>
#include <kenshi/gui/CharacterStatsWindow.h>
#include <kenshi/gui/DatapanelGUI.h>
bool InstallBowRuntime();
void EnableBowRuntime(bool on);

namespace {
const StatsEnumerated BOW_STAT=static_cast<StatsEnumerated>(10001);
const char* SAVE_KEY="bowmod skill";
const char* ROW_ID="bowmod.archery";

bool enabled=false;
CRITICAL_SECTION valuesLock;
std::map<const CharStats*,float> values;
__declspec(thread) const CharStats* combatContext=NULL;
__declspec(thread) const CharStats* xpContext=NULL;

float& Value(const CharStats* s) {
    EnterCriticalSection(&valuesLock);
    std::map<const CharStats*,float>::iterator i=values.find(s);
    if(i==values.end()) i=values.insert(std::make_pair(s,1.0f)).first;
    float* result=&i->second;
    LeaveCriticalSection(&valuesLock);
    return *result;
}
bool BowEquipped(const CharStats* s) {
    if(!s || !s->me) return false;
    Crossbow* weapon=s->me->getRangedWeapon();
    if(!weapon) return false;
    GameData* data=weapon->getGameData();
    if(!data) return false;
    // Explicit FCS flag shared with the editor's filtered bow category.
    auto marker=data->bdata.find("bowmod bow");
    return marker!=data->bdata.end() && marker->second;
}
typedef float (*GetFn)(const CharStats*,StatsEnumerated,bool);
typedef float& (*RefFn)(CharStats*,StatsEnumerated);
typedef float (*RangedFn)(CharStats*,StatsEnumerated);
typedef void (*XPFn)(CharStats*,float,float,StatsEnumerated);
typedef void (*DataFn)(CharStats*,GameData*);
typedef void (*DtorFn)(CharStats*);
typedef void (*WindowFn)(CharacterStatsWindow*);
GetFn getOriginal=NULL;RefFn refOriginal=NULL;
RangedFn accuracyOriginal=NULL,reloadOriginal=NULL;
XPFn xpOriginal=NULL;DataFn saveOriginal=NULL,loadOriginal=NULL;
DtorFn destroyOriginal=NULL;WindowFn setupOriginal=NULL,updateOriginal=NULL;

float Get(const CharStats* s,StatsEnumerated stat,bool raw) {
    if(enabled && (stat==BOW_STAT || (stat==STAT_CROSSBOWS && combatContext==s))) {
        float result=Value(s);
        if(stat==STAT_CROSSBOWS && combatContext==s) result=BowProgression::EffectiveSkill(result);
        if(!raw && s->medical) result*=s->medical->getHealthStatModifier(STAT_CROSSBOWS,true,true,true,true,true,true);
        return result;
    }
    return getOriginal(s,stat,raw);
}
float& Ref(CharStats* s,StatsEnumerated stat) {
    if(enabled && (stat==BOW_STAT || (stat==STAT_CROSSBOWS && xpContext==s))) return Value(s);
    return refOriginal(s,stat);
}
float Accuracy(CharStats* s,StatsEnumerated stat) {
    const CharStats* previous=combatContext;
    if(enabled && stat==STAT_CROSSBOWS && BowEquipped(s)) combatContext=s;
    float result=accuracyOriginal(s,stat);combatContext=previous;return result;
}
float Reload(CharStats* s,StatsEnumerated stat) {
    const CharStats* previous=combatContext;
    if(enabled && stat==STAT_CROSSBOWS && BowEquipped(s)) combatContext=s;
    float result=reloadOriginal(s,stat);combatContext=previous;return result;
}
void XP(CharStats* s,float time,float mult,StatsEnumerated stat) {
    const CharStats* previous=xpContext;
    if(enabled && stat==STAT_CROSSBOWS && BowEquipped(s)) xpContext=s;
    // Native XP curve and race/global XP multipliers; redirect only the result float.
    xpOriginal(s,time,mult,stat);xpContext=previous;
}
void Load(CharStats* s,GameData* d) {
    loadOriginal(s,d);
    if(!enabled) return;
    float v=1.0f;
    if(d) { auto i=d->fdata.find(SAVE_KEY);if(i!=d->fdata.end()) v=i->second; }
    if(!_finite(v) || v<0.0f || v>100.0f) v=1.0f;
    Value(s)=v;
}
void Save(CharStats* s,GameData* d) {
    saveOriginal(s,d);
    if(enabled && d) d->add(SAVE_KEY,Value(s),"BowMod",true,false);
}
void Destroy(CharStats* s) {
    EnterCriticalSection(&valuesLock);values.erase(s);LeaveCriticalSection(&valuesLock);
    destroyOriginal(s);
}
void RegisterStat() {
    RefreshBowText();
    // Native getStat uses map::operator[]: a missing ID returns an empty entry,
    // not NULL. BuildStats is not a per-frame refresh function.
    CharacterStatsWindow::Stat* crossbow=CharacterStatsWindow::getStat(STAT_CROSSBOWS);
    if(!crossbow || crossbow->id.empty()) return;
    CharacterStatsWindow::Stat* existing=CharacterStatsWindow::getStat(BOW_STAT);
    if(existing && !existing->id.empty() && existing->id!=ROW_ID) {
        enabled=false;
        EnableBowRuntime(false);
        ErrorLog("[BowMod] Stat ID conflict; BowMod disabled to preserve the other plugin.");
        return;
    }
    if(!existing || existing->id.empty())
        CharacterStatsWindow::addStat(BOW_STAT,ROW_ID,BOW_LABEL,true,
            BOW_DESCRIPTION);
    CharacterStatsWindow::Stat* bow=CharacterStatsWindow::getStat(BOW_STAT);
    if(bow){bow->name=BOW_LABEL;bow->description=BOW_DESCRIPTION;}
    // Register the row in the same native list used by setup, refresh and hover.
    // The old setLine hook targeted an overload that this window does not use.
    typedef decltype(CharacterStatsWindow::GeneralSkills) SkillGroups;
    C_ASSERT(sizeof(SkillGroups::value_type)==80);
    intptr_t groupAddress=BowNative::Address(BowNative::SkillGroups);
    if(!groupAddress || !bow) return;
    SkillGroups& groups=*reinterpret_cast<SkillGroups*>(groupAddress);
    for(auto g=groups.begin();g!=groups.end();++g) {
        auto& rows=g->second;
        for(auto i=rows.begin();i!=rows.end();++i) {
            if((*i)->type==STAT_CROSSBOWS) {
                if(std::find(rows.begin(),rows.end(),bow)==rows.end()) rows.insert(i+1,bow);
                return;
            }
        }
    }
}
void Setup(CharacterStatsWindow* w) {
    if(enabled) RegisterStat();
    // C-window follows the currently selected character, including bow skill.
    w->autoUpdateMode=true;
    setupOriginal(w);
}
void Update(CharacterStatsWindow* w) {
    if(enabled) RegisterStat();
    updateOriginal(w);
}

// Restrict the HUD substitution to the character currently being rendered.
__declspec(thread) const CharStats* hudContext=NULL;
__declspec(thread) bool bowHudLine=false;
typedef void (*HudFn)(CharStats*,DatapanelGUI*,int,bool);
typedef std::string* (*FormatFn)(const CharStats*,std::string*,StatsEnumerated);
typedef DataPanelLine* (*HudLineFn)(DatapanelGUI*,const std::string&,const std::string&,int);
HudFn hudOriginal=NULL;
FormatFn formatOriginal=NULL;
HudLineFn hudLineOriginal=NULL;
void Hud(CharStats* s,DatapanelGUI* panel,int category,bool combat) {
    const CharStats* previous=hudContext;bool previousLine=bowHudLine;
    hudContext=enabled && BowEquipped(s) ? s : NULL;bowHudLine=false;
    if(hudContext)RefreshBowText();
    hudOriginal(s,panel,category,combat);
    hudContext=previous;bowHudLine=previousLine;
}
std::string* FormatHudStat(const CharStats* s,std::string* result,StatsEnumerated stat) {
    if(hudContext==s && stat==STAT_CROSSBOWS) {
        bowHudLine=true;
        return formatOriginal(s,result,BOW_STAT);
    }
    return formatOriginal(s,result,stat);
}
DataPanelLine* HudLine(DatapanelGUI* panel,const std::string& name,const std::string& value,int category) {
    if(hudContext && bowHudLine) {
        bowHudLine=false;
        return hudLineOriginal(panel,std::string(BOW_LABEL)+":",value,category);
    }
    return hudLineOriginal(panel,name,value,category);
}
typedef void (*DetailsFn)(Character*,DatapanelGUI*,const std::string&,int);
DetailsFn detailsOriginal=NULL;
typedef DataPanelLine* (*PlainLineFn)(DatapanelGUI*,const std::string&,const std::string&,int,bool,bool);
typedef void (*RaceLineFn)(Character*,DatapanelGUI*,StatsEnumerated);
std::string Percent(float value) {std::ostringstream out;out<<static_cast<int>(value*100.0f+.5f)<<"%";return out.str();}
void Details(Character* c,DatapanelGUI* panel,const std::string& name,int stat) {
    if(!enabled || stat!=BOW_STAT || !c || !c->stats) {detailsOriginal(c,panel,name,stat);return;}
    PlainLineFn line=reinterpret_cast<PlainLineFn>(BowNative::Address(BowNative::LinePlain));
    RaceLineFn race=reinterpret_cast<RaceLineFn>(BowNative::Address(BowNative::RaceXPLine));
    if(!line || !race) {detailsOriginal(c,panel,name,stat);return;}
    panel->clear();
    line(panel,BOW_LABEL,"",0,false,true);
    line(panel,AFFECTED,"",0,false,true);
    const CharStats* previous=combatContext;combatContext=c->stats;
    const float accuracy=accuracyOriginal(c->stats,STAT_CROSSBOWS);
    const float reload=reloadOriginal(c->stats,STAT_CROSSBOWS);
    combatContext=previous;
    line(panel,ACCURACY,Percent(accuracy),0,false,true);
    line(panel,RELOAD,Percent(reload),0,false,true);
    line(panel,TRAINING,"",0,false,true);
    line(panel,TRAIN_BOW,"",0,false,true);
    race(c,panel,STAT_CROSSBOWS);
}
template<typename F> bool Hook(BowNative::Entry method,void* detour,F** original) {
    intptr_t address=BowNative::Address(method);
    return address && KenshiLib::AddHook(address,detour,original)==KenshiLib::SUCCESS;
}
}
const CharStats* SetBowShotContext(const CharStats* stats) {
    const CharStats* previous=combatContext;
    combatContext=stats;
    return previous;
}
__declspec(dllexport) void startPlugin() {
    InitializeCriticalSection(&valuesLock);
    bool ok=InstallBowLocalization();
    ok=Hook(BowNative::GetStat,(void*)&Get,&getOriginal)&&ok;
    ok=Hook(BowNative::GetStatRef,(void*)&Ref,&refOriginal)&&ok;
    ok=Hook(BowNative::Accuracy,(void*)&Accuracy,&accuracyOriginal)&&ok;
    ok=Hook(BowNative::Reload,(void*)&Reload,&reloadOriginal)&&ok;
    ok=Hook(BowNative::XP,(void*)&XP,&xpOriginal)&&ok;
    ok=Hook(BowNative::Load,(void*)&Load,&loadOriginal)&&ok;
    ok=Hook(BowNative::Save,(void*)&Save,&saveOriginal)&&ok;
    ok=Hook(BowNative::Destroy,(void*)&Destroy,&destroyOriginal)&&ok;
    ok=Hook(BowNative::Setup,(void*)&Setup,&setupOriginal)&&ok;
    ok=Hook(BowNative::UpdateStats,(void*)&Update,&updateOriginal)&&ok;
    ok=Hook(BowNative::Details,(void*)&Details,&detailsOriginal)&&ok;
    ok=Hook(BowNative::HudStats,(void*)&Hud,&hudOriginal)&&ok;
    ok=Hook(BowNative::FormatStat,(void*)&FormatHudStat,&formatOriginal)&&ok;
    ok=Hook(BowNative::HudLine,(void*)&HudLine,&hudLineOriginal)&&ok;
    ok=InstallBowRuntime()&&ok;
    enabled=ok;EnableBowRuntime(ok);
    if(ok) DebugLog("[BowMod] Development skill hooks loaded; game validation pending.");
    else ErrorLog("[BowMod] Incomplete hook setup: all BowMod skill behavior disabled.");
}

