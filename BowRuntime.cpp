// SPDX-License-Identifier: GPL-3.0-or-later
#include <Windows.h>
#include <algorithm>
#include <sstream>
#include <Debug.h>
#include <core/Functions.h>
#include "NativeAddresses.h"
#include "ArrowAttachment.h"
#include "RunningAttachment.h"
#include "BowPosePolicy.h"
#include <kenshi/CharMovement.h>
#include <ogre/OgreQuaternion.h>
#include <kenshi/Character.h>
#include <kenshi/CharStats.h>
#include <kenshi/Gear.h>
#include <kenshi/GunClass.h>
#include <kenshi/GameData.h>
#include <kenshi/Animation/AnimationClass.h>
#include <ogre/OgreEntity.h>
#include <ogre/OgreAnimationState.h>
const CharStats* SetBowShotContext(const CharStats* stats);

namespace {
bool active=false;
Crossbow* Bow(Character* c) {
    if(!c) return NULL;
    Crossbow* bow=c->getRangedWeapon();
    if(!bow) return NULL;
    GameData* data=bow->getGameData();
    if(!data) return NULL;
    auto flag=data->bdata.find("bowmod bow");
    return flag!=data->bdata.end() && flag->second ? bow : NULL;
}
bool HeldBow(Character* c) {
    Crossbow* bow=Bow(c);
    return bow && BowPosePolicy::CanPose(c->getCurrentWeapon()==bow && (!c->movement || !c->movement->isCurrentlyMoving()),
        c->movement ? c->movement->getCurrentSpeed() : 0.0f,
        c->isDown(),c->isDead(),c->isRagdoll(),c->animation && c->animation->currentlyStumbling(),c->isCurrentlyGettingUp);
}
bool BowClip(const std::string& name) {return name.compare(0,7,"bowmod_")==0;}
bool SplitSprint(const std::string& name) {return name=="bowmod_sprint_lower" || name=="bowmod_sprint_upper";}
bool MovingBow(Character* c) {
    Crossbow* bow=Bow(c);
    return bow && c->getCurrentWeapon()==bow && c->movement && c->movement->isCurrentlyMoving() &&
        c->movement->getCurrentSpeed()>.1f && !c->isDown() && !c->isDead() && !c->isRagdoll() &&
        !c->isCurrentlyGettingUp && c->animation && !c->animation->currentlyStumbling();
}
void StopSplitSprint(AnimationClass* a) {
    if(a->isAnimationPlaying("bowmod_sprint_upper")) a->stopAnimation(std::string("bowmod_sprint_upper"));
    if(a->isAnimationPlaying("bowmod_sprint_lower")) a->stopAnimation(std::string("bowmod_sprint_lower"));
}
bool RetiredClip(const std::string& name) {
    return name=="bowmod_sprint" || name=="bowmod_idle" || name=="Standing Sprint Forward" ||
        name=="bowmod_aim" || name=="bowmod_equip" || name=="Standing Aim Overdraw_Aimming" || name=="Standing Equip Bow_drow_bow";
}
bool CrossbowPose(const std::string& name) {
    return name=="aimH" || name=="aimwalk";
}
void ClearBowPose(AnimationClass* a) {
    typedef void (*StopFn)(AnimationClass*,const std::string&);
    StopFn stop=reinterpret_cast<StopFn>(BowNative::Address(BowNative::StopAnimation));
    const char* names[]={"bowmod_aim","bowmod_aim_hold","bowmod_nock","bowmod_reload","bowmod_release","bowmod_equip","bowmod_sprint","bowmod_idle","Standing Sprint Forward","aimH","aimwalk","stand 1 crossbow"};
    for(unsigned int i=0;i<sizeof(names)/sizeof(names[0]);++i)
        if(a->isAnimationPlaying(names[i])) stop(a,std::string(names[i]));
}
typedef AnimationData* (*DataFn)(AnimationClass*,const std::string&);
typedef void (*ManualFn)(AnimationClass*,AnimationData*,float,float);
typedef void (*UpdateFn)(AnimationClass*,float);
typedef void (*ShootFn)(GunClass*,Character*,RootObject*,StatsEnumerated,const Ogre::Vector3&);
DataFn dataOriginal=NULL;ManualFn manualOriginal=NULL;
UpdateFn updateOriginal=NULL;ShootFn shootOriginal=NULL;
typedef void (*AmmoVisibilityFn)(GunClass*);
typedef void* (*AmmoNodeFn)(const Ogre::MovableObject*);
typedef void (*LocalPositionFn)(void*,const Ogre::Vector3&);
typedef void (*LocalRotationFn)(void*,Ogre::Quaternion);
typedef Ogre::Vector3* (*ReadPositionFn)(void*,Ogre::Vector3*);
typedef Ogre::Quaternion* (*ReadRotationFn)(void*,Ogre::Quaternion*);
AmmoVisibilityFn ammoVisibilityOriginal=NULL;
AmmoNodeFn ammoNode=NULL;LocalPositionFn setAmmoPosition=NULL;
LocalRotationFn setAmmoRotation=NULL;ReadPositionFn readAmmoPosition=NULL;
ReadRotationFn readAmmoRotation=NULL;
typedef void (*VisibleFn)(Ogre::MovableObject*,bool);
VisibleFn setAmmoVisible=NULL;
bool RunningCarry(Character* c) {
    if(!c || !MovingBow(c) || !c->animation) return false;
    AnimationClass* a=c->animation;
    if(a->layer.size()<=LOWER || !a->layer[LOWER])return false;
    const lektor<AnimationClassBase::SingleAnimation*>& list=a->layer[LOWER]->addList;
    for(unsigned int i=0;i<list.size();++i)
        if(list[i] && list[i]->animName=="bowmod_sprint_lower" && list[i]->weight>.01f) return true;
    return false;
}
// Use active entries only: fading-out reloads must not resurrect an arrow.
struct ShotPhase {
    float frame;bool drawing;bool aiming;bool releasing;
    ShotPhase():frame(0),drawing(false),aiming(false),releasing(false){}
};
ShotPhase ShootingPhase(Character* c) {
    ShotPhase result;
    if(!HeldBow(c) || !c->animation) return result;
    float best=0.01f;
    AnimationClass* a=c->animation;
    for(unsigned int l=0;l<a->layer.size();++l) if(a->layer[l]) {
        const lektor<AnimationClassBase::SingleAnimation*>& list=a->layer[l]->addList;
        for(unsigned int i=0;i<list.size();++i) {
            AnimationClassBase::SingleAnimation* s=list[i];if(!s || s->weight<=.01f)continue;
            if(s->animName=="bowmod_aim_hold")result.aiming=true;
            if((s->animName=="bowmod_reload" || s->animName=="bowmod_nock" || s->animName=="bowmod_release") && s->weight>=best) {
                best=s->weight;result.releasing=s->animName=="bowmod_release";result.drawing=!result.releasing;
                float p=(std::max)(0.0f,(std::min)(1.0f,s->currentFrameTime01));
                result.frame=31.0f*(result.releasing ? 1.0f-p : p);
            }
        }
    }
    if(!result.drawing && !result.releasing && result.aiming)result.frame=31.0f;
    return result;
}
void MatchArrowLength(GunClass* gun,Ogre::Vector3& pos,const Ogre::Quaternion& rot) {
    // Both meshes have their tip at zero; Blender +Y becomes native -Z.
    if(gun->liveAmmoMesh.find("arrow_26")!=std::string::npos)
        pos+=rot*Ogre::Vector3(0,0,-SHORT_ARROW_NOCK_OFFSET);
}
void UpdateAmmoVisibility(GunClass* gun,Character* c) {
    if(!gun) return;
    const bool ready=c && HeldBow(c);
    const ShotPhase phase=ShootingPhase(c);
    const bool posing=ready && !phase.releasing && gun->numShotsCurrent>0 && (phase.aiming || phase.drawing);
    const bool carry=RunningCarry(c);
    // Native visibility follows loaded ammunition, even while moving. A carried
    // bow must be slack; leave ammunition counts and reload progress untouched.
    if(c && MovingBow(c)) {
        if(gun->drawnStringMesh)setAmmoVisible(gun->drawnStringMesh,false);
        if(gun->slackStringMesh)setAmmoVisible(gun->slackStringMesh,gun->visible);
    }
    for(unsigned int i=0;i<gun->ammoObjects.size();++i) if(gun->ammoObjects[i])
        setAmmoVisible(gun->ammoObjects[i],gun->visible && i==0 && (carry || posing));
}
volatile LONG loggedAmmo=0;
void AlignAmmo(GunClass* gun,bool forceAim=false) {
    if(!active || !gun || !gun->gunData) return;
    auto flag=gun->gunData->bdata.find("bowmod bow");
    if(flag==gun->gunData->bdata.end() || !flag->second) return;
    const bool carry=!forceAim && RunningCarry(gun->user.getCharacter());
    const float* p=carry ? RUN_ARROW_POS : ARROW_POS;
    const float* q=carry ? RUN_ARROW_ROT : ARROW_ROT;
    Ogre::Vector3 pos(p[0],p[1],p[2]);
    Ogre::Quaternion rot(q[0],q[1],q[2],q[3]);
    // Ooyumi uses native loaded/unloaded meshes, not an independent arrow path.
    if(!carry)MatchArrowLength(gun,pos,rot);
    // Carrying is cosmetic; projectile origin always stays at the firing point.
    gun->gunBarrelOffsetPos=Ogre::Vector3(ARROW_POS[0],ARROW_POS[1],ARROW_POS[2]);
    MatchArrowLength(gun,gun->gunBarrelOffsetPos,Ogre::Quaternion(ARROW_ROT[0],ARROW_ROT[1],ARROW_ROT[2],ARROW_ROT[3]));
    // GunClass::createPhysical creates a child SceneNode for live ammo and
    // applies two -90-degree rotations for vanilla crossbows. Replace that
    // LOCAL transform only: both meshes then share the weapon attachment frame.
    // Never move an attachment parent, character bone or world-space node.
    for(unsigned int i=0;i<gun->ammoObjects.size();++i) {
        Ogre::MovableObject* arrow=gun->ammoObjects[i];
        if(!arrow) continue;
        void* node=ammoNode(arrow);
        if(!node || node==gun->getParentNode()) continue;
        setAmmoPosition(node,pos);setAmmoRotation(node,rot);
        if(InterlockedCompareExchange(&loggedAmmo,1,0)==0) {
            Ogre::Vector3 check;Ogre::Quaternion q;
            readAmmoPosition(node,&check);readAmmoRotation(node,&q);
            std::ostringstream msg;
            msg<<"[BowMod r8] loaded-arrow LOCAL position error="<<(check-pos).length()
               <<", rotation dot="<<(q.w*rot.w+q.x*rot.x+q.y*rot.y+q.z*rot.z)
               <<", ammo nodes="<<gun->ammoObjects.size();
            DebugLog(msg.str());
        }
    }
}
void AmmoVisibility(GunClass* gun) {
    ammoVisibilityOriginal(gun);AlignAmmo(gun);
    if(active && gun && gun->gunData) {
        auto flag=gun->gunData->bdata.find("bowmod bow");
        if(flag!=gun->gunData->bdata.end() && flag->second) UpdateAmmoVisibility(gun,gun->user.getCharacter());
    }
}
AnimationData* Data(AnimationClass* a,const std::string& name) {
    if(active && CrossbowPose(name) && HeldBow(a->me) && a->hasAnimation("bowmod_aim_hold")) {
        AnimationData* custom=dataOriginal(a,"bowmod_aim_hold");
        if(custom) {return custom;}
    }
    return dataOriginal(a,name);
}
void Manual(AnimationClass* a,AnimationData* data,float time,float weight) {
    if(active && data && SplitSprint(data->dataName) && Bow(a->me)) {
        if(MovingBow(a->me)) manualOriginal(a,data,time,weight);
        return;
    }
    if(active && data && Bow(a->me) && (RetiredClip(data->dataName) || RetiredClip(data->animName))) {return;}
    if(active && data && Bow(a->me) && (BowClip(data->dataName) || CrossbowPose(data->dataName)) && !HeldBow(a->me)) {return;}
    // Also covers an aimH pointer cached before the bow was equipped.
    if(active && data && CrossbowPose(data->dataName) && HeldBow(a->me) && a->hasAnimation("bowmod_aim_hold")) {
        AnimationData* custom=dataOriginal(a,"bowmod_aim_hold");
        if(custom) {data=custom;}
    }
    manualOriginal(a,data,time,weight);
}
typedef void (*RunFn)(AnimationClass*,AnimationData*,float,AnimationLayerEnum,float);
typedef void (*RunExtFn)(AnimationClass*,AnimationData*,float,AnimationLayerEnum,float,const std::string&);
RunFn runOriginal=NULL;RunExtFn runExtOriginal=NULL;
AnimationData* RoutePose(AnimationClass* a,AnimationData* data) {
    if(!active || !data || !Bow(a->me)) return data;
    if(SplitSprint(data->dataName)) return MovingBow(a->me) ? data : NULL;
    if(data->dataName=="run lower" && MovingBow(a->me) && a->hasAnimation("bowmod_sprint_lower")) {
        AnimationData* replacement=dataOriginal(a,"bowmod_sprint_lower");
        if(replacement){return replacement;}
    }
    if(RetiredClip(data->dataName) || RetiredClip(data->animName)) {return NULL;}
    if(BowClip(data->dataName) || CrossbowPose(data->dataName) || data->dataName=="stand 1 crossbow") {
        if(!HeldBow(a->me)) {return NULL;}
        if(CrossbowPose(data->dataName) && a->hasAnimation("bowmod_aim_hold")) {return dataOriginal(a,"bowmod_aim_hold");}
    }
    return data;
}
void Run(AnimationClass* a,AnimationData* data,float speed,AnimationLayerEnum layer,float blend) {
    AnimationData* routed=RoutePose(a,data);if(!routed && data) return;
    runOriginal(a,routed,speed,layer,blend);
}
void RunExt(AnimationClass* a,AnimationData* data,float speed,AnimationLayerEnum layer,float blend,const std::string& ext) {
    AnimationData* routed=RoutePose(a,data);if(!routed && data) return;
    runExtOriginal(a,routed,speed,layer,blend,ext);
}
typedef void (*ActionFn)(AnimationClass*,AnimationData*,float,float,bool);
ActionFn actionOriginal=NULL;
void Action(AnimationClass* a,AnimationData* data,float speed,float weight,bool stumble) {
    AnimationData* routed=RoutePose(a,data);if(!routed && data) return;
    actionOriginal(a,routed,speed,weight,stumble);
}
typedef void (*StumbleFn)(AnimationClass*,AnimationData*);
typedef void (*RagdollFn)(Character*,bool,RagdollPart::Enum);
StumbleFn stumbleOriginal=NULL;RagdollFn ragdollOriginal=NULL;
void Stumble(AnimationClass* a,AnimationData* data) {
    stumbleOriginal(a,data);
    Crossbow* bow=active ? Bow(a->me) : NULL;
    if(bow){ClearBowPose(a);UpdateAmmoVisibility(bow->gunClass,NULL);}
}
void Ragdoll(Character* c,bool on,RagdollPart::Enum part) {
    ragdollOriginal(c,on,part);
    Crossbow* bow=active && on ? Bow(c) : NULL;
    if(bow){if(c->animation) ClearBowPose(c->animation);UpdateAmmoVisibility(bow->gunClass,NULL);}
}
void Shoot(GunClass* gun,Character* c,RootObject* target,StatsEnumerated stat,const Ogre::Vector3& aim) {
    const int before=gun->numShotsCurrent;
    Crossbow* bow=active ? Bow(c) : NULL;
    const bool bowShot=bow && bow->gunClass==gun;
    if(bowShot) {
        if(c->animation) StopSplitSprint(c->animation);
        AlignAmmo(gun,true);
        for(unsigned int i=0;i<gun->ammoObjects.size();++i)
            if(gun->ammoObjects[i]) setAmmoVisible(gun->ammoObjects[i],false);
    }
    const CharStats* previous=bowShot ? SetBowShotContext(c->stats) : NULL;
    // Native shoot reads weapon proficiency and STAT_FRIENDLY_FIRE separately.
    // Redirect only crossbow proficiency; precision shooting remains shared.
    shootOriginal(gun,c,target,stat,aim);
    if(bowShot) SetBowShotContext(previous);
    if(bow && bow->gunClass==gun && gun->numShotsCurrent<before && HeldBow(c) && c->animation && c->animation->hasAnimation("bowmod_release"))
        c->animation->playAction("bowmod_release",1.0f,1.0f,false);
}
bool ShootingClip(const std::string& name) {
    return name=="bowmod_reload" || name=="bowmod_nock" || name=="bowmod_release" || name=="bowmod_aim_hold";
}
void FinishShotCrossfade(AnimationClass* a) {
    if(!active || !HeldBow(a->me)) return;
    for(unsigned int l=0;l<a->layer.size();++l) if(a->layer[l]) {
        AnimationClassBase::AnimationLayer* layer=a->layer[l];
        AnimationClassBase::SingleAnimation* incoming=NULL;
        for(unsigned int i=0;i<layer->addList.size();++i) {
            AnimationClassBase::SingleAnimation* s=layer->addList[i];
            if(s && ShootingClip(s->animName) && s->desiredWeight>0 &&
                (!incoming || s->weight>incoming->weight)) incoming=s;
        }
        if(!incoming) continue;
        // Native action playback can keep both shots in addList (not removeList).
        // Preserve logical weights/timers so progression and ammo timing remain
        // native, but evaluate exactly one compensated weapon pose for rendering.
        bool overlapping=false;
        for(unsigned int i=0;i<layer->addList.size();++i) {
            AnimationClassBase::SingleAnimation* s=layer->addList[i];
            if(!s || s==incoming || !ShootingClip(s->animName) || s->weight<=0.0f) continue;
            if(s->mainState && s->mainState!=incoming->mainState) {
                s->mainState->setWeight(0.0f);overlapping=true;
            }
        }
        if(overlapping && incoming->mainState) incoming->mainState->setWeight(1.0f);
        bool outgoing=false;
        for(unsigned int i=0;i<layer->removeList.size();++i) {
            AnimationClassBase::SingleAnimation* s=layer->removeList[i];
            if(!s || !ShootingClip(s->animName)) continue;
            outgoing=true;s->weight=0.0f;s->desiredWeight=0.0f;
            if(s->mainState){s->mainState->setWeight(0.0f);s->mainState->setEnabled(false);}
            // Native fade() removes zero-weight entries on its next update.
            // Do not erase or destroy native animation objects ourselves.
        }
        if(outgoing) {
            incoming->weight=incoming->desiredWeight;
            if(incoming->mainState)incoming->mainState->setWeight(overlapping ? 1.0f : incoming->weight);
        }
    }
}
void Update(AnimationClass* a,float dt) {
    if(active && !MovingBow(a->me)) StopSplitSprint(a);
    if(active && Bow(a->me) && !HeldBow(a->me)) ClearBowPose(a);
    // Prop2 is parented to the opposite hand in the native rig. Blending
    // independent compensation tracks can detach it from the left hand.
    FinishShotCrossfade(a);
    updateOriginal(a,dt);
    FinishShotCrossfade(a);
    if(active && MovingBow(a->me)) {
        // The lower movement request is routed through the native locomotion
        // selector. Supply the missing upper layer at that exact gait phase.
        AnimationClassBase::SingleAnimation* lower=NULL;
        if(a->layer.size()>LOWER && a->layer[LOWER]) {
            const lektor<AnimationClassBase::SingleAnimation*>& list=a->layer[LOWER]->addList;
            for(unsigned int i=0;i<list.size();++i)
                if(list[i] && list[i]->animName=="bowmod_sprint_lower") lower=list[i];
        }
        if(lower && a->hasAnimation("bowmod_sprint_upper")) {
            AnimationData* upper=dataOriginal(a,"bowmod_sprint_upper");
            if(upper) manualOriginal(a,upper,lower->currentFrameTime01,lower->weight);
        } else if(a->isAnimationPlaying("bowmod_sprint_upper")) a->stopAnimation(std::string("bowmod_sprint_upper"));
    }
    Crossbow* bow=active ? Bow(a->me) : NULL;
    if(!bow || !bow->gunClass) return;
    AlignAmmo(bow->gunClass);
    if(!HeldBow(a->me)) ClearBowPose(a);
    // Re-evaluate native loaded/slack visibility when movement ends as well.
    ammoVisibilityOriginal(bow->gunClass);
    UpdateAmmoVisibility(bow->gunClass,a->me);
}

template<typename F> bool Hook(BowNative::Entry method,void* detour,F** original) {
    intptr_t address=BowNative::Address(method);
    return address && KenshiLib::AddHook(address,detour,original)==KenshiLib::SUCCESS;
}
}
bool InstallBowRuntime() {
    bool ok=true;
    ok=Hook(BowNative::AnimData,(void*)&Data,&dataOriginal)&&ok;
    ok=Hook(BowNative::Manual,(void*)&Manual,&manualOriginal)&&ok;
    ok=Hook(BowNative::Shoot,(void*)&Shoot,&shootOriginal)&&ok;
    ok=Hook(BowNative::AnimUpdate,(void*)&Update,&updateOriginal)&&ok;
    ok=Hook(BowNative::AmmoVisibility,(void*)&AmmoVisibility,&ammoVisibilityOriginal)&&ok;
    ok=Hook(BowNative::RunLayer,(void*)&Run,&runOriginal)&&ok;
    ok=Hook(BowNative::RunLayerExt,(void*)&RunExt,&runExtOriginal)&&ok;
    ok=Hook(BowNative::StartStumble,(void*)&Stumble,&stumbleOriginal)&&ok;
    ok=Hook(BowNative::RagdollCallback,(void*)&Ragdoll,&ragdollOriginal)&&ok;
    ok=Hook(BowNative::PlayAction,(void*)&Action,&actionOriginal)&&ok;
    HMODULE ogre=GetModuleHandleA("OgreMain_x64.dll");
    ammoNode=reinterpret_cast<AmmoNodeFn>(GetProcAddress(ogre,"?getParentSceneNode@MovableObject@Ogre@@QEBAPEAVSceneNode@2@XZ"));
    setAmmoPosition=reinterpret_cast<LocalPositionFn>(GetProcAddress(ogre,"?setPosition@Node@Ogre@@QEAAXAEBVVector3@2@@Z"));
    setAmmoRotation=reinterpret_cast<LocalRotationFn>(GetProcAddress(ogre,"?setOrientation@Node@Ogre@@QEAAXVQuaternion@2@@Z"));
    readAmmoPosition=reinterpret_cast<ReadPositionFn>(GetProcAddress(ogre,"?getPosition@Node@Ogre@@QEBA?AVVector3@2@XZ"));
    readAmmoRotation=reinterpret_cast<ReadRotationFn>(GetProcAddress(ogre,"?getOrientation@Node@Ogre@@QEBA?AVQuaternion@2@XZ"));
    setAmmoVisible=reinterpret_cast<VisibleFn>(GetProcAddress(ogre,"?setVisible@MovableObject@Ogre@@QEAAX_N@Z"));
    ok=ammoNode && setAmmoPosition && setAmmoRotation && readAmmoPosition && readAmmoRotation && setAmmoVisible && ok;
    if(ok) DebugLog("[BowMod r13] Split sprint lower/upper enabled for held bows; upper follows lower gait phase. Overdraw aim and equip remain retired.");
    return ok;
}
void EnableBowRuntime(bool on) {active=on;}
