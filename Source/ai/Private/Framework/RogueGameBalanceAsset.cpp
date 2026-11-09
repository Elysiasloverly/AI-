#include "Framework/RogueGameBalanceAsset.h"
#include "Engine/CurveTable.h"

#include "Misc/PackageName.h"
#include "Serialization/CustomVersion.h"

namespace
{
	// 记录当前分组布局版本；已完成迁移的项目资产不再保留旧标量副本。
	const FGuid GameBalanceLayoutVersion(0x7813CF72, 0x2EBB4E2A, 0xA614905F, 0xC30A8DD1);
	constexpr int32 GroupedLayout = 1;
	FCustomVersionRegistration RegisterGameBalanceLayout(GameBalanceLayoutVersion, GroupedLayout, TEXT("RogueGameBalanceLayout"));
}

void URogueGameBalanceAsset::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(GameBalanceLayoutVersion);
	Super::Serialize(Ar);
}



URogueGameBalanceAsset* URogueGameBalanceAsset::GetDefaultGameBalance()
{
	static TWeakObjectPtr<URogueGameBalanceAsset> Cached;
	if (Cached.IsValid())
	{
		return Cached.Get();
	}

	const TCHAR* PackagePath = TEXT("/Game/Data/Core/DA_GameBalance");
	if (!FPackageName::DoesPackageExist(PackagePath))
	{
		return nullptr;
	}

	URogueGameBalanceAsset* Asset = LoadObject<URogueGameBalanceAsset>(
		nullptr, TEXT("/Game/Data/Core/DA_GameBalance.DA_GameBalance"));
	Cached = Asset;
	return Asset;
}

const FRogueEnemyBalanceRow* URogueGameBalanceAsset::FindEnemyBalanceRow(ERogueEnemyType Type) const
{
	for (const FRogueEnemyBalanceRow& Row : EnemyBalanceRows)
	{
		if (Row.EnemyType == Type)
		{
			return &Row;
		}
	}

	return nullptr;
}

bool URogueGameBalanceAsset::TryEvaluateGrowthCurve(FName RowName, float Wave, float& OutValue) const
{
    OutValue = 0.0f;
    if (GrowthCurveTable == nullptr || !FMath::IsFinite(Wave)) { return false; }
    const FRealCurve* Curve = GrowthCurveTable->FindCurve(RowName, TEXT("RogueGameBalance.Growth"), false);
    if (Curve == nullptr || Curve->GetNumKeys() == 0) { return false; }
    const float Value = Curve->Eval(FMath::Max(1.0f, Wave));
    if (!FMath::IsFinite(Value) || Value < 0.0f) { return false; }
    OutValue = Value;
    return true;
}
