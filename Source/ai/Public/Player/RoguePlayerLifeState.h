#pragma once

#include "CoreMinimal.h"
#include "RoguePlayerLifeState.generated.h"

/** 玩家生命周期由服务端决定；濒死和本局出局都属于不可参与战斗的幽灵状态。 */
UENUM(BlueprintType)
enum class ERoguePlayerLifePhase : uint8
{
	Alive UMETA(DisplayName = "正常"),
	Downed UMETA(DisplayName = "濒死等待复活"),
	Eliminated UMETA(DisplayName = "本局结束")
};

/** 状态和截止时刻放在同一复制属性中，客户端不依靠 GAS 效果到达/过期的先后猜测复活。 */
USTRUCT(BlueprintType)
struct AI_API FRoguePlayerLifeState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	ERoguePlayerLifePhase Phase = ERoguePlayerLifePhase::Alive;

	/** 服务端世界时间；只在 Downed 期间有效，团灭或复活时清零。 */
	UPROPERTY(BlueprintReadOnly)
	float ReviveServerTime = 0.0f;

	/** 用同一次网络序列化发送状态与时间，避免结构成员分别增量复制。 */
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
	{
		uint8 PhaseBits = static_cast<uint8>(Phase);
		Ar.SerializeBits(&PhaseBits, 2);
		Ar << ReviveServerTime;
		if (Ar.IsLoading())
		{
			Phase = static_cast<ERoguePlayerLifePhase>(PhaseBits);
		}
		bOutSuccess = PhaseBits <= static_cast<uint8>(ERoguePlayerLifePhase::Eliminated) && !Ar.IsError();
		return true;
	}
};

template<>
struct TStructOpsTypeTraits<FRoguePlayerLifeState> : public TStructOpsTypeTraitsBase2<FRoguePlayerLifeState>
{
	enum { WithNetSerializer = true };
};

namespace RoguePlayerLifeCycle
{
	inline constexpr float DownedDurationSeconds = 10.0f;
}
