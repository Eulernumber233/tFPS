#include "FPSItemDef.h"
#include "FPSCharacter.h"

// ---------------------------------------------------------------------------
// UFPSItemDef（基类）
// ---------------------------------------------------------------------------

bool UFPSItemDef::ServerCanUse(AFPSCharacter* User, const FInventoryEntry& Entry) const
{
	return User != nullptr && Entry.IsValidEntry();
}

int32 UFPSItemDef::ServerUseItem(AFPSCharacter* User, FInventoryEntry& Entry) const
{
	return 0;   // 基类无效果，子类覆盖。
}

int32 UFPSItemDef::GetCurrentValue(const FInventoryEntry& Entry) const
{
	if (!Entry.IsValidEntry())
		return 0;

	if (bUsesDurability)
	{
		if (DefaultValue > 0)
			return FMath::RoundToInt((float)Value * (float)Entry.Durability / (float)DefaultValue);
		return 0;
	}
	return Value * Entry.Count;
}

// ---------------------------------------------------------------------------
// UFPSHealItemDef（Type 1 — 即时血包，带耐久）
// ---------------------------------------------------------------------------

UFPSHealItemDef::UFPSHealItemDef()
{
	bUsesDurability = true;
	MaxStack = 1;
	DefaultValue = 64;
	UseTime = 0.0f;
	Description = NSLOCTEXT("UFPSItemDef", "HealDesc", "立即恢复一定生命值，消耗等量耐久。");
}

bool UFPSHealItemDef::ServerCanUse(AFPSCharacter* User, const FInventoryEntry& Entry) const
{
	if (!Super::ServerCanUse(User, Entry))
		return false;

	if (Entry.Durability <= 0)
		return false;

	return User->GetHealth() < User->GetMaxHealth();
}

int32 UFPSHealItemDef::ServerUseItem(AFPSCharacter* User, FInventoryEntry& Entry) const
{
	const float Missing = User->GetMaxHealth() - User->GetHealth();
	const int32 HealAmount = FMath::Min((int32)FMath::CeilToInt(Missing), Entry.Durability);
	if (HealAmount <= 0)
		return 0;

	User->ServerApplyHeal((float)HealAmount);
	Entry.Durability -= HealAmount;
	return HealAmount;
}

// ---------------------------------------------------------------------------
// UFPSChanneledHealItemDef（Type 2 — 引导治疗）
// ---------------------------------------------------------------------------

UFPSChanneledHealItemDef::UFPSChanneledHealItemDef()
{
	bUsesDurability = true;
	MaxStack = 1;
	DefaultValue = 100;
	UseTime = 2.0f;
	Description = NSLOCTEXT("UFPSItemDef", "ChanneledHealDesc", "前摇后逐跳恢复生命值。F键可中途取消。");
}

bool UFPSChanneledHealItemDef::ServerCanUse(AFPSCharacter* User, const FInventoryEntry& Entry) const
{
	if (!Super::ServerCanUse(User, Entry))
		return false;

	if (Entry.Durability <= 0)
		return false;

	return User->GetHealth() < User->GetMaxHealth();
}

int32 UFPSChanneledHealItemDef::ServerUseItem(AFPSCharacter* User, FInventoryEntry& Entry) const
{
	const float Missing = User->GetMaxHealth() - User->GetHealth();
	const int32 Committed = FMath::Min((int32)FMath::CeilToInt(Missing), Entry.Durability);
	if (Committed <= 0)
		return 0;

	Entry.Durability -= Committed;
	return Committed;
}

// ---------------------------------------------------------------------------
// UFPSHoTItemDef（Type 3 — 持续回血 Buff）
// ---------------------------------------------------------------------------

UFPSHoTItemDef::UFPSHoTItemDef()
{
	bUsesDurability = false;
	MaxStack = 4;
	DefaultValue = 1;
	UseTime = 3.0f;
	Description = NSLOCTEXT("UFPSItemDef", "HoTDesc", "使用后激活持续回血效果，可叠加延长持续时间。");
}

bool UFPSHoTItemDef::ServerCanUse(AFPSCharacter* User, const FInventoryEntry& Entry) const
{
	if (!Super::ServerCanUse(User, Entry))
		return false;

	return Entry.Count > 0;
}

int32 UFPSHoTItemDef::ServerUseItem(AFPSCharacter* User, FInventoryEntry& Entry) const
{
	if (Entry.Count <= 0)
		return 0;

	Entry.Count -= 1;
	return 1;
}

// ---------------------------------------------------------------------------
// UFPSValuableItemDef（Type 4 — 纯价值物品，不可使用）
// ---------------------------------------------------------------------------

UFPSValuableItemDef::UFPSValuableItemDef()
{
	bUsesDurability = false;
	MaxStack = 1;
	DefaultValue = 1;
	Value = 1000;
	Description = NSLOCTEXT("UFPSItemDef", "ValuableDesc", "贵重物品，不可使用。带到提交点可换取带出积分。");
}

// ---------------------------------------------------------------------------
// UFPSAmmoItemDef（Type 5 — 子弹，可堆叠消耗品）
// ---------------------------------------------------------------------------

UFPSAmmoItemDef::UFPSAmmoItemDef()
{
	bUsesDurability = false;
	MaxStack = 120;
	DefaultValue = 30;
	Tier = 1;
	Value = 1;
	Description = NSLOCTEXT("UFPSItemDef", "AmmoDesc", "弹药，不同口径/等级不可混用。");
}
