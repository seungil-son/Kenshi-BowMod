from pathlib import Path
import json
from bow_translations import LANG
w=Path(__file__).resolve().parent;root=w.parent
keys=['BOW_LABEL','BOW_DESCRIPTION','AFFECTED','ACCURACY','RELOAD','PRECISION','TRAINING','TRAIN_BOW','SHARED_PRECISION']
langs=['en','ko','ja','zh','es','ru','pt']
def q(s):return '"'+''.join('\\x%02X'%b for b in s.encode('utf-8'))+'"'
records={}
for lang in langs:
 t=LANG[lang];out={}
 for i in range(6):
  inches=32 if i in [0,1,5] else 26
  desc=t['roles'][i]+' '+t['ammo'].format(n=inches)
  out[str(400+i)+'-BowMod.mod']=[t['names'][i],desc]
  out[str(600+i)+'-BowMod.mod']=[t['test']+' '+str(i+1)+' - '+t['names'][i],'']
 for i,n in enumerate([26,32]):out[str(210+i)+'-BowMod.mod']=[t['arrow'].format(n=n),t['arrowdesc']]
 for i in range(3):out[str(500+i)+'-BowMod.mod']=[t['research'][i],t['researchdesc'][i]]
 out['610-BowMod.mod']=[t['test'],''];out['620-BowMod.mod']=[t['test'],t['testdesc']]
 records[lang]=out
 path=root/'package/BowMod/Localization'/f'{lang}.json';path.parent.mkdir(exist_ok=True);path.write_text(json.dumps({'records':out,'ui':dict(zip(keys,t['ui']))},ensure_ascii=False,indent=2),encoding='utf-8')
(root/'BowText.h').write_text('#pragma once\n'+''.join('extern const char* '+k+';\n' for k in keys))
s='''// Generated from work/bow_translations.py. UTF-8 bytes in ASCII source.
#define UNICODE
#include <Windows.h>
#include <core/Functions.h>
#include <Debug.h>
#include <kenshi/GameDataManager.h>
#include <kenshi/GameData.h>
#include <kenshi/LocaleInfo.h>
#include <kenshi/Globals.h>
#include <kenshi/GameWorld.h>
#include <kenshi/RootObjectBase.h>
#include <kenshi/gui/ForgottenGUI.h>
#include <sstream>
#include "BowText.h"
#include "BowLocalization.h"
#include "NativeAddresses.h"
'''
s+='\n'.join('const char* '+k+'='+q(LANG['en']['ui'][i])+';' for i,k in enumerate(keys))+'\nnamespace {\n'
s+='static const char* ui[7][9]={\n'+',\n'.join('{'+','.join(q(x) for x in LANG[l]['ui'])+'}' for l in langs)+'};\n'
s+='struct Translation {const char* id; const char* name[7]; const char* description[7];};\nstatic const Translation entries[]={\n'
s+=',\n'.join('{'+q(id)+',{'+','.join(q(records[l][id][0]) for l in langs)+'},{'+','.join(q(records[l][id][1]) for l in langs)+'}}' for id in records['en'])+'};\n'
s+='''int Language() {
    LocaleManager* manager=LocaleManager::getInstance();
    LocaleInfo* locale=manager ? manager->getCurrentLocale() : NULL;
    if(!locale) return 0;
    const std::string& id=locale->id;
    if(id.compare(0,2,"ko")==0)return 1;
    if(id.compare(0,2,"ja")==0)return 2;
    if(id=="zh_CN" || id=="zh" || id=="zh-CN")return 3;
    if(id.compare(0,2,"es")==0)return 4;
    if(id.compare(0,2,"ru")==0)return 5;
    if(id.compare(0,2,"pt")==0)return 6;
    return 0;
}
typedef std::string* (*NameFn)(const RootObjectBase*,std::string*);
NameFn nameOriginal=NULL;
std::string* ItemName(const RootObjectBase* object,std::string* result) {
    nameOriginal(object,result);
    if(!object || !object->data)return result;
    const std::string& id=object->data->stringID;
    // Only our six weapons and two ammunition records. Preserve custom names.
    if(id!="400-BowMod.mod" && id!="401-BowMod.mod" && id!="402-BowMod.mod" &&
       id!="403-BowMod.mod" && id!="404-BowMod.mod" && id!="405-BowMod.mod" &&
       id!="210-BowMod.mod" && id!="211-BowMod.mod")return result;
    for(unsigned int i=0;i<sizeof(entries)/sizeof(entries[0]);++i) {
        if(id!=entries[i].id)continue;
        for(int language=0;language<7;++language) {
            if(*result==entries[i].name[language]) {
                *result=entries[i].name[Language()];
                return result;
            }
        }
        return result;
    }
    return result;
}
typedef void (*PostFn)(GameDataManager*);
PostFn postOriginal=NULL;
typedef void (*GuiFn)(ForgottenGUI*);
GuiFn guiOriginal=NULL;
void UpdateGui(ForgottenGUI* gui) {
    if(!gui->isLoading())RefreshBowLocalization();
    guiOriginal(gui);
}
void Apply(GameDataManager* manager) {
    const int language=Language();
    unsigned int changed=0, foundCount=0;
    for(unsigned int i=0;i<sizeof(entries)/sizeof(entries[0]);++i) {
        auto found=manager->gamedataSID.find(entries[i].id);
        if(found==manager->gamedataSID.end() || !found->second)continue;
        GameData* data=found->second;
        ++foundCount;
        if(data->name!=entries[i].name[language]) {
            manager->renameData(data,entries[i].name[language]);
            ++changed;
        }
        if(entries[i].description[language][0] && data->sdata["description"]!=entries[i].description[language]) {
            data->sdata["description"]=entries[i].description[language];
            ++changed;
        }
    }
    static int reportedLanguage=-1;
    if(changed || (foundCount && reportedLanguage!=language)) {
        std::ostringstream message;
        message<<"[BowMod] Localization language="<<language<<" records="<<foundCount<<" changed="<<changed;
        DebugLog(message.str());
        reportedLanguage=language;
    }
    RefreshBowText();
}
void Post(GameDataManager* manager) {
    postOriginal(manager);
    Apply(manager);
}
}
void RefreshBowLocalization() {
    // Called only from the GUI thread while the loading screen is inactive.
    static DWORD last=0;
    DWORD now=GetTickCount();
    if(now-last<1000 || !ou)return;
    last=now;
    Apply(&ou->gamedata);
}
void RefreshBowText() {
    const int language=Language();
'''
s+=''.join('    '+k+'=ui[language]['+str(i)+'];\n' for i,k in enumerate(keys))+'}\n'
s+='''bool InstallBowLocalization() {
    intptr_t address=BowNative::Address(BowNative::DataPostProcess);
    bool ok=address && KenshiLib::AddHook(address,(void*)&Post,&postOriginal)==KenshiLib::SUCCESS;
    intptr_t guiAddress=BowNative::Address(BowNative::DiagnosticGuiUpdate);
    ok=ok && guiAddress && KenshiLib::AddHook(guiAddress,(void*)&UpdateGui,&guiOriginal)==KenshiLib::SUCCESS;
    intptr_t nameAddress=BowNative::Address(BowNative::ObjectName);
    return ok && nameAddress && KenshiLib::AddHook(nameAddress,(void*)&ItemName,&nameOriginal)==KenshiLib::SUCCESS;
}
'''
(root/'BowLocalization.cpp').write_text(s,encoding='ascii')
(w/'localization_records.json').write_text(json.dumps(records,ensure_ascii=False,indent=2),encoding='utf-8')
print('Generated 7 languages;',len(records['en']),'records each;',len(keys),'UI strings')
