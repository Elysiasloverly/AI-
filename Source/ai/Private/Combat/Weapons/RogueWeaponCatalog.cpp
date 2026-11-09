#include "Combat/Weapons/RogueWeaponCatalog.h"
#include "Combat/Weapons/RogueWeaponBase.h"
#include "Combat/Weapons/RogueWeapon_Projectile.h"
#include "Combat/Weapons/RogueWeapon_Scythe.h"
#include "Combat/Weapons/RogueWeapon_Rocket.h"
#include "Combat/Weapons/RogueWeapon_Laser.h"
#include "Combat/Weapons/RogueWeapon_HellTower.h"
#include "Combat/Weapons/RogueWeapon_Mortar.h"
#include "Combat/Weapons/RogueWeapon_Talisman.h"
#include "Combat/Weapons/RogueWeapon_FrostBlade.h"
#include "Player/RogueCharacter.h"
#include "Progression/Upgrades/RogueUpgradeRuleAsset.h"

namespace
{
// 添加武器时在此登记稳定身份；枚举数值和已经发布的行名不要更改。
const FRogueWeaponDescriptor Weapons[] = {
	{TEXT("Projectile"), ERogueUpgradeType::ProjectileCount, ERogueWeaponUpgradeSource::Projectile,
	 &ARogueWeapon_Projectile::StaticClass},
	{TEXT("Scythe"), ERogueUpgradeType::ScytheCount, ERogueWeaponUpgradeSource::Scythe,
	 &ARogueWeapon_Scythe::StaticClass},
	{TEXT("Rocket"), ERogueUpgradeType::RocketCount, ERogueWeaponUpgradeSource::Rocket,
	 &ARogueWeapon_Rocket::StaticClass},
	{TEXT("Laser"), ERogueUpgradeType::LaserCount, ERogueWeaponUpgradeSource::Laser,
	 &ARogueWeapon_Laser::StaticClass},
	{TEXT("HellTower"), ERogueUpgradeType::HellTowerCount, ERogueWeaponUpgradeSource::HellTower,
	 &ARogueWeapon_HellTower::StaticClass},
	{TEXT("Mortar"), ERogueUpgradeType::MortarCount, ERogueWeaponUpgradeSource::Mortar,
	 &ARogueWeapon_Mortar::StaticClass},
	{TEXT("Talisman"), ERogueUpgradeType::TalismanCount, ERogueWeaponUpgradeSource::Talisman,
	 &ARogueWeapon_Talisman::StaticClass},
	{TEXT("FrostBlade"), ERogueUpgradeType::FrostBladeCount, ERogueWeaponUpgradeSource::FrostBlade,
	 &ARogueWeapon_FrostBlade::StaticClass}};
struct FExclusive
{
	ERogueUpgradeType Card, Quantity;
};
constexpr FExclusive ExclusiveCards[] = {
	{ERogueUpgradeType::ProjectileBurst, ERogueUpgradeType::ProjectileCount},
	{ERogueUpgradeType::ScytheSize, ERogueUpgradeType::ScytheCount},
	{ERogueUpgradeType::ScytheLifesteal, ERogueUpgradeType::ScytheCount},
	{ERogueUpgradeType::RocketBlastRadius, ERogueUpgradeType::RocketCount},
	{ERogueUpgradeType::LaserRefraction, ERogueUpgradeType::LaserCount},
	{ERogueUpgradeType::HellTowerDamageRamp, ERogueUpgradeType::HellTowerCount},
	{ERogueUpgradeType::MortarBlastRadius, ERogueUpgradeType::MortarCount},
	{ERogueUpgradeType::TalismanPierce, ERogueUpgradeType::TalismanCount},
	{ERogueUpgradeType::TalismanRepeat, ERogueUpgradeType::TalismanCount},
	{ERogueUpgradeType::FrostBladeSize, ERogueUpgradeType::FrostBladeCount},
	{ERogueUpgradeType::FrostBladeWave, ERogueUpgradeType::FrostBladeCount},
	{ERogueUpgradeType::FrostBladeWaveCount, ERogueUpgradeType::FrostBladeCount},
	{ERogueUpgradeType::FrostBladeWaveSize, ERogueUpgradeType::FrostBladeCount}};
} // namespace

TConstArrayView<FRogueWeaponDescriptor> RogueWeaponCatalog::GetAll()
{
	return Weapons;
}
const FRogueWeaponDescriptor *RogueWeaponCatalog::FindByQuantity(ERogueUpgradeType Type)
{
	for (const auto &W : Weapons)
		if (W.QuantityType == Type)
			return &W;
	return nullptr;
}
const FRogueWeaponDescriptor *RogueWeaponCatalog::FindBySource(ERogueWeaponUpgradeSource Source)
{
	for (const auto &W : Weapons)
		if (W.UpgradeSource == Source)
			return &W;
	return nullptr;
}
bool RogueWeaponCatalog::IsWeaponQuantity(ERogueUpgradeType Type)
{
	return FindByQuantity(Type) != nullptr;
}

bool RogueWeaponCatalog::TryGetQuantityForUpgrade(ERogueUpgradeType Upgrade, ERogueUpgradeType &OutQuantity)
{
	if (IsWeaponQuantity(Upgrade))
	{
		OutQuantity = Upgrade;
		return true;
	}
	for (const auto &Entry : ExclusiveCards)
		if (Entry.Card == Upgrade)
		{
			OutQuantity = Entry.Quantity;
			return true;
		}
	return false;
}

int32 RogueWeaponCatalog::GetOwnedCount(const ARogueCharacter *Character, ERogueUpgradeType Upgrade)
{
	ERogueUpgradeType Quantity;
	if (!Character || !TryGetQuantityForUpgrade(Upgrade, Quantity))
		return 0;
	for (const ARogueWeaponBase *Weapon : Character->GetWeapons())
		if (IsValid(Weapon) && Weapon->GetCountUpgradeType() == Quantity)
			return Weapon->GetEffectiveCount();
	// 寒弹是角色的基础武器；保留未完成武器初始化时原访问器的语义。
	return Quantity == ERogueUpgradeType::ProjectileCount ? 1 : 0;
}

TConstArrayView<FRogueWeaponUpgradeRuleRow> RogueWeaponCatalog::GetDefaultUpgradeRules()
{
	// 默认规则集中定义；实际读取数据资产时仍保留用户设置的规则及顺序。
	static const FRogueWeaponUpgradeRuleRow Rules[] = {
		{ERogueWeaponUpgradeSource::Talisman, ERogueUpgradeType::TalismanCount},
		{ERogueWeaponUpgradeSource::Talisman, ERogueUpgradeType::TalismanPierce},
		{ERogueWeaponUpgradeSource::Talisman, ERogueUpgradeType::TalismanRepeat},
		{ERogueWeaponUpgradeSource::FrostBlade, ERogueUpgradeType::FrostBladeSize},
		{ERogueWeaponUpgradeSource::FrostBlade, ERogueUpgradeType::FrostBladeWave},
		{ERogueWeaponUpgradeSource::FrostBlade, ERogueUpgradeType::FrostBladeWaveCount},
		{ERogueWeaponUpgradeSource::FrostBlade, ERogueUpgradeType::FrostBladeWaveSize},
		{ERogueWeaponUpgradeSource::Projectile, ERogueUpgradeType::ProjectileBurst},
		{ERogueWeaponUpgradeSource::Rocket, ERogueUpgradeType::RocketBlastRadius},
		{ERogueWeaponUpgradeSource::HellTower, ERogueUpgradeType::HellTowerDamageRamp},
		{ERogueWeaponUpgradeSource::Scythe, ERogueUpgradeType::ScytheCount},
		{ERogueWeaponUpgradeSource::Scythe, ERogueUpgradeType::ScytheLifesteal},
		{ERogueWeaponUpgradeSource::Rocket, ERogueUpgradeType::RocketCount},
		{ERogueWeaponUpgradeSource::Laser, ERogueUpgradeType::LaserCount},
		{ERogueWeaponUpgradeSource::HellTower, ERogueUpgradeType::HellTowerCount},
		{ERogueWeaponUpgradeSource::Mortar, ERogueUpgradeType::MortarCount}};
	return Rules;
}

TSubclassOf<ARogueWeaponBase> RogueWeaponCatalog::ResolveClass(FName RowName, const FRogueWeaponTableRow &Row)
{
	if (Row.WeaponClass)
		return Row.WeaponClass;
	for (const auto &W : Weapons)
		if (W.RowName == RowName)
			return W.NativeClass();
	if (const auto *W = FindByQuantity(Row.CountUpgradeType))
		return W->NativeClass();
	return nullptr;
}
