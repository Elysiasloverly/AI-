#include "AbilitySystem/RogueGameplayCue_Impact.h"

#include "AbilitySystem/RogueGameplayCue_Downed.h"
#include "AbilitySystemGlobals.h"
#include "Core/RogueGameplayTags.h"
#include "GameplayCueManager.h"
#include "GameplayCueSet.h"
#include "Combat/RogueImpactEffect.h"

void RogueGameplayCues::EnsureNativeImpactCuesRegistered()
{
	// 注意这里取的是**当前生效的**管理器，而不是某个自定义子类的 this：
	// 即使引擎回退到了默认 UGameplayCueManager，这条路径依然能注册成功。
	UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager();
	UGameplayCueSet* CueSet = CueManager != nullptr ? CueManager->GetRuntimeCueSet() : nullptr;
	if (CueSet == nullptr)
	{
		// 正常启动顺序下这里不该为空（运行时 cue set 在引擎初始化时就建好了）。
		// 真为空说明调用得太早 —— 打一条 Warning，避免"注册静默失败"这种最难查的情况。
		UE_LOG(LogTemp, Warning, TEXT("[Rogue] 运行时 GameplayCueSet 尚未建立，原生 cue 注册推迟到下一次机会"));
		return;
	}

	// 原生 cue 清单：**Tag → 通知类**。
	//
	// 注意两种 cue 的基类不同，所以不能再用"一个类吃三个 Tag"的老写法：
	//   · 三个 Impact 共用一个 `UGameplayCueNotify_Static`（无状态，按 MatchedTagName 分流风格）；
	//   · 倒地那个是 `UGameplayCueNotify_Actor`（有实例、有生命周期，见 RogueGameplayCue_Downed.h）。
	//
	// 用类路径（FSoftObjectPath）而不是类指针，是因为 AddCues 收的是路径 ——
	// 原生类（`/Script/ai.XXX`）一样能解析，不需要为它们做 .uasset。
	struct FNativeCueEntry
	{
		FGameplayTag Tag;
		const UClass* NotifyClass;
	};

	static const TArray<FNativeCueEntry> NativeCues = {
		{ RogueGameplayTags::GameplayCue_Impact_Spark,     URogueGameplayCue_Impact::StaticClass() },
		{ RogueGameplayTags::GameplayCue_Impact_Laser,     URogueGameplayCue_Impact::StaticClass() },
		{ RogueGameplayTags::GameplayCue_Impact_Explosion, URogueGameplayCue_Impact::StaticClass() },
		{ RogueGameplayTags::GameplayCue_State_Downed,     ARogueGameplayCue_Downed::StaticClass() },
	};

	// 快路径：全都在（AddCues 内部会重建 GameplayCueDataMap，所以这里查得到）
	bool bAllRegistered = true;
	for (const FNativeCueEntry& Entry : NativeCues)
	{
		if (!CueSet->GameplayCueDataMap.Contains(Entry.Tag))
		{
			bAllRegistered = false;
			break;
		}
	}
	if (bAllRegistered)
	{
		return;
	}

	TArray<FGameplayCueReferencePair> CuesToAdd;
	CuesToAdd.Reserve(NativeCues.Num());
	for (const FNativeCueEntry& Entry : NativeCues)
	{
		if (Entry.NotifyClass != nullptr)
		{
			CuesToAdd.Emplace(Entry.Tag, FSoftObjectPath(Entry.NotifyClass));
		}
	}

	CueSet->AddCues(CuesToAdd);

	// 打一行注册结果，并把"注册完到底认不认"一起报出来。
	// cue 注册失败的表现是"特效全部消失"且没有任何报错 —— 有这行日志，
	// 出问题时一眼就能确认注册跑没跑、以及 cue set 里到底有没有这几个 Tag。
	int32 MappedCount = 0;
	for (const FNativeCueEntry& Entry : NativeCues)
	{
		MappedCount += CueSet->GameplayCueDataMap.Contains(Entry.Tag) ? 1 : 0;
	}
	UE_LOG(LogTemp, Log, TEXT("[Rogue] 原生 GameplayCue 注册：提交 %d 条，cue set 中已映射 %d 条（管理器=%s）"),
		CuesToAdd.Num(), MappedCount, *GetNameSafe(CueManager));
}

bool URogueGameplayCue_Impact::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	// 世界从"这条 cue 落在谁身上"取，而不是 GetWorld()：
	// 这是个 CDO（静态通知没有实例世界），GetWorld() 拿不到东西。
	UWorld* World = IsValid(MyTarget) ? MyTarget->GetWorld() : nullptr;
	if (World == nullptr)
	{
		return false;
	}

	const ERogueImpactVisualStyle Style = ResolveStyle(Parameters);

	// RawMagnitude = 统一缩放（"这次冲击有多大"）。
	// 这是 RawMagnitude 的本义用法：特效的"量级"就是它的大小。
	// <=0 视为没传 → 1.0，避免调用方漏填时特效缩成 0（看不见 = 最难查的 bug）。
	const float UniformScale = Parameters.RawMagnitude > 0.0f ? Parameters.RawMagnitude : 1.0f;

	// NormalizedMagnitude = Z 轴压扁比例（1 = 不压）。
	// 为什么不把三分量都传过来：cue 参数里能承载"形状"的自由浮点只有一个，
	// 而实际用到的非等比缩放只有"贴地压扁"这一种（震荡柱地面波），
	// 压成一个比例既够用又省带宽。
	const float SquashZ = Parameters.NormalizedMagnitude > 0.0f ? Parameters.NormalizedMagnitude : 1.0f;
	const FVector Scale(UniformScale, UniformScale, UniformScale * SquashZ);

	ARogueImpactEffect::SpawnImpactEffect(
		World,
		Parameters.Location,
		Parameters.Normal.Rotation(),
		Style,
		Scale,
		GetLifetimeForStyle(Style),
		Parameters.EffectCauser.Get());

	return true;
}

ERogueImpactVisualStyle URogueGameplayCue_Impact::ResolveStyle(const FGameplayCueParameters& Parameters)
{
	// MatchedTagName 是 UGameplayCueSet 在路由时写进去的（"哪条 Tag 命中了这个通知"）。
	// 它本身是 NotReplicated 字段，但**每个端都是各自被路由一次、各自填一次**，
	// 所以服务端和客户端都能读到，不依赖复制 —— 这也是不需要为每种风格单独写一个 C++ 类的原因。
	const FGameplayTag MatchedTag = Parameters.MatchedTagName;

	if (MatchedTag == RogueGameplayTags::GameplayCue_Impact_Laser)
	{
		return ERogueImpactVisualStyle::Laser;
	}
	if (MatchedTag == RogueGameplayTags::GameplayCue_Impact_Explosion)
	{
		return ERogueImpactVisualStyle::Explosion;
	}

	// Spark 是默认风格（枪口火光、弹体命中、地面波起手都用它）。
	// "贴地压扁"不进 Tag：它由 NormalizedMagnitude 参数承载，见 OnExecute 里的说明。
	return ERogueImpactVisualStyle::Spark;
}

float URogueGameplayCue_Impact::GetLifetimeForStyle(ERogueImpactVisualStyle Style)
{
	// 这个值只决定**池对象多久回收**，与粒子自身时长无关
	// （ARogueImpactEffect::ActivatePooledEffect 内部还有 MinimumLifetime 下限保护）。
	// 所以按风格给一个"肯定够长"的常数即可，不需要每次调用都传。
	switch (Style)
	{
	case ERogueImpactVisualStyle::Explosion: return 0.45f;
	case ERogueImpactVisualStyle::Laser:     return 0.20f;
	case ERogueImpactVisualStyle::Spark:
	default:                                 return 0.18f;
	}
}
