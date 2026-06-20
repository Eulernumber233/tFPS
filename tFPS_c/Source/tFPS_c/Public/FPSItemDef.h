#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FPSItemDef.generated.h"

class AFPSCharacter;
class AFPSPickup;
class UStaticMesh;

// LevelColorMapping
USTRUCT(BlueprintType)
struct FLevelColor
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Level = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor Color = FLinearColor::White;
};

UCLASS(BlueprintType)
class ULevelColorMapping : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FLevelColor> LevelColors;
};

/**
 * How an item behaves when "used" from the inventory.
 * Instant = synchronous (current heal items);
 * ChanneledHeal = wind-up + periodic heal while locked (Delta Force big medkit);
 * HoTApply = brief application then free-acting HoT buff (PUBG painkiller).
 */
UENUM(BlueprintType)
enum class EFPSItemUseType : uint8
{
	None			UMETA(DisplayName = "None"),
	Instant			UMETA(DisplayName = "Instant"),
	ChanneledHeal	UMETA(DisplayName = "ChanneledHeal"),
	HoTApply		UMETA(DisplayName = "HoTApply")
};

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

	/**
	 * 道具 Slot 在 CanvasPanel 中的像素 X 位置（由 RecalculateSlotPositions 填充，WBP 直接读）。
	 * 不参与网络复制（客户端本地独立计算，结果一致）。
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Layout", meta = (SkipForCompression))
	float SlotPixelX = 0.0f;

	/** 道具 Slot 在 CanvasPanel 中的像素 Y 位置。 */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Layout")
	float SlotPixelY = 0.0f;

	/** 道具 Slot 的像素宽度（= ItemDef->GridWidth × CellWidth）。MVP 全为 CellWidth。 */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Layout")
	float SlotPixelWidth = 0.0f;

	/** 道具 Slot 的像素高度（= ItemDef->GridHeight × CellHeight）。MVP 全为 CellHeight。 */
	UPROPERTY(BlueprintReadOnly, Category = "Inventory|Layout")
	float SlotPixelHeight = 0.0f;

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

	/** 道具描述语（以类为单位预设，蓝图 DataAsset 可覆盖）。tooltip / 详情面板展示用。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	FText Description;

	/** 背包格子图标 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UTexture2D> Icon = nullptr;

	/** 在地上时的拾取物网格体。通用 BP_Pickup_Generic 在 OnConstruction 里读取此字段自动设模型。
	 *  为空则显示占位方块（或由专用 BP_Pickup_xxx 子类自行提供模型）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item|Pickup")
	TSoftObjectPtr<UStaticMesh> PickupMesh = nullptr;

	/** true=带耐久（每个占一格，用 Durability）；false=可堆叠（用 Count，合并成 xN） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	bool bUsesDurability = false;

	/** 可堆叠道具单格上限（子弹=120）。带耐久道具忽略。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item", meta = (EditCondition = "!bUsesDurability"))
	int32 MaxStack = 1;

	/** 捡一次给的初始值：带耐久道具=初始耐久（血包64）；可堆叠道具=每次拾取给的数量。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	int32 DefaultValue = 1;

	/** 使用耗时（秒）。0=即时生效（如急救包）；>0=需要一段不能被打断的时间（如大血包引导/止疼药使用）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	float UseTime = 0.0f;

	/** 道具等级（不同等级不堆叠）。子弹：Tier 1=普通, Tier 2=绿, Tier 3=蓝, Tier 4=紫, Tier 5=金。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	int32 Tier = 1;

	/** 基础价值（用于带出计分）。带耐久道具按剩余比例折算（见 GetCurrentValue）。纯贵重品（金条）填 >0。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
	int32 Value = 0;

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
	 * 该道具的使用行为类型（Instant/ChanneledHeal/HoTApply/None）。子类覆盖返回对应枚举。
	 * 背包组件用此决定同步执行还是走 Character 异步状态机。
	 */
	virtual EFPSItemUseType GetItemUseType() const { return EFPSItemUseType::None; }

	/** 运行时价值：带耐久道具按剩余比例折算（Value × Durability/DefaultValue），可堆叠=Value×Count。 */
	virtual int32 GetCurrentValue(const FInventoryEntry& Entry) const;

	/**
	 * 当前这一格能否被使用（服务端调用）。
	 * 基类默认：有 ItemDef 即可。子类覆盖加具体条件（如血包：玩家未满血 && 耐久>0）。
	 */
	virtual bool ServerCanUse(AFPSCharacter* User, const FInventoryEntry& Entry) const;

	/**
	 * 使用一次该道具（服务端权威逻辑）。
	 * 直接修改传入的 Entry（扣耐久 / 扣数量）；背包组件在调用后检查 Entry，
	 * Durability<=0 或 Count<=0 时删除该格。
	 * @return 本次消耗的耐久/数量（即时类=实际回血量，引导类=承诺回血量，HoT=1）。
	 */
	virtual int32 ServerUseItem(AFPSCharacter* User, FInventoryEntry& Entry) const;
};

/**
 * Type 1 — 即时治疗（血包）。带耐久：点一下就回血，回多少扣多少耐久。
 *
 * 例：满耐久 64 的大血包，玩家缺 30 血时使用 → 回 30 血、耐久剩 34。
 */
UCLASS()
class TFPS_C_API UFPSHealItemDef : public UFPSItemDef
{
	GENERATED_BODY()

public:
	UFPSHealItemDef();

	virtual EFPSItemUseType GetItemUseType() const override { return EFPSItemUseType::Instant; }
	virtual bool ServerCanUse(AFPSCharacter* User, const FInventoryEntry& Entry) const override;
	virtual int32 ServerUseItem(AFPSCharacter* User, FInventoryEntry& Entry) const override;
};

/**
 * Type 2 — 引导治疗（三角洲大血包风格）。UseTime 前摇期间不能奔跑/射击/瞄准/换弹，
 * 前摇结束后按 HealInterval 间隔逐跳回血，直至承诺量交付完毕。耐久在开始时一次性扣除。
 * F 键可中途取消（已扣耐久不退还）。
 */
UCLASS()
class TFPS_C_API UFPSChanneledHealItemDef : public UFPSItemDef
{
	GENERATED_BODY()

public:
	UFPSChanneledHealItemDef();

	/** 每次跳多少血 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChanneledHeal")
	int32 HealPerTick = 10;

	/** 跳血间隔（秒） */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ChanneledHeal")
	float HealInterval = 0.5f;

	virtual EFPSItemUseType GetItemUseType() const override { return EFPSItemUseType::ChanneledHeal; }
	virtual bool ServerCanUse(AFPSCharacter* User, const FInventoryEntry& Entry) const override;
	virtual int32 ServerUseItem(AFPSCharacter* User, FInventoryEntry& Entry) const override;
};

/**
 * Type 3 — 持续回血 Buff（PUBG 止疼药风格）。使用耗时 UseTime 秒（期间锁定动作），
 * 结束后激活 HoT buff：每秒回血速度和上限由 Character 的 HoTHealPerSecond / HoTMaxBuffDuration 决定，
 * 此道具仅提供持续时间（叠加延长时间，不超过 Character 的上限）。
 * 本类型是一次性消耗品（Count 记数量，用一次扣 1）。
 */
UCLASS()
class TFPS_C_API UFPSHoTItemDef : public UFPSItemDef
{
	GENERATED_BODY()

public:
	UFPSHoTItemDef();

	/** 单次使用给 HoT buff 增加多少秒 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HoT")
	float HoTBaseDuration = 10.0f;

	virtual EFPSItemUseType GetItemUseType() const override { return EFPSItemUseType::HoTApply; }
	virtual bool ServerCanUse(AFPSCharacter* User, const FInventoryEntry& Entry) const override;
	virtual int32 ServerUseItem(AFPSCharacter* User, FInventoryEntry& Entry) const override;
};

/**
 * Type 4 — 纯价值物品（金条、钻石、金币）。不可使用，仅用于带出计分。
 * ServerCanUse 永远返回 false，Value 字段填入价值。
 */
UCLASS()
class TFPS_C_API UFPSValuableItemDef : public UFPSItemDef
{
	GENERATED_BODY()

public:
	UFPSValuableItemDef();

	virtual EFPSItemUseType GetItemUseType() const override { return EFPSItemUseType::None; }
	// ServerCanUse 返回 false（贵重品不可"使用"）
	// ServerUseItem 空实现
};

/**
 * Type 5 — 子弹（可堆叠消耗品）。Caliber + Tier 决定能否堆叠：
 * 不同 Caliber 或不同 Tier 的子弹是不同的 DataAsset 实例，天然不会并到同一格。
 *
 * 子弹进入背包管理（作为 FInventoryEntry，走 Count 堆叠），换弹时 Weapon 从背包
 * 找匹配 AcceptedCaliber 的 UFPSAmmoItemDef 条目逐格取弹补入弹夹。
 */
UCLASS()
class TFPS_C_API UFPSAmmoItemDef : public UFPSItemDef
{
	GENERATED_BODY()

public:
	UFPSAmmoItemDef();

	/** 弹药口径（如 "5.56"、"7.62"、"9mm"、"12Gauge"）。武器只接受匹配口径的子弹。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo")
	FName Caliber;

	/** 此弹药提供的单发额外伤害（加到武器基础伤害上）。高 Tier 子弹可填更高值。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ammo")
	float BaseDamage = 0.0f;

	virtual EFPSItemUseType GetItemUseType() const override { return EFPSItemUseType::None; }
	// 子弹不可直接"使用"—— 由换弹时 Weapon 从背包取弹消耗。ServerCanUse 返回 false。
};

/**
 * 弹夹中的一段弹药：同一类型的连续子弹。
 * 索引 0 = 弹夹顶部（先发射），末尾 = 弹夹底部（最后装入）。
 */
USTRUCT(BlueprintType)
struct FWeaponAmmoEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UFPSAmmoItemDef> AmmoDef = nullptr;

	UPROPERTY(BlueprintReadOnly)
	int32 Count = 0;
};
