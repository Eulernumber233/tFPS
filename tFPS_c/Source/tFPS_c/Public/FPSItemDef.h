#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FPSItemDef.generated.h"

class AFPSCharacter;
class AFPSPickup;

/**
 * 背包中一格的运行时记录（复制的结构体）。
 *
 * 两种道具共用此结构，由 ItemDef->bUsesDurability 决定走哪条字段：
 *   - 可堆叠（子弹、一次性药）：用 Count，多个同类合并成一格 xN
 *   - 带耐久（血包，治疗扣耐久）：每个独立占一格，用 Durability，不堆叠
 *
 * ItemDef 是指向数据资产（UDataAsset）的指针 —— DataAsset 在所有端本地都有同一份，
 * 复制时只同步这个指针（UObject 引用按路径解析），不复制资产内容。
 */  
USTRUCT(BlueprintType)
struct FInventoryEntry
{
	GENERATED_BODY()

	/** 这一格代表哪种道具（指向数据资产）。为空表示空格（已被移除）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<class UFPSItemDef> ItemDef = nullptr;

	/** 可堆叠道具的当前数量（子弹=30）。带耐久道具固定为 1。 */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 Count = 1;

	/** 带耐久道具的剩余耐久（血包剩余可治疗量=64）。可堆叠道具忽略此字段。 */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 Durability = 0;

	// ---- 空间网格预留（MVP 阶段不用：放置时填 X=Y=-1 表示"未定位"，UI 按数组顺序铺格） ----
	// 升级到塔科夫式占格背包时启用：放置策略 FindSlotFor() 用装箱算法算出 (X,Y)，
	// UI 改用 CanvasPanel 按 (X,Y) + ItemDef 的 GridWidth/GridHeight 绝对定位 + 拖拽。

	/** 道具在网格中的左上角列坐标（-1=未定位，计数网格模式不关心）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 GridX = -1;

	/** 道具在网格中的左上角行坐标（-1=未定位）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 GridY = -1;

	bool IsValidEntry() const { return ItemDef != nullptr; }
};

/**
 * 道具数据定义（DataAsset，所有端本地各持一份，不复制）。
 *
 * 这是"大血包"这个**数据**本身：名字、图标、是否耐久、堆叠上限、初始值，
 * 以及"使用一次会发生什么"的权威逻辑（ServerUseItem）。
 *
 * MVP 阶段使用方案 A（多态子类）：每种道具一个子类，override ServerUseItem。
 * 未来若升级到方案 B（Effect 列表），只需把 ServerUseItem 内部改成遍历 Effects 数组，
 * 背包组件 / UI / Pickup 全都不用动 —— 它们只认 ItemDef->ServerUseItem 这个接口。
 *
 * 蓝图侧用法：右键 Content → Miscellaneous → Data Asset → 选 FPSHealItemDef，
 * 得到一个 DA_HealLarge 资产实例，在细节面板填 DisplayName / Icon / DefaultValue 等。
 */
UCLASS(Abstract, BlueprintType)
class TFPS_C_API UFPSItemDef : public UDataAsset
{
	GENERATED_BODY()

public:
	/** 背包/拾取提示显示的名字 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	/** 背包格子图标 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UTexture2D> Icon = nullptr;

	/** true=带耐久（每个占一格，用 Durability）；false=可堆叠（用 Count，合并成 xN） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	bool bUsesDurability = false;

	/** 可堆叠道具单格上限（子弹=120）。带耐久道具忽略。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (EditCondition = "!bUsesDurability"))
	int32 MaxStack = 1;

	/** 捡一次给的初始值：带耐久道具=初始耐久（血包64）；可堆叠道具=每次拾取给的数量。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	int32 DefaultValue = 1;

	// ---- 空间网格预留（MVP 阶段所有道具都填 1×1，每个占一格；升级塔科夫式时改这两个值） ----

	/** 道具在背包网格占的列数（宽）。MVP 全为 1。塔科夫式：大狙=1、子弹=1、医疗箱=2…… */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Grid")
	int32 GridWidth = 1;

	/** 道具在背包网格占的行数（高）。MVP 全为 1。塔科夫式：大狙=5、手枪=2…… */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Grid")
	int32 GridHeight = 1;

	/**
	 * 丢弃该道具时在地上生成的 Pickup 蓝图类。
	 * 一般指向与"拾取该道具的那个 Pickup"相同的蓝图（如 BP_Pickup_HealLarge）。
	 * 为空则该道具不可丢弃（ServerDropItem 直接 return）。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TSubclassOf<AFPSPickup> DropPickupClass;

	/**
	 * 当前这一格能否被使用（服务端调用）。
	 * 基类默认：有 ItemDef 即可。子类覆盖加具体条件（如血包：玩家未满血 && 耐久>0）。
	 */
	virtual bool ServerCanUse(AFPSCharacter* User, const FInventoryEntry& Entry) const;

	/**
	 * 使用一次该道具（服务端调用，权威逻辑）。
	 * 直接修改传入的 Entry（扣耐久 / 扣数量）；背包组件在调用后检查 Entry，
	 * Durability<=0 或 Count<=0 时删除该格。
	 * 基类为空实现，子类必须覆盖。
	 */
	virtual void ServerUseItem(AFPSCharacter* User, FInventoryEntry& Entry) const;
};

/**
 * 治疗类道具定义（血包）。带耐久：治疗多少血就扣多少耐久。
 *
 * 例：满耐久 64 的大血包，玩家缺 30 血时使用 → 回 30 血、耐久剩 34。
 * 玩家满血时 ServerCanUse 返回 false，不浪费。
 */
UCLASS()
class TFPS_C_API UFPSHealItemDef : public UFPSItemDef
{
	GENERATED_BODY()

public:
	UFPSHealItemDef();

	virtual bool ServerCanUse(AFPSCharacter* User, const FInventoryEntry& Entry) const override;
	virtual void ServerUseItem(AFPSCharacter* User, FInventoryEntry& Entry) const override;
};
