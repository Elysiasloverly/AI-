#pragma once
#if WITH_DEV_AUTOMATION_TESTS
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformMisc.h"

// 仅供显式启动的独立进程探针使用；不能从正式玩法调用退出函数。
namespace RogueProbeSupport
{
inline UWorld *FindGameWorld()
{
	if (!GEngine)
	{
		return nullptr;
	}
	for (const FWorldContext &Context : GEngine->GetWorldContexts())
	{
		if (Context.World() && Context.World()->IsGameWorld())
		{
			return Context.World();
		}
	}
	return nullptr;
}

inline bool Finish(const TCHAR *Probe, bool bSuccess, const TCHAR *Detail)
{
	if (bSuccess)
	{
		UE_LOG(LogTemp, Display, TEXT("[%s] PASS %s"), Probe, Detail);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[%s] FAIL %s"), Probe, Detail);
	}
	FPlatformMisc::RequestExitWithStatus(false, bSuccess ? 0 : 1);
	return false; // 停止ticker，避免退出前再次执行测试步骤。
}
} // namespace RogueProbeSupport
#endif
