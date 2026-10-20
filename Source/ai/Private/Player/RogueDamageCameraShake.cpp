#include "Player/RogueDamageCameraShake.h"

URogueDamageCameraShake::URogueDamageCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 0.22s：短到不打断操作，长到能被眼睛捕捉到。淡出比淡入长，收尾才不会"突然停下"。
	OscillationDuration = 0.22f;
	OscillationBlendInTime = 0.015f;
	OscillationBlendOutTime = 0.17f;

	// 旋转：Roll 给足（俯视角下最像"挨了一下"），Pitch/Yaw 只做轻微方向感。
	RotOscillation.Pitch.Amplitude = 0.9f;
	RotOscillation.Pitch.Frequency = 26.0f;
	RotOscillation.Yaw.Amplitude = 0.5f;
	RotOscillation.Yaw.Frequency = 22.0f;
	RotOscillation.Roll.Amplitude = 1.6f;
	RotOscillation.Roll.Frequency = 30.0f;

	// 位移：Z 轴略大，配合"被打得晃一下"，X/Y 避免相机平移感过强。
	LocOscillation.X.Amplitude = 1.2f;
	LocOscillation.X.Frequency = 25.0f;
	LocOscillation.Y.Amplitude = 1.2f;
	LocOscillation.Y.Frequency = 25.0f;
	LocOscillation.Z.Amplitude = 2.0f;
	LocOscillation.Z.Frequency = 28.0f;
}
