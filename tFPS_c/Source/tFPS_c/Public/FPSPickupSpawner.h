#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FPSPickupSpawner.generated.h"

class UFPSItemDef;
class AFPSPickup;
class UBillboardComponent;

/**
 * 生成表条目：一个可生成的 ItemDef 及其基础权重。
 * 在蓝图子类的 Details 面板中填写，每个条目决定"这个 Spawner 能生成什么、概率多大"。
 */
USTRUCT(BlueprintType)
struct FSpawnTableEntry
{
	GENERATED_BODY()

	/** 要生成的 Item 数据（DA_HealLarge / DA_Ammo556_T2 等）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	TObjectPtr<UFPSItemDef> ItemDef = nullptr;

	/** 基础生成权重（越大越容易被选中，默认 1.0）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	float BaseWeight = 1.0f;

	/**
	 * 该条目的专属 Pickup 蓝图（可选）。优先于 Spawner 的 DefaultPickupClass。
	 * 为空时逐级回退到 DefaultPickupClass → ItemDef->DropPickupClass。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	TSubclassOf<AFPSPickup> OverridePickupClass;
};

/**
 * 掉落物生成器：按时间周期性在地图上生成 Pickup。
 *
 * 核心机制：
 *   - 服务端权威：所有生成逻辑仅在 HasAuthority() 端运行，生成的 AFPSPickup 自带复制。
 *   - 计时：BaseSpawnInterval ± 随机偏移，每个周期尝试一次生成。
 *   - 防堆叠：生成前用球体重叠检是否有现成 Pickup，有则跳过本周期。
 *   - Tier 递进：CurrentMaxTier = Floor(Lerp(BaseSpawnTier, MaxSpawnTier, Elapsed/TierRampTime))，
 *     仅 ItemDef->Tier <= CurrentMaxTier 的条目参与抽选。越早的游戏只有低级道具，随时间高级道具加入池。
 *   - 偏向：BiasItemClass 不为空时，该子类的条目权重 *= BiasDegree。例如设 UFPSHealItemDef + BiasDegree=3.0，
 *     所有治疗类道具的生成概率变为 3 倍。
 *   - Pickup 蓝图回退链：条目 OverridePickupClass → Spawner DefaultPickupClass → ItemDef->DropPickupClass。
 */
UCLASS()
class TFPS_C_API AFPSPickupSpawner : public AActor
{
	GENERATED_BODY()

public:
	AFPSPickupSpawner();

	virtual void BeginPlay() override;

	// ============================================================
	//  生成表 — 可在此填写所有可能生成的道具
	// ============================================================

	/** 所有可生成的道具条目，每项含 ItemDef + 基础权重 + 可选专属 Pickup 蓝图。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn|Table")
	TArray<FSpawnTableEntry> SpawnTable;

	/** 默认 Pickup 蓝图（条目未填 OverridePickupClass 且 ItemDef->DropPickupClass 也为空时的兜底）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn|Table")
	TSubclassOf<AFPSPickup> DefaultPickupClass;

	// ============================================================
	//  计时 — 生成间隔与首次延迟
	// ============================================================

	/** 基础生成间隔（秒）。每个周期 = BaseSpawnInterval ± SpawnIntervalRandomDeviation。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn|Timing")
	float BaseSpawnInterval = 30.0f;

	/** 生成间隔的随机偏移（秒），均匀分布在 ± 此值范围内。最终间隔 = BaseSpawnInterval + RandRange(-Dev, +Dev)，
	 *  下限 1 秒防止连续高频生成。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn|Timing")
	float SpawnIntervalRandomDeviation = 10.0f;

	/**
	 * 首次生成前的固定延迟（秒）。>= 0 时直接用此值（不加随机）；< 0 时等同常规间隔。
	 * 用于控制"开局多久地图上开始有货"。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn|Timing")
	float InitialSpawnDelay = -1.0f;

	// ============================================================
	//  Tier 递进 — 随时间提高可生成等级
	// ============================================================

	/** 游戏开始时即可生成的 Tier。仅 ItemDef->Tier <= CurrentMaxTier 的条目进入抽选池。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn|Tier", meta = (ClampMin = "1", ClampMax = "5"))
	int32 BaseSpawnTier = 1;

	/** 可生成的最高 Tier（在 TierRampTime 后完全解锁）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn|Tier", meta = (ClampMin = "1", ClampMax = "5"))
	int32 MaxSpawnTier = 5;

	/**
	 * Tier 递进时间（秒）：从 BaseSpawnTier 线性扩展到 MaxSpawnTier 的时长。
	 * 例：Base=1, Max=5, Ramp=300s → 75s 后 CurrentMaxTier=2, 150s 后=3, 225s 后=4, 300s 后=5。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn|Tier")
	float TierRampTime = 300.0f;

	// ============================================================
	//  偏向 — 提高某类道具的生成概率
	// ============================================================

	/**
	 * 偏向的 ItemDef 子类。不为空时，所有 IsA(BiasItemClass) 的条目权重 *= BiasDegree。
	 * 例：设为 UFPSHealItemDef，所有血包类道具概率倍增。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn|Bias")
	TSubclassOf<UFPSItemDef> BiasItemClass;

	/** 偏向倍率（>= 1.0）。2.0 = 偏向类型权重翻倍。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn|Bias", meta = (ClampMin = "1.0"))
	float BiasDegree = 2.0f;

	// ============================================================
	//  行为 — 防堆叠 / 自动启停
	// ============================================================

	/** 防堆叠检查半径（cm）：生成前检测此范围内是否已有 AFPSPickup，有则跳过本周期。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	float SpawnCheckRadius = 80.0f;

	/** 是否在 BeginPlay 时自动开始生成循环。关掉后可由蓝图手动调 StartSpawning。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	bool bAutoStart = true;

	// ============================================================
	//  蓝图接口
	// ============================================================

	/** 立刻尝试生成一次（不重置 timer）。仅服务端有效。 */
	UFUNCTION(BlueprintCallable, Category = "Spawn")
	void ForceSpawn();

	/** 启动生成循环。若已在运行则无副作用。 */
	UFUNCTION(BlueprintCallable, Category = "Spawn")
	void StartSpawning();

	/** 停止生成循环（不清除已生成的 Pickup）。 */
	UFUNCTION(BlueprintCallable, Category = "Spawn")
	void StopSpawning();

	/** 查询当前根据已过时间计算出的最大可生成 Tier。 */
	UFUNCTION(BlueprintCallable, Category = "Spawn")
	int32 GetCurrentMaxTier() const;

	// ============================================================
	//  蓝图事件（BlueprintImplementableEvent）
	// ============================================================

	/** 生成物品成功时触发（仅服务端，参数为生成的 Pickup）。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Spawn|Events")
	void OnSpawnSucceeded(AFPSPickup* SpawnedPickup);

	/** 进入等待生成状态时触发（仅服务端，上一轮 Pickup 已被拾取销毁，开始倒计时下一轮生成）。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Spawn|Events")
	void OnWaitingForRespawn();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	/** 周期到点 → 尝试生成一个 Pickup。 */
	void TrySpawn();

	/** 根据 BaseSpawnInterval + 随机偏移安排下一次生成。 */
	void ScheduleNextSpawn();

	/**
	 * 从生成表中按权重 + Tier 过滤 + 偏向抽选一个 ItemDef。
	 * @return 抽中的 ItemDef，无可选条目则返回 nullptr。
	 */
	UFPSItemDef* SelectItemToSpawn() const;

	/** 防堆叠：SpawnCheckRadius 内是否已有 AFPSPickup。 */
	bool IsPickupAlreadyAtSpawnPoint() const;

	/** 解析条目最终使用的 Pickup 蓝图类（Override → Default → ItemDef::DropPickupClass）。 */
	TSubclassOf<AFPSPickup> GetPickupClassForEntry(const FSpawnTableEntry& Entry) const;

	/** 当前生成的 Pickup 被销毁时回调（被拾取 / 游戏结束等）。 */
	UFUNCTION()
	void OnSpawnedPickupDestroyed(AActor* DestroyedActor);

	/** 编辑器半可视化 — 显示生成点位置与防堆叠径。 */
	UPROPERTY()
	TObjectPtr<UBillboardComponent> Billboard;

	/** 生成循环开始时的世界时间（用于 Tier 递进计算）。 */
	float SpawnStartWorldTime = 0.0f;

	/** 生成 timer 柄。 */
	FTimerHandle SpawnTimerHandle;

	/** 生成循环是否运行中。 */
	bool bSpawning = false;

	/** 当前等待被拾取的 Pickup（弱引用，被销毁后自动置空）。 */
	UPROPERTY()
	TWeakObjectPtr<AFPSPickup> CurrentSpawnedPickup;
};
