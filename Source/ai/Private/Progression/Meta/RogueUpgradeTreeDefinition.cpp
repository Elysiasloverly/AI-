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
			|| Type == ERogueUpgradeType::HellTowerCount || Type == ERogueUpgradeType::MortarCount || Type == ERogueUpgradeType::TalismanCount || Type==ERogueUpgradeType::FrostBladeCount;
	}

	bool IsDedicatedType(ERogueUpgradeType Type)
	{
		return Type==ERogueUpgradeType::FrostBladeSize || Type==ERogueUpgradeType::FrostBladeWave || Type==ERogueUpgradeType::FrostBladeWaveCount || Type==ERogueUpgradeType::FrostBladeWaveSize || Type == ERogueUpgradeType::TalismanPierce || Type == ERogueUpgradeType::TalismanRepeat || Type == ERogueUpgradeType::ProjectileBurst || Type == ERogueUpgradeType::RocketBlastRadius
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

	Add(TEXT("Weapon.Projectile"), TEXT("冰符·寒弹"), TEXT("自动瞄准近处敌人，发射冰蓝灵弹。\n初始自带；数量卡增加每轮弹量，冰符连诵增加连发轮数。"), ERogueMetaNodeKind::Weapon,
		ERogueUpgradeType::ProjectileCount, 0, 780).bInitiallyUnlocked = true;
	Add(TEXT("Card.ProjectileCount"), TEXT("寒弹增殖"), TEXT("初始开放；本局每选一次，每轮灵弹增加 1 枚。连发轮数由冰符连诵提升。"), ERogueMetaNodeKind::UpgradeCard,
		ERogueUpgradeType::ProjectileCount, 0, 520, TEXT("Weapon.Projectile")).bInitiallyUnlocked = true;

	struct FWeapon { const TCHAR* Id; const TCHAR* Name; ERogueUpgradeType Type; const TCHAR* Description; const TCHAR* CardDescription; const TCHAR* CardTitle; };
	const FWeapon Weapons[] = {
		{ TEXT("Scythe"), TEXT("银刃"), ERogueUpgradeType::ScytheCount, TEXT("银色刀刃环绕自身旋转，切割靠近的敌人。\n解锁后加入获取卡池，本局抽到才装备。"), TEXT("开放银刃数量升级；本局持有银刃后，每选一次增加 1 把环绕刀刃。"), TEXT("银刃环阵") },
		{ TEXT("Rocket"), TEXT("冰焰·陨星"), ERogueUpgradeType::RocketCount, TEXT("从高空召唤冰焰球斜向砸落并爆炸；地面留下蓝焰，减速敌人并持续造成少量伤害。\n解锁后加入获取卡池，本局抽到才装备。"), TEXT("开放陨星数量升级；本局持有冰焰·陨星后，每选一次每轮额外召唤 1 颗冰焰球。"), TEXT("陨星齐落") },
		{ TEXT("Laser"), TEXT("星符·流光"), ERogueUpgradeType::LaserCount, TEXT("向敌人发射蓝白激光；星尘折射可追加跳转目标。\n解锁后加入获取卡池，本局抽到才装备。"), TEXT("开放流光数量升级；本局持有星符·流光后，每选一次每轮增加 1 束激光。"), TEXT("流光齐射") },
		{ TEXT("HellTower"), TEXT("焰符·灼光"), ERogueUpgradeType::HellTowerCount, TEXT("金橙色火焰光束持续锁定敌人，连续命中同一目标时每跳伤害递增。\n解锁后加入获取卡池，本局抽到才装备。"), TEXT("开放灼光数量升级；本局持有焰符·灼光后，每选一次可同时锁定的不同敌人增加 1 名。"), TEXT("灼光分流") },
		{ TEXT("Mortar"), TEXT("冰符·霜玉"), ERogueUpgradeType::MortarCount, TEXT("沿抛物线投出冰蓝球体，轰击敌人所在区域，落地爆炸。\n解锁后加入获取卡池，本局抽到才装备。"), TEXT("开放霜玉数量升级；本局持有冰符·霜玉后，每选一次每轮增加 1 枚抛射冰球。"), TEXT("霜玉连投") }
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Weapons); ++Index)
	{
		const FWeapon& Weapon = Weapons[Index];
		const FString WeaponId = FString::Printf(TEXT("Weapon.%s"), Weapon.Id);
		const FString CardId = FString::Printf(TEXT("Card.%sCount"), Weapon.Id);
		const FString Description = Weapon.Description;
		const FString CardTitle = Weapon.CardTitle;
		const float X = (Index + 1) * 330.0f;
		Add(*WeaponId, Weapon.Name, *Description, ERogueMetaNodeKind::Weapon, Weapon.Type, X, 780);
		Add(*CardId, *CardTitle, Weapon.CardDescription,
			ERogueMetaNodeKind::UpgradeCard, Weapon.Type, X, 520, *WeaponId);
	}
	Add(TEXT("Card.ScytheSize"), TEXT("银刃巨化"), TEXT("开放此专属卡；本局抽到后：\n银刃外观和伤害范围增加基础尺寸的 20%；环绕距离不变。"),
		ERogueMetaNodeKind::UpgradeCard, ERogueUpgradeType::ScytheSize, 330, 260, TEXT("Card.ScytheCount"));
	Add(TEXT("Card.ScytheLifesteal"), TEXT("红雾吸血"), TEXT("开放此专属卡；本局抽到后：\n银刃造成伤害时回复生命。首次吸血率 20%，再次选择 +5 个百分点，受吸血率上限限制。"),
		ERogueMetaNodeKind::UpgradeCard, ERogueUpgradeType::ScytheLifesteal, 330, 0, TEXT("Card.ScytheSize"));
	Add(TEXT("Card.LaserRefraction"), TEXT("星尘折射"), TEXT("开放此专属卡；本局抽到后：\n每束流光的折射次数 +1；命中后可跳向附近敌人，折射段按武器配置的倍率结算伤害。"),
		ERogueMetaNodeKind::UpgradeCard, ERogueUpgradeType::LaserRefraction, 990, 260, TEXT("Card.LaserCount"));
	Add(TEXT("Card.MortarBlastRadius"), TEXT("霜玉扩散"), TEXT("开放此专属卡；本局抽到后：\n霜玉爆炸半径 +100 厘米，扩大落点周围的伤害区域。"),
		ERogueMetaNodeKind::UpgradeCard, ERogueUpgradeType::MortarBlastRadius, 1650, 260, TEXT("Card.MortarCount"));

	FRogueUpgradeTreeNode& ProjectileBurst = Add(TEXT("Card.ProjectileBurst"), TEXT("冰符连诵"),
		TEXT("初始开放；本局每选一次，寒弹每轮攻击额外连射 1 次，每次发出完整弹幕。首次选取从单发变为 2 连发。"),
		ERogueMetaNodeKind::UpgradeCard, ERogueUpgradeType::ProjectileBurst, 0, 0, TEXT("Card.ProjectileCount"));
	ProjectileBurst.bInitiallyUnlocked = true;
	ProjectileBurst.Cost = 0;
	Add(TEXT("Card.RocketBlastRadius"), TEXT("冰焰扩散"), TEXT("开放此专属卡；本局抽到后：\n冰焰球爆炸半径 +90 厘米，同时增大球体、焰尾与地面蓝焰的影响范围。"),
		ERogueMetaNodeKind::UpgradeCard, ERogueUpgradeType::RocketBlastRadius, 0, 0, TEXT("Card.RocketCount"));
	Add(TEXT("Card.HellTowerDamageRamp"), TEXT("焰心递燃"), TEXT("开放此专属卡；本局抽到后：\n灼光每跳伤害递增值 +0.1；连续锁定同一目标越久，伤害增长越快。"),
		ERogueMetaNodeKind::UpgradeCard, ERogueUpgradeType::HellTowerDamageRamp, 0, 0, TEXT("Card.HellTowerCount"));

	Add(TEXT("Weapon.Talisman"),TEXT("灵符·归心"),TEXT("灵符环绕自身待命，自动飞出穿刺敌人，再返回身边。\n默认每次穿刺 1 名敌人 1 次；解锁后，本局抽到获取卡才装备。"),ERogueMetaNodeKind::Weapon,ERogueUpgradeType::TalismanCount,1980,780);
	Add(TEXT("Card.TalismanCount"),TEXT("归心符阵"),TEXT("开放数量卡；本局已持有灵符后，每选一次增加 1 张环绕灵符，分别自动出击。"),ERogueMetaNodeKind::UpgradeCard,ERogueUpgradeType::TalismanCount,1980,520,TEXT("Weapon.Talisman"));
	Add(TEXT("Card.TalismanPierce"),TEXT("灵符贯阵"),TEXT("开放此专属卡；本局每选一次，每张灵符单次出击可穿刺的不同敌人数 +1。"),ERogueMetaNodeKind::UpgradeCard,ERogueUpgradeType::TalismanPierce,1980,260,TEXT("Card.TalismanCount"));
	Add(TEXT("Card.TalismanRepeat"),TEXT("灵符重奏"),TEXT("开放此专属卡；本局每选一次，每张灵符对每名敌人的穿刺次数 +1，飞出后往返追刺再归位。"),ERogueMetaNodeKind::UpgradeCard,ERogueUpgradeType::TalismanRepeat,1980,0,TEXT("Card.TalismanPierce"));

    Add(TEXT("Weapon.FrostBlade"),TEXT("冰刀·霜月"),TEXT("永久解锁后每局自动装备一把，悬于身后，自动横斩附近敌群后归位。刀越大，扇形范围与伤害越高，基础攻击加成也生效。"),ERogueMetaNodeKind::Weapon,ERogueUpgradeType::FrostBladeCount,2310,780);
    Add(TEXT("Card.FrostBladeSize"),TEXT("霜刃延展"),TEXT("开放霜刃延展卡：按卡牌配置增加冰刀尺寸，横斩范围与伤害随尺寸提高，基础攻击加成也参与放大。"),ERogueMetaNodeKind::UpgradeCard,ERogueUpgradeType::FrostBladeSize,2310,520,TEXT("Weapon.FrostBlade"));
    Add(TEXT("Card.FrostBladeWave"),TEXT("霜月出鞘"),TEXT("开放此专属卡；冰刀横斩时向前放出一道低伤害的浅弯月剑气；穿透敌人，碰地图或30秒后消失。"),ERogueMetaNodeKind::UpgradeCard,ERogueUpgradeType::FrostBladeWave,2310,260,TEXT("Card.FrostBladeSize"));
    Add(TEXT("Card.FrostBladeWaveCount"),TEXT("霜月重叠"),TEXT("开放此专属卡；每次横斩额外发出1道剑气，向前呈小角度扇形散开。"),ERogueMetaNodeKind::UpgradeCard,ERogueUpgradeType::FrostBladeWaveCount,2310,0,TEXT("Card.FrostBladeWave"));
    Add(TEXT("Card.FrostBladeWaveSize"),TEXT("霜月盈空"),TEXT("开放此专属卡；剑气外观和命中范围增加基础尺寸的20%，不改变伤害与寿命。"),ERogueMetaNodeKind::UpgradeCard,ERogueUpgradeType::FrostBladeWaveSize,2310,-260,TEXT("Card.FrostBladeWaveCount"));

	FRogueUpgradeTreeNode& Armor = Add(TEXT("Armor.Unlock"), TEXT("解锁护甲"), TEXT("永久启用玩家配置中的基础护甲，并开放护甲容量、减伤相关卡及基础护甲升级。"),
		ERogueMetaNodeKind::ArmorUnlock, ERogueUpgradeType::ArmorCapacity, 660, 780);
	Armor.Cost = 3;
	struct FStat { const TCHAR* Id; const TCHAR* Title; const TCHAR* Description; ERogueMetaStat Stat; float Bonus; };
	const FStat Stats[] = {
		{ TEXT("AttackPower"), TEXT("基础攻击力"), TEXT("每级增加 3 点基础攻击力，各武器按自身倍率获得伤害加成。"), ERogueMetaStat::AttackPower, 3.0f },
		{ TEXT("MaxHealth"), TEXT("基础生命"), TEXT("每级增加 20 点开局最大生命。"), ERogueMetaStat::MaxHealth, 20.0f },
		{ TEXT("ArmorCapacity"), TEXT("基础护甲"), TEXT("先解锁护甲，每级再增加 10 点开局最大护甲。"), ERogueMetaStat::ArmorCapacity, 10.0f },
		{ TEXT("MoveSpeed"), TEXT("基础移速"), TEXT("每级增加 30 厘米/秒基础移动速度。"), ERogueMetaStat::MoveSpeed, 30.0f },
		{ TEXT("PickupRadius"), TEXT("吸取范围"), TEXT("每级增加 40 厘米经验球吸取半径。"), ERogueMetaStat::PickupRadius, 40.0f },
		{ TEXT("HealthRegen"), TEXT("生命恢复"), TEXT("每级增加每秒 0.2 点生命恢复。"), ERogueMetaStat::HealthRegen, 0.2f },
		{ TEXT("AttackSpeed"), TEXT("攻击间隔缩短"), TEXT("每级增加 0.03 秒攻击间隔缩短加成，按各武器系数及最短间隔生效。"), ERogueMetaStat::AttackSpeed, 0.03f },
		{ TEXT("ExperienceGain"), TEXT("经验获取"), TEXT("每级经验获取倍率增加 5 个百分点。"), ERogueMetaStat::ExperienceGain, 0.05f }
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
    if(Type==ERogueUpgradeType::FrostBladeCount)return false; // Automatically equipped from the owner's permanent unlock.
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

void URogueUpgradeTreeDefinition::UnlockAllDebugProgression()
{
#if WITH_EDITOR
	FText Error;
	FString Slot;
	int32 UserIndex = 0;
	bool bHasGameWorld = false;
	bool bSuccess = false;
	if (auto* Live = FindDebugProgression(DebugPIEInstance, DebugLocalPlayerIndex, bHasGameWorld, Error))
	{
		Slot = Live->GetSaveSlotName();
		bSuccess = Live->UnlockAllForDebug(Error);
		if (bSuccess) DebugCurrentIceBricks = Live->GetIceBricks();
	}
	else if (!bHasGameWorld && URogueProgressionSubsystem::ResolveSaveLocation(DebugLocalPlayerIndex, Slot, UserIndex, Error))
	{
		bSuccess = URogueProgressionSubsystem::UnlockAllInSlotForDebug(Slot, UserIndex, GetDefaultDefinition(), Error);
		if (bSuccess) RefreshDebugIceBricks();
	}
	DebugIceBrickStatus = bSuccess
		? FText::FromString(FString::Printf(TEXT("%s 的全部节点已升满，冰砖 %d。永久属性和联机卡池从下一局生效。"), *Slot, DebugCurrentIceBricks))
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
