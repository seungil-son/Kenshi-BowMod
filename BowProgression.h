#pragma once
#include <cmath>
namespace BowProgression {
inline float EffectiveSkill(float value) {
    if(value<=0.0f) return 0.0f;
    if(value<=60.0f) return value*0.65f;
    if(value>=100.0f) return 100.0f;
    return 39.0f+61.0f*static_cast<float>(std::pow((value-60.0f)/40.0f,1.25f));
}
}
