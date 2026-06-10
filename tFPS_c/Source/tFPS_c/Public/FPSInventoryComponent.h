#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FPSItemDef.h"
#include "FPSInventoryComponent.generated.h"

class AFPSCharacter;
class UFPSAmmoItemDef;

/**
 * 道具使用的返回信息。背包组件扣完耐久/数量后，把异步配置打包传回给 Character 驱动状态机。
 * 即时类（Instant）道具用不到这些字段，调用方忽略即可。
 */
USTRUCT(BlueprintType)
struct FItemUseResult
{
	GENERATED_BODY()

	UPROPERTY()
	EFPSItemUseType UseType = EFPSItemUseType::None;

	/** 本次消耗的耐久/数量（Type 1=实际回血, Type 2=承诺回血量, Type 3=1） */
	UPROPERTY()
	int32 ConsumedAmount = 0;

	// ---- Type 2 (ChanneledHeal) 配置 ----
	float UseTime = 0.0f;
	int32 HealPerTick = 0;
	float HealInterval = 0.0f;

	// ---- Type 3 (HoTApply) 配置 ----
	float HoTBaseDuration = 0.0f;
};

/** 背包内容变化事件（每端本机，OnRep 驱动）。UI 订阅此事件刷新背包面板。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);

/**
 * 玩家背包组件（挂在 AFPSCharacter 上）。
 *
 * 服务端权威：所有增删/使用逻辑只在服务端跑，Items 数组复制到**所属客户端**
 * （COND_OwnerOnly —— 背包只给本人看，别人看不到你背包里有什么）。
 *
 * 混合存储模型：
 *   - 可堆叠道具（!bUsesDurability）：拾取时合并到现有同类格子 Count++（受 MaxStack 限制）
 *   - 带耐久道具（bUsesDurability）：每个永远新建一格，各管各的耐久，不堆叠
 *
 * 使用流程：UI 点格子 → Character/Controller 的 Server RPC → 本组件 ServerUseItem(Index)
 *   → ItemDef->ServerCanUse 校验 → ItemDef->ServerUseItem 改 Entry（扣耐久/数量）
 *   → 组件检查 Entry，耗尽则删格 → 复制 → OnInventoryChanged → UI 刷新
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class TFPS_C_API UFPSInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFPSInventoryComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ---- 服务端权威接口 ----

	/**
	 * 服务端：向背包添加一个全新道具（按 Def->DefaultValue 给初始耐久/数量）。
	 * 地图手摆 Pickup 拾取走这条。
	 * @return 是否成功（背包满且无法堆叠时返回 false，拾取方据此决定是否销毁 Pickup）。
	 */
	bool ServerAddItem(UFPSItemDef* Def);

	/**
	 * 服务端：把一条已有运行时状态的背包格加进背包（丢弃后捡回走这条，保留原耐久/数量）。
	 * 可堆叠道具仍会尝试并入现有格子；带耐久道具新建一格用 Entry 自带的 Durability。
	 * @return 是否成功（背包满返回 false）。
	 */
	bool ServerAddEntry(const FInventoryEntry& Entry);

	/**
	 * 服务端：开始使用第 Index 格的道具。
	 * 对即时类道具（Instant）同步完成全部效果；对异步类道具（ChanneledHeal/HoTApply）
	 * 只扣耐久/数量，把异步配置打包进 FItemUseResult 返回给调用方驱动状态机。
	 * @return 使用结果。UseType==None 表示无法使用（条件不满足或已耗尽）。
	 */
	FItemUseResult BeginItemUse(int32 Index);

	/** 服务端：使用第 Index 格的道具（即时类道具同步路径，保留兼容）。 */
	void ServerUseItem(int32 Index);

	/**
	 * 服务端：丢弃第 Index 格的道具 —— 从背包移除并在玩家脚下生成对应 Pickup。
	 * Pickup 类由 ItemDef->DropPickupClass 指定（蓝图数据资产填）。
	 */
	void ServerDropItem(int32 Index);

	/**
	 * 服务端：移除第 Index 格道具（不生成掉落 Pickup，用于提交/消耗等不落地场景）。
	 * 静默删除——不广播 OnPickupBlocked、不生成任何 Actor。
	 */
	void ServerRemoveItemAt(int32 Index);

	/** 服务端：清空背包（角色复活时可调用，当前 MVP 暂不强制清，保留接口）。 */
	void ServerClear();

	/**
	 * 服务端：从背包消耗指定口径的子弹（换弹时 Weapon 调用）。
	 * 遍历背包中所有匹配 Caliber 的 UFPSAmmoItemDef 条目，按条目顺序逐格取弹。
	 * @param Caliber 需要的口径
	 * @param Amount 需要的发数
	 * @param OutLastDef 输出最后取弹的弹药类型（无匹配时为 nullptr）
	 * @param OutBreakdown 可选：输出每次取弹的（AmmoDef, Count）明细，供调用方按弹药链顺序记录
	 * @return 实际取到的发数（<= Amount）
	 */
	int32 ConsumeAmmo(FName Caliber, int32 Amount, UFPSAmmoItemDef*& OutLastDef,
		TArray<struct FWeaponAmmoEntry>* OutBreakdown = nullptr);

	/** 手动触发背包复制刷新（Listen Server host 不走复制，武器换弹后需调此刷新 UI）。 */
	void MarkItemsDirty();

	// ---- 蓝图/UI 读取 ----

	/** 背包当前内容（只读引用）。UI 遍历此数组绘制格子。 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	const TArray<FInventoryEntry>& GetItems() const { return Items; }

	/** 背包格数上限（= 行 × 列）。UI 用来判断铺多少空格。 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 GetMaxSlots() const { return GridColumns * GridRows; }

	/** 背包网格列数（UI 铺格用：第 Index 个道具 → Row=Index/Columns, Col=Index%Columns）。 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 GetGridColumns() const { return GridColumns; }

	/** 背包网格行数。 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 GetGridRows() const { return GridRows; }

	/** 背包内容变化事件 —— UI 订阅刷新。 */
	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FOnInventoryChanged OnInventoryChanged;

protected:
	// 网格容量 = 列 × 行。MVP 固定（套娃背包升级时由装备的背包道具改写这两个值）。
	// 最大设计上限 8×5（CLAUDE.md 约定），蓝图默认可填 3×5 起步。

	/** 背包网格列数（蓝图可调，MVP 默认 5）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	int32 GridColumns = 5;

	/** 背包网格行数（蓝图可调，MVP 默认 3 → 3×5=15 格）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	int32 GridRows = 3;

	/**
	 * 放置策略（升级到塔科夫式占格背包的唯一替换点）。
	 * 当前实现：计数模式 —— 只看总格数没满就返回 true，坐标填 (-1,-1) 表示"未定位"，
	 * UI 按数组顺序铺格。升级时把这里换成装箱算法：给 Def->GridWidth×GridHeight 找空位，
	 * 算出左上角 (OutX,OutY)，找不到返回 false。其余增删/使用/复制逻辑都不用动。
	 */
	bool FindSlotFor(const UFPSItemDef* Def, int32& OutX, int32& OutY) const;

	/** 背包内容（服务端权威，COND_OwnerOnly 复制给本人）。RepNotify → 广播 OnInventoryChanged。 */
	UPROPERTY(ReplicatedUsing = OnRep_Items)
	TArray<FInventoryEntry> Items;

	UFUNCTION()
	void OnRep_Items();

	/** 取得拥有此组件的角色（OwnerCharacter）。 */
	AFPSCharacter* GetOwnerCharacter() const;
};
