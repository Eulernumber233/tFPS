#include "FPSInventoryComponent.h"
#include "FPSCharacter.h"
#include "FPSPickup.h"
#include "FPSItemDef.h"
#include "Net/UnrealNetwork.h"

UFPSInventoryComponent::UFPSInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);   // 组件参与复制（Items 才会同步到客户端）
}

void UFPSInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// 背包只给本人看：COND_OwnerOnly 只复制给拥有该角色的客户端。
	DOREPLIFETIME_CONDITION(UFPSInventoryComponent, Items, COND_OwnerOnly);
}

AFPSCharacter* UFPSInventoryComponent::GetOwnerCharacter() const
{
	return Cast<AFPSCharacter>(GetOwner());
}

bool UFPSInventoryComponent::ServerAddItem(UFPSItemDef* Def)
{
	if (!Def)
		return false;

	// 按 DefaultValue 造一个全新道具的初始格，复用 ServerAddEntry 的堆叠/放置逻辑。
	FInventoryEntry NewEntry;
	NewEntry.ItemDef = Def;
	if (Def->bUsesDurability)
	{
		NewEntry.Count = 1;
		NewEntry.Durability = Def->DefaultValue;   // 血包初始耐久
	}
	else
	{
		NewEntry.Count = FMath::Min(Def->DefaultValue, Def->MaxStack);
		NewEntry.Durability = 0;
	}
	return ServerAddEntry(NewEntry);
}

bool UFPSInventoryComponent::ServerAddEntry(const FInventoryEntry& InEntry)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !InEntry.IsValidEntry())
		return false;

	UFPSItemDef* Def = InEntry.ItemDef;

	// 可堆叠道具：先尝试并入现有同类未满格子。
	if (!Def->bUsesDurability)
	{
		for (FInventoryEntry& Entry : Items)
		{
			if (Entry.ItemDef == Def && Entry.Count < Def->MaxStack)
			{
				const int32 Space = Def->MaxStack - Entry.Count;
				const int32 Added = FMath::Min(Space, InEntry.Count);
				Entry.Count += Added;
				OnRep_Items();   // 服务端本机手动刷（host 不走复制）
				return true;
			}
		}
	}

	// 无法并入（或带耐久道具）→ 新建一格。放置策略决定有没有空间放下。
	int32 SlotX = -1, SlotY = -1;
	if (!FindSlotFor(Def, SlotX, SlotY))
		return false;   // 背包放不下（计数模式=满了；占格模式=无连续空间）

	FInventoryEntry NewEntry = InEntry;   // 保留传入的耐久/数量（丢弃捡回时不重置）
	NewEntry.GridX = SlotX;               // MVP 计数模式下为 -1；占格模式下为装箱算出的坐标
	NewEntry.GridY = SlotY;
	Items.Add(NewEntry);

	OnRep_Items();   // 服务端本机手动刷
	return true;
}

bool UFPSInventoryComponent::FindSlotFor(const UFPSItemDef* Def, int32& OutX, int32& OutY) const
{
	// ---- 当前实现：计数模式 ----
	// 只判断总格数没满。坐标返回 (-1,-1) 表示"未定位"，UI 按数组顺序铺格。
	OutX = -1;
	OutY = -1;
	return Items.Num() < GetMaxSlots();

	// ---- 升级到塔科夫式占格背包时，把上面整段换成装箱算法： ----
	// 建一个 GridColumns×GridRows 的占用位图，标记已有道具占的格子，
	// 然后从左上到右下扫描，找一个能放下 Def->GridWidth×GridHeight 的空矩形，
	// 找到则把左上角写进 OutX/OutY 返回 true，扫完没有返回 false。
	// 其余 ServerAddItem / 复制 / UI 数据流都不用改。
}

FItemUseResult UFPSInventoryComponent::BeginItemUse(int32 Index)
{
	FItemUseResult Result;

	if (!GetOwner() || !GetOwner()->HasAuthority())
		return Result;
	if (!Items.IsValidIndex(Index))
		return Result;

	AFPSCharacter* Character = GetOwnerCharacter();
	if (!Character || Character->IsDead())
		return Result;

	FInventoryEntry& Entry = Items[Index];
	UFPSItemDef* Def = Entry.ItemDef;
	if (!Def || !Def->ServerCanUse(Character, Entry))
		return Result;

	// 道具自己改 Entry（扣耐久 / 扣数量）。返回消耗量。
	const int32 Consumed = Def->ServerUseItem(Character, Entry);
	if (Consumed <= 0 && Def->GetItemUseType() != EFPSItemUseType::Instant)
		return Result;   // 异步类道具没消耗任何东西 → 使用失败

	// 组装返回信息
	Result.UseType = Def->GetItemUseType();
	Result.ConsumedAmount = Consumed;
	Result.UseTime = Def->UseTime;

	// 按子类提取专用配置（Cast 失败则留默认值 0，无影响）
	if (const UFPSChanneledHealItemDef* CHDef = Cast<UFPSChanneledHealItemDef>(Def))
	{
		Result.HealPerTick = CHDef->HealPerTick;
		Result.HealInterval = CHDef->HealInterval;
	}
	else if (const UFPSHoTItemDef* HoTDef = Cast<UFPSHoTItemDef>(Def))
	{
		Result.HoTBaseDuration = HoTDef->HoTBaseDuration;
	}

	// 检查是否耗尽 → 删格
	const bool bExhausted = Def->bUsesDurability ? (Entry.Durability <= 0) : (Entry.Count <= 0);
	if (bExhausted)
		Items.RemoveAt(Index);

	OnRep_Items();   // 服务端本机手动刷
	return Result;
}

void UFPSInventoryComponent::ServerUseItem(int32 Index)
{
	// 即时类道具：BeginItemUse 同步完成全部效果，忽略返回值即可。
	// 异步类道具的调用方应改用 BeginItemUse 拿 FItemUseResult。
	BeginItemUse(Index);
}

void UFPSInventoryComponent::ServerDropItem(int32 Index)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
		return;
	if (!Items.IsValidIndex(Index))
		return;

	AFPSCharacter* Character = GetOwnerCharacter();
	if (!Character)
		return;

	const FInventoryEntry Entry = Items[Index];   // 拷贝一份（下面要 RemoveAt）
	UFPSItemDef* Def = Entry.ItemDef;
	if (!Def || !Def->DropPickupClass)
		return;   // 没配可丢弃的 Pickup 类 → 不可丢弃

	// 在玩家脚下前方一点生成 Pickup（避免和身体重叠立刻又被自己 overlap 捡回）。
	const FVector SpawnLoc = Character->GetActorLocation()
		+ Character->GetActorForwardVector() * 80.0f
		+ FVector(0, 0, -40.0f);
	const FRotator SpawnRot = FRotator::ZeroRotator;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AFPSPickup* Dropped = GetWorld()->SpawnActor<AFPSPickup>(Def->DropPickupClass, SpawnLoc, SpawnRot, Params);
	if (!Dropped)
		return;

	// 带耐久道具：把剩余耐久带到地上的 Pickup（这样捡回来还是这么多耐久，不是满的）。
	// 注：需要 AFPSPickup 暴露设置初始耐久的接口——见 AFPSPickup::SetDroppedState。
	Dropped->SetDroppedState(Entry);

	// 从背包移除该格。
	Items.RemoveAt(Index);
	OnRep_Items();   // 服务端本机手动刷
}

void UFPSInventoryComponent::ServerRemoveItemAt(int32 Index)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
		return;
	if (!Items.IsValidIndex(Index))
		return;

	Items.RemoveAt(Index);
	OnRep_Items();
}

void UFPSInventoryComponent::ServerClear()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
		return;

	Items.Empty();
	OnRep_Items();
}

int32 UFPSInventoryComponent::ConsumeAmmo(FName Caliber, int32 Amount, UFPSAmmoItemDef*& OutLastDef,
	TArray<FWeaponAmmoEntry>* OutBreakdown)
{
	OutLastDef = nullptr;
	if (OutBreakdown)
		OutBreakdown->Reset();

	if (!GetOwner() || !GetOwner()->HasAuthority() || Caliber.IsNone() || Amount <= 0)
		return 0;

	int32 Consumed = 0;

	// 从数组末尾向前遍历（删格时不影响前面索引）
	for (int32 i = Items.Num() - 1; i >= 0; --i)
	{
		if (Consumed >= Amount)
			break;

		FInventoryEntry& Entry = Items[i];
		UFPSAmmoItemDef* AmmoDef = Cast<UFPSAmmoItemDef>(Entry.ItemDef);
		if (!AmmoDef || AmmoDef->Caliber != Caliber)
			continue;

		const int32 Take = FMath::Min(Amount - Consumed, Entry.Count);
		if (Take <= 0)
			continue;

		Entry.Count -= Take;
		Consumed += Take;
		OutLastDef = AmmoDef;

		// 记录每次取弹明细（调用方用此构建弹药链）
		if (OutBreakdown)
			OutBreakdown->Add({AmmoDef, Take});

		if (Entry.Count <= 0)
			Items.RemoveAt(i);
	}

	return Consumed;
}

void UFPSInventoryComponent::MarkItemsDirty()
{
	OnRep_Items();
}

void UFPSInventoryComponent::OnRep_Items()
{
	// 服务端本机直接调（手动），客户端收到 Items 复制后自动调 → 统一广播给 UI。
	OnInventoryChanged.Broadcast();
}
