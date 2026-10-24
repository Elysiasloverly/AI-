#include "Progression/Meta/RogueUpgradeTreeDefinition.h"
#include "Misc/PackageName.h"
#if WITH_EDITOR
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Progression/Meta/RogueProgressionSubsystem.h"
#include "Progression/Meta/RogueProgressionSaveGame.h"
#endif

namespace RogueMetaDefinitions
{
	bool IsQuantityType(ERogueUpgradeType Type)
	{
		return Type == ERogueUpgradeType::ProjectileCount || Type == ERogueUpgradeType::ScytheCount
			|| Type == ERogueUpgradeType::RocketCount || Type == ERogueUpgradeType::LaserCount
			|| Type == ERogueUpgradeType::HellTowerCount || Type == ERogueUpgradeType::MortarCount;
	}

	bool IsDedicatedType(ERogueUpgradeType Type)
	{
		return Type == ERogueUpgradeType::ProjectileBurst || Type == ERogueUpgradeType::RocketBlastRadius
			|| Type == ERogueUpgradeType::HellTowerDamageRamp || Type == ERogueUpgradeType::ScytheSize || Type == ERogueUpgradeType::ScytheLifesteal
			|| Type == ERogueUpgradeType::LaserRefraction || Type == ERogueUpgradeType::MortarBlastRadius;
	}
}

URogueUpgradeTreeDefinition::URogueUpgradeTreeDefinition()
{
	ResetToDefaults();
}

void URogueUpgradeTreeDefinition::ResetToDefaults()
{
	Nodes.Reset();
	auto Add = [this](const TCHAR* Id, const TCHAR* Title, const TCHAR* Description, ERogueMetaNodeKind Kind,
		ERogueUpgradeType Type, float X, float Y, const TCHAR* Prerequisite = nullptr) -> FRogueUpgradeTreeNode&
	{
		FRogueUpgradeTreeNode& Node = Nodes.AddDefaulted_GetRef();
		Node.NodeId = FName(Id);
		Node.Title = FText::FromString(Title);
		Node.Description = FText::FromString(Description);
		Node.Kind = Kind;
		Node.UpgradeType = Type;
		Node.TreePosition = FVector2D(X, Y);
		if (Prerequisite) { Node.Prerequisites.Add(FName(Prerequisite)); }
		return Node;
	};

	Add(TEXT("Weapon.Projectile"), TEXT("御札"), TEXT("自动瞄准近处敌人，向前发射御札。\n初始武器，开局自带；数量卡可增加每轮弹幕。"), ERogueMetaNodeKind::Weapon,
		ERogueUpgradeType::ProjectileCount, 0, 780).bInitiallyUnlocked = true;
	Add(TEXT("Card.ProjectileCount"), TEXT("御札数量卡"), TEXT("初始开放。本局抽到后，增加每轮发射的御札数量。"), ERogueMetaNodeKind::UpgradeCard,
		ERogueUpgradeType::ProjectileCount, 0, 520, TEXT("Weapon.Projectile")).bInitiallyUnlocked = true;

	struct FWeapon { const TCHAR* Id; const TCHAR* Name; ERogueUpgradeType Type; const TCHAR* Description; const TCHAR* CardDescription; };
	const FWeapon Weapons[] = {
		{ TEXT("Scythe"), TEXT("银刃"), ERogueUpgradeType::ScytheCount, TEXT("刀刃环绕自身旋转，切割靠近的敌人。\n解锁后可在本局升级与商店中抽取。"), TEXT("开放银刃数量卡；本局持有银刃后，可继续抽卡增加环绕刀刃。") },
		{ TEXT("Rocket"), TEXT("八卦炉"), ERogueUpgradeType::RocketCount, TEXT("向敌人齐射爆裂弹，命中后造成范围伤害。\n解锁后可在本局升级与商店中抽取。"), TEXT("开放八卦炉数量卡；持有后，可增加每轮齐射的爆裂弹数量。") },
		{ TEXT("Laser"), TEXT("极限火花"), ERogueUpgradeType::LaserCount, TEXT("自动发射激光攻击敌人，专属卡可追加折射。\n解锁后可在本局升级与商店中抽取。"), TEXT("开放极限火花数量卡；持有后，可增加每轮发射的激光数量。") },
		{ TEXT("HellTower"), TEXT("御柱"), ERogueUpgradeType::HellTowerCount, TEXT("持续锁定敌人照射，连续攻击同一目标时伤害递增。\n解锁后可在本局升级与商店中抽取。"), TEXT("开放御柱数量卡；持有后，可增加同时锁定的敌人数。") },
		{ TEXT("Mortar"), TEXT("阴阳玉"), ERogueUpgradeType::MortarCount, TEXT("抛出阴阳玉轰击敌人所在区域，落地时爆炸。\n解锁后可在本局升级与商店中抽取。"), TEXT("开放阴阳玉数量卡；持有后，可增加每轮投出的阴阳玉数量。") }
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Weapons); ++Index)
	{
		const FWeapon& Weapon = Weapons[Index];
		const FString WeaponId = FString::Printf(TEXT("Weapon.%s"), Weapon.Id);
		const FString CardId = FString::Printf(TEXT("Card.%sCount"), Weapon.Id);
		const FString Description = Weapon.Description;
		const FString CardTitle = FString::Printf(TEXT("%s数量卡"), Weapon.Name);
		const float X = (Index + 1) * 330.0f;
		Add(*WeaponId, Weapon.Name, *Description, ERogueMetaNodeKind::Weapon, Weapon.Type, X, 780);
		Add(*CardId, *CardTitle, Weapon.CardDescription,
			ERogueMetaNodeKind::UpgradeCard, Weapon.Type, X, 520, *WeaponId);
	}
	Add(TEXT("Card.ScytheSize"), TEXT("银刃巨化"), TEXT("开放银刃巨化卡；本局抽到后扩大刀刃外观和伤害范围，环绕距离不变。"),
		ERogueMetaNodeKind::UpgradeCard, ERogueUpgradeType::ScytheSize, 330, 260, TEXT("Card.ScytheCount"));
	Add(TEXT("Card.ScytheLifesteal"), TEXT("红雾吸血"), TEXT("开放红雾吸血卡；本局抽到后，银刃造成伤害时为自身回复生命。"),
		ERogueMetaNodeKind::UpgradeCard, ERogueUpgradeType::ScytheLifesteal, 330, 0, TEXT("Card.ScytheSize"));
	Add(TEXT("Card.LaserRefraction"), TEXT("星尘折射"), TEXT("开放星尘折射卡；本局抽到后，激光可继续跳向附近敌人。"),
		ERogueMetaNodeKind::UpgradeCard, ERogueUpgradeType::LaserRefraction, 990, 260, TEXT("Card.LaserCount"));
	Add(TEXT("Card.MortarBlastRadius"), TEXT("阴阳玉爆炸范围"), TEXT("开放阴阳玉扩散卡；本局抽到后扩大爆炸半径，覆盖更多敌人。"),
		ERogueMetaNodeKind::UpgradeCard, ERogueUpgradeType::MortarBlastRadius, 1650, 260, TEXT("Card.MortarCount"));

	Add(TEXT("Card.ProjectileBurst"), TEXT("灵符连诵"), TEXT("开放「灵符连诵」卡。御札每轮攻击额外连射 1 次；每次连射都发出完整弹幕。"),
		ERogueMetaNodeKind::UpgradeCard, ERogueUpgradeType::ProjectileBurst, 0, 0, TEXT("Card.ProjectileCount"));
	Add(TEXT("Card.RocketBlastRadius"), TEXT("炉心扩散"), TEXT("开放「炉心扩散」卡。八卦炉爆炸半径 +90 厘米，扩大爆裂弹的伤害覆盖范围。"),
		ERogueMetaNodeKind::UpgradeCard, ERogueUpgradeType::RocketBlastRadius, 0, 0, TEXT("Card.RocketCount"));
	Add(TEXT("Card.HellTowerDamageRamp"), TEXT("神威增幅"), TEXT("开放「神威增幅」卡。御柱每跳伤害递增值 +0.1；持续锁定同一目标时，伤害增长更快。"),
		ERogueMetaNodeKind::UpgradeCard, ERogueUpgradeType::HellTowerDamageRamp, 0, 0, TEXT("Card.HellTowerCount"));

	FRogueUpgradeTreeNode& Armor = Add(TEXT("Armor.Unlock"), TEXT("解锁护甲"),
		TEXT("启用玩家数值配置中的基础护甲，并开放护甲相关升级卡。"),
		ERogueMetaNodeKind::ArmorUnlock, ERogueUpgradeType::ArmorCapacity, 660, 780);
	Armor.Cost = 3;
	struct FStat { const TCHAR* Id; const TCHAR* Title; const TCHAR* Description; ERogueMetaStat Stat; float Bonus; };
	const FStat Stats[] = {
		{ TEXT("AttackPower"), TEXT("基础攻击力"), TEXT("每级额外增加 3 点攻击力。"), ERogueMetaStat::AttackPower, 3.0f },
		{ TEXT("MaxHealth"), TEXT("基础生命"), TEXT("每级增加 20 点最大生命。"), ERogueMetaStat::MaxHealth, 20.0f },
		{ TEXT("ArmorCapacity"), TEXT("基础护甲"), TEXT("解锁护甲后，每级增加 10 点最大护甲。"), ERogueMetaStat::ArmorCapacity, 10.0f },
		{ TEXT("MoveSpeed"), TEXT("基础移速"), TEXT("每级增加 30 厘米/秒移动速度。"), ERogueMetaStat::MoveSpeed, 30.0f },
		{ TEXT("PickupRadius"), TEXT("吸取范围"), TEXT("每级增加 40 厘米经验吸取半径。"), ERogueMetaStat::PickupRadius, 40.0f },
		{ TEXT("HealthRegen"), TEXT("生命恢复"), TEXT("每级增加每秒 0.2 点生命恢复。"), ERogueMetaStat::HealthRegen, 0.2f },
		{ TEXT("AttackSpeed"), TEXT("攻击间隔缩短"), TEXT("每级将武器攻击间隔额外缩短 0.03 秒，受武器最短间隔限制。"), ERogueMetaStat::AttackSpeed, 0.03f },
		{ TEXT("ExperienceGain"), TEXT("经验获取"), TEXT("每级经验获取倍率增加 0.05（5 个百分点）。"), ERogueMetaStat::ExperienceGain, 0.05f }
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Stats); ++Index)
	{
		const FStat& Stat = Stats[Index];
		const FString Id = FString::Printf(TEXT("Stat.%s"), Stat.Id);
		FRogueUpgradeTreeNode& Node = Add(*Id, Stat.Title, Stat.Description, ERogueMetaNodeKind::BaseStat,
			ERogueUpgradeType::MaxHealth, (Index % 4) * 330.0f, Index < 4 ? 520.0f : 130.0f,
			Stat.Stat == ERogueMetaStat::ArmorCapacity ? TEXT("Armor.Unlock") : nullptr);
		Node.Cost = 3;
		Node.MaxRank = 5;
		Node.Stat = Stat.Stat;
		Node.BonusPerRank = Stat.Bonus;
	}
}

const URogueUpgradeTreeDefinition* URogueUpgradeTreeDefinition::GetDefaultDefinition()
{
	static TWeakObjectPtr<URogueUpgradeTreeDefinition> CachedAsset;
	if (CachedAsset.IsValid()) { return CachedAsset.Get(); }
	if (FPackageName::DoesPackageExist(TEXT("/Game/Data/Progression/DA_UpgradeTree")))
	{
		if (URogueUpgradeTreeDefinition* Asset = LoadObject<URogueUpgradeTreeDefinition>(nullptr,
			TEXT("/Game/Data/Progression/DA_UpgradeTree.DA_UpgradeTree")))
		{
			CachedAsset = Asset;
			return Asset;
		}
	}
	return GetDefault<URogueUpgradeTreeDefinition>();
}

const FRogueUpgradeTreeNode* URogueUpgradeTreeDefinition::FindNode(FName NodeId) const
{
	return Nodes.FindByPredicate([NodeId](const FRogueUpgradeTreeNode& Node) { return Node.NodeId == NodeId; });
}

int32 URogueUpgradeTreeDefinition::GetMaximumRank(const FRogueUpgradeTreeNode& Node) const
{
	return Node.Kind == ERogueMetaNodeKind::BaseStat ? FMath::Clamp(Node.MaxRank, 1, 5) : 1;
}

int32 URogueUpgradeTreeDefinition::GetNodeRank(const FRogueProgressionSnapshot& Snapshot, FName NodeId) const
{
	TSet<FName> Path;
	TFunction<int32(FName)> Resolve = [this, &Snapshot, &Path, &Resolve](FName Id) -> int32
	{
		const FRogueUpgradeTreeNode* Node = FindNode(Id);
		if (Node == nullptr || Path.Contains(Id) || Path.Num() >= 128) return 0;
		const int32 RawRank = Node->bInitiallyUnlocked ? GetMaximumRank(*Node)
			: FMath::Clamp(Snapshot.NodeRanks.FindRef(Id), 0, GetMaximumRank(*Node));
		if (RawRank == 0) return 0;
		Path.Add(Id);
		for (FName Parent : Node->Prerequisites)
		{
			if (Resolve(Parent) == 0) { Path.Remove(Id); return 0; }
		}
		Path.Remove(Id);
		return RawRank;
	};
	return Resolve(NodeId);
}

FRogueProgressionSnapshot URogueUpgradeTreeDefinition::SanitizeSnapshot(const FRogueProgressionSnapshot& Snapshot) const
{
	FRogueProgressionSnapshot Clean;
	// Build from reachable prerequisites, rather than trusting mutually dependent purchased nodes.
	// This also removes cycles and orphaned nodes without recursion on untrusted network data.
	for (int32 Pass = 0; Pass < FMath::Min(Nodes.Num(), 128); ++Pass)
	{
		bool bChanged = false;
		for (const FRogueUpgradeTreeNode& Node : Nodes)
		{
			if (Clean.NodeRanks.Num() >= 128) { break; }
			if (Node.NodeId.IsNone() || Clean.NodeRanks.Contains(Node.NodeId)) { continue; }
			const int32* Requested = Snapshot.NodeRanks.Find(Node.NodeId);
			if (!Requested || *Requested <= 0 || Node.bInitiallyUnlocked) { continue; }
			const bool bReady = !Node.Prerequisites.ContainsByPredicate([this, &Clean](FName Prerequisite)
			{
				return GetNodeRank(Clean, Prerequisite) == 0;
			});
			if (bReady)
			{
				Clean.NodeRanks.Add(Node.NodeId, FMath::Min(*Requested, GetMaximumRank(Node)));
				bChanged = true;
			}
		}
		if (!bChanged) { break; }
	}
	return Clean;
}

FText URogueUpgradeTreeDefinition::GetPurchaseFailure(const FRogueProgressionSnapshot& Snapshot, int32 IceBricks, FName NodeId) const
{
	const FRogueUpgradeTreeNode* Node = FindNode(NodeId);
	if (!Node) { return FText::FromString(TEXT("没有找到这个升级节点。")); }
	if (GetNodeRank(Snapshot, NodeId) >= GetMaximumRank(*Node))
	{
		return FText::FromString(Node->Kind == ERogueMetaNodeKind::BaseStat ? TEXT("已达到最高等级。") : TEXT("已经解锁。"));
	}
	for (FName Prerequisite : Node->Prerequisites)
	{
		if (GetNodeRank(Snapshot, Prerequisite) <= 0)
		{
			const FRogueUpgradeTreeNode* Parent = FindNode(Prerequisite);
			return FText::Format(FText::FromString(TEXT("请先解锁：{0}")), Parent ? Parent->Title : FText::FromName(Prerequisite));
		}
	}
	if (IceBricks < FMath::Max(Node->Cost, 1)) { return FText::FromString(TEXT("冰砖不足。")); }
	return FText::GetEmpty();
}

bool URogueUpgradeTreeDefinition::IsWeaponUnlocked(const FRogueProgressionSnapshot& Snapshot, ERogueUpgradeType QuantityType) const
{
	if (QuantityType == ERogueUpgradeType::ProjectileCount) { return true; }
	const FRogueUpgradeTreeNode* Node = Nodes.FindByPredicate([QuantityType](const FRogueUpgradeTreeNode& Entry)
	{
		return Entry.Kind == ERogueMetaNodeKind::Weapon && Entry.UpgradeType == QuantityType;
	});
	return Node && GetNodeRank(Snapshot, Node->NodeId) > 0;
}

bool URogueUpgradeTreeDefinition::IsUpgradeAvailable(const FRogueProgressionSnapshot& Snapshot, ERogueUpgradeType Type,
	bool bWeaponAlreadyOwned) const
{
	if (Type == ERogueUpgradeType::ProjectileCount) { return true; }
	if (Type == ERogueUpgradeType::Armor || Type == ERogueUpgradeType::ArmorCapacity) { return IsArmorUnlocked(Snapshot); }
	if (RogueMetaDefinitions::IsQuantityType(Type) && !bWeaponAlreadyOwned) { return IsWeaponUnlocked(Snapshot, Type); }
	if (RogueMetaDefinitions::IsQuantityType(Type) || RogueMetaDefinitions::IsDedicatedType(Type))
	{
		const FRogueUpgradeTreeNode* Node = Nodes.FindByPredicate([Type](const FRogueUpgradeTreeNode& Entry)
		{
			return Entry.Kind == ERogueMetaNodeKind::UpgradeCard && Entry.UpgradeType == Type;
		});
		return Node && GetNodeRank(Snapshot, Node->NodeId) > 0;
	}
	return true;
}

bool URogueUpgradeTreeDefinition::IsArmorUnlocked(const FRogueProgressionSnapshot& Snapshot) const
{
	return Nodes.ContainsByPredicate([this, &Snapshot](const FRogueUpgradeTreeNode& Node)
	{
		return Node.Kind == ERogueMetaNodeKind::ArmorUnlock && GetNodeRank(Snapshot, Node.NodeId) > 0;
	});
}

float URogueUpgradeTreeDefinition::GetStatBonus(const FRogueProgressionSnapshot& Snapshot, ERogueMetaStat Stat) const
{
	if (Stat == ERogueMetaStat::None || (Stat == ERogueMetaStat::ArmorCapacity && !IsArmorUnlocked(Snapshot))) { return 0; }
	float Bonus = 0.0f;
	for (const FRogueUpgradeTreeNode& Node : Nodes)
	{
		if (Node.Kind == ERogueMetaNodeKind::BaseStat && Node.Stat == Stat && FMath::IsFinite(Node.BonusPerRank))
		{
			Bonus += FMath::Max(0.0f, Node.BonusPerRank) * GetNodeRank(Snapshot, Node.NodeId);
		}
	}
	return FMath::IsFinite(Bonus) ? Bonus : 0.0f;
}

#if WITH_EDITOR
namespace
{
	URogueProgressionSubsystem* FindDebugProgression(int32 PIEInstance, int32 LocalPlayerIndex, bool& bHasGameWorld, FText& Error)
	{
		bHasGameWorld = false;
		if (GEngine == nullptr) return nullptr;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType != EWorldType::PIE && Context.WorldType != EWorldType::Game) continue;
			bHasGameWorld = true;
			if (Context.WorldType == EWorldType::PIE && Context.PIEInstance != PIEInstance) continue;
			if (Context.WorldType == EWorldType::Game && PIEInstance != 0) continue;
			UGameInstance* Instance = Context.World() ? Context.World()->GetGameInstance() : nullptr;
			if (Instance && Instance->GetLocalPlayers().IsValidIndex(LocalPlayerIndex))
			{
				if (URogueProgressionSubsystem* Live = Instance->GetLocalPlayers()[LocalPlayerIndex]->GetSubsystem<URogueProgressionSubsystem>()) return Live;
			}
		}
		if (bHasGameWorld) Error = FText::FromString(TEXT("未找到指定的 PIE 窗口或本地玩家，请检查高级调试选项。"));
		return nullptr;
	}
}
#endif

void URogueUpgradeTreeDefinition::RefreshDebugIceBricks()
{
#if WITH_EDITOR
	DebugCurrentIceBricks = -1;
	FText Error;
	bool bHasGameWorld = false;
	if (URogueProgressionSubsystem* Live = FindDebugProgression(DebugPIEInstance, DebugLocalPlayerIndex, bHasGameWorld, Error))
	{
		if (!Live->IsSaveWritable()) { DebugIceBrickStatus = Live->GetSaveError(); return; }
		DebugCurrentIceBricks = Live->GetIceBricks();
		DebugIceBrickStatus = FText::FromString(TEXT("已读取运行中的本地玩家：") + Live->GetSaveSlotName());
		return;
	}
	if (bHasGameWorld) { DebugIceBrickStatus = Error; return; }
	FString Slot; int32 UserIndex = 0; bool bWritable = false;
	if (!URogueProgressionSubsystem::ResolveSaveLocation(DebugLocalPlayerIndex, Slot, UserIndex, Error))
	{
		DebugIceBrickStatus = Error; return;
	}
	URogueProgressionSaveGame* Save = URogueProgressionSubsystem::LoadProfileFromSlot(Slot, UserIndex, this, bWritable, Error);
	if (!bWritable || !Save) { DebugIceBrickStatus = Error; return; }
	DebugCurrentIceBricks = Save->IceBricks;
	DebugIceBrickStatus = FText::FromString(TEXT("已读取本地存档：") + Slot);
#endif
}

void URogueUpgradeTreeDefinition::ApplyDebugIceBricks()
{
#if WITH_EDITOR
	FText Error;
	if (DebugIceBricks < 0) { DebugIceBrickStatus = FText::FromString(TEXT("冰砖数量不能为负数。")); return; }
	bool bHasGameWorld = false;
	bool bSuccess = false;
	FString Slot; int32 UserIndex = 0;
	if (URogueProgressionSubsystem* Live = FindDebugProgression(DebugPIEInstance, DebugLocalPlayerIndex, bHasGameWorld, Error))
	{
		Slot = Live->GetSaveSlotName();
		bSuccess = Live->SetIceBricksForDebug(DebugIceBricks, Error);
		if (bSuccess) DebugCurrentIceBricks = Live->GetIceBricks();
	}
	else if (!bHasGameWorld && URogueProgressionSubsystem::ResolveSaveLocation(DebugLocalPlayerIndex, Slot, UserIndex, Error))
	{
		bSuccess = URogueProgressionSubsystem::SetIceBricksInSlotForDebug(Slot, UserIndex, GetDefaultDefinition(), DebugIceBricks, Error);
		if (bSuccess) DebugCurrentIceBricks = DebugIceBricks;
	}
	DebugIceBrickStatus = bSuccess
		? FText::FromString(FString::Printf(TEXT("已将 %s 的冰砖设置为 %d，原解锁进度和奖励记录已保留。"), *Slot, DebugCurrentIceBricks))
		: Error;
#endif
}

void URogueUpgradeTreeDefinition::ResetDebugProgression()
{
#if WITH_EDITOR
	FText Error;
	bool bHasGameWorld = false;
	bool bSuccess = false;
	FString Slot; int32 UserIndex = 0;
	if (URogueProgressionSubsystem* Live = FindDebugProgression(DebugPIEInstance, DebugLocalPlayerIndex, bHasGameWorld, Error))
	{
		Slot = Live->GetSaveSlotName();
		bSuccess = Live->ResetProgressionForDebug(Error);
		if (bSuccess) DebugCurrentIceBricks = Live->GetIceBricks();
	}
	else if (!bHasGameWorld && URogueProgressionSubsystem::ResolveSaveLocation(DebugLocalPlayerIndex, Slot, UserIndex, Error))
	{
		bSuccess = URogueProgressionSubsystem::ResetProgressionInSlotForDebug(Slot, UserIndex, GetDefaultDefinition(), Error);
		if (bSuccess) RefreshDebugIceBricks();
	}
	DebugIceBrickStatus = bSuccess
		? FText::FromString(FString::Printf(TEXT("已重置 %s 的解锁进度：仅保留配置中的初始免费节点，冰砖 %d 及 Boss 奖励记录已保留。永久属性和联机卡池从下一局生效。"), *Slot, DebugCurrentIceBricks))
		: Error;
#endif
}
