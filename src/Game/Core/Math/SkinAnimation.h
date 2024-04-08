#ifndef __ANIMATION_H_
#define __ANIMATION_H_

#include "../Skinning/SkinStructs.h"

class SkinAnimation
{
public:
    SkinAnimation() = default;
    SkinAnimation(AnimationInfo &info);
    ~SkinAnimation();

    UDim2 Update(double delta);
    UDim2 Calculate(double alpha);

private:
    double        m_CurrentTime = 0.0;
    AnimationInfo m_Info;
};

#endif // __ANIMATION_H_