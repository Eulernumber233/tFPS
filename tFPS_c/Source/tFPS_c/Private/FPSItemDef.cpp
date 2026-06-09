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
	bUsesDurability = true;   // 血包每个独立占一格，治疗扣耐久
	MaxStack = 1;
	DefaultValue = 64;        // 默认满耐久 64（蓝图数据资产里可改）
	UseTime = 0.0f;           // 即时生效
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
// UFPSChanneledHealItemDef（Type 2 — 引导治疗，三角洲大血包风格）
// ---------------------------------------------------------------------------

UFPSChanneledHealItemDef::UFPSChanneledHealItemDef()
{
	bUsesDurability = true;   // 耐久 = 总可治疗量
	MaxStack = 1;
	DefaultValue = 100;       // 默认满耐久 100（蓝图可改）
	UseTime = 2.0f;           // 2 秒前摇
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

	// 耐久一次性扣除，后续由 Character 状态机逐跳回血
	Entry.Durability -= Committed;
	return Committed;
}

// ---------------------------------------------------------------------------
// UFPSHoTItemDef（Type 3 — 持续回血 Buff，PUBG 止疼药风格）
// ---------------------------------------------------------------------------

UFPSHoTItemDef::UFPSHoTItemDef()
{
	bUsesDurability = false;   // 可堆叠的一次性消耗品（Count 记瓶数）
	MaxStack = 4;              // 一格最多叠 4 瓶
	DefaultValue = 1;          // 捡一次给 1 瓶
	UseTime = 3.0f;            // 3 秒使用时间（Character 的 HoTHealPerSecond 和 HoTMaxBuffDuration 决定回血速度和上限）
}

bool UFPSHoTItemDef::ServerCanUse(AFPSCharacter* User, const FInventoryEntry& Entry) const
{
	if (!Super::ServerCanUse(User, Entry))
		return false;

	// 只要有瓶就能喝 — 满血也能喝（预刷 buff 应对即将到来的战斗）
	return Entry.Count > 0;
}

int32 UFPSHoTItemDef::ServerUseItem(AFPSCharacter* User, FInventoryEntry& Entry) const
{
	if (Entry.Count <= 0)
		return 0;

	Entry.Count -= 1;
	return 1;   // 消耗 1 瓶
}

// ---------------------------------------------------------------------------
// UFPSValuableItemDef（Type 4 — 纯价值物品，不可使用）
// ---------------------------------------------------------------------------

UFPSValuableItemDef::UFPSValuableItemDef()
{
	bUsesDurability = false;
	MaxStack = 1;
	DefaultValue = 1;
	Value = 1000;   // 默认价值（金条/钻石在蓝图数据资产里填具体值）
}

// ---------------------------------------------------------------------------
// UFPSAmmoItemDef（Type 5 — 子弹，可堆叠消耗品）
// ---------------------------------------------------------------------------

UFPSAmmoItemDef::UFPSAmmoItemDef()
{
	bUsesDurability = false;
	MaxStack = 120;       // 单格最多叠 120 发
	DefaultValue = 30;    // 捡一次给 30 发
	Tier = 1;
	Value = 1;            // 子弹基础价值很低
}
