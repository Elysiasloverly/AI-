#pragma once

#include "CoreMinimal.h"
#include "Shakes/LegacyCameraShake.h"
#include "RogueDamageCameraShake.generated.h"

/**
 * 玩家受伤时的短促镜头震动。
 *
 * 为什么用 ULegacyCameraShake 而不是自己写 Pattern：
 *   · 它把"时长 / 淡入淡出 / 三轴旋转与位移的振幅、频率、波形"都暴露成属性，
 *     想让手感不一样，继承一个 BP 覆盖参数即可，不需要改 C++；
 *   · HUD 传入的 Scale 会按比例缩放全部振幅，所以伤害越重震得越明显。
 *
 * 默认值按俯视角相机调过：Roll 最明显（最容易读出"被打了"），Pitch/Yaw 克制，
 * 避免频繁受击时准心跟着晃到无法瞄准。
 */
UCLASS(Blueprintable)
class AI_API URogueDamageCameraShake : public ULegacyCameraShake
{
	GENERATED_BODY()

public:
	URogueDamageCameraShake(const FObjectInitializer& ObjectInitializer);
};
