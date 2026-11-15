#pragma once
namespace BowPosePolicy {
inline bool CanPose(bool held,float speed,bool down,bool dead,bool ragdoll,bool stumble,bool gettingUp) {
    return held && speed<.1f && !down && !dead && !ragdoll && !stumble && !gettingUp;
}
inline bool ShowArrow(bool canPose,bool posing,bool gunVisible,int loaded,int index) {
    return canPose && posing && gunVisible && index<loaded;
}
inline bool ShowShootingArrow(bool ready,bool drawing,bool aiming,bool releasing,float frame,int loaded) {
    return ready && !releasing && (drawing ? frame>=9.0f : aiming && loaded>0);
}
}
