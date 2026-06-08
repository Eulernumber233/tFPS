#include "FPSItemDef.h"
#include "FPSCharacter.h"

// ---------------------------------------------------------------------------
// UFPSItemDef（基类）
// ---------------------------------------------------------------------------

bool UFPSItemDef::ServerCanUse(AFPSCharacter* User, const FInventoryEntry& Entry) const
{
	return User != nullptr && Entry.IsValidEntry();
}

void UFPSItemDef::ServerUseItem(AFPSCharacter* User, FInventoryEntry& Entry) const
{
	// 基类无效果，子类覆盖。
}

// ---------------------------------------------------------------------------
// UFPSHealItemDef（血包，带耐久）
// ---------------------------------------------------------------------------

UFPSHealItemDef::UFPSHealItemDef()
{
	bUsesDurability = true;   // 血包每个独立占一格，治疗扣耐久
	MaxStack = 1;
	DefaultValue = 64;        // 默认满耐久 64（蓝图数据资产里可改）
}

bool UFPSHealItemDef::ServerCanUse(AFPSCharacter* User, const FInventoryEntry& Entry) const
{
	if (!Super::ServerCanUse(User, Entry))
		return false;

	// 耐久耗尽 / 已满血 → 不可用（避免浪费）
	if (Entry.Durability <= 0)
		return false;

	return User->GetHealth() < User->GetMaxHealth();
}

void UFPSHealItemDef::ServerUseItem(AFPSCharacter* User, FInventoryEntry& Entry) const
{
	// 实际回血 = min(缺的血, 剩余耐久)。回多少血扣多少耐久。
	const float Missing = User->GetMaxHealth() - User->GetHealth();
	const int32 HealAmount = FMath::Min((int32)FMath::CeilToInt(Missing), Entry.Durability);
	if (HealAmount <= 0)
		return;

	User->ServerApplyHeal((float)HealAmount);   // 服务端权威加血（复制到所有端）
	Entry.Durability -= HealAmount;             // 扣耐久（背包组件据此决定是否删格）
}
