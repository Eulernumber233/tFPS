#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FPSSubmissionPoint.generated.h"

class USphereComponent;
class USceneComponent;
class AFPSCharacter;

/** 提交点状态——服务端权威，复制到所有端。 */
UENUM(BlueprintType)
enum class EFPSSubmissionState : uint8
{
	Closed	UMETA(DisplayName = "Closed"),
	Open	UMETA(DisplayName = "Open")
};

/**
 * 物品提交点 Actor（场景手摆，复制）。
 *
 * 随机进入"开放"状态，开放期间玩家走到范围内可以：
 *   - 按 F 键一键提交背包中所有纯价值物品（UFPSValuableItemDef）
 *   - 打开背包右键物品单独提交（菜单出现"提交"选项）
 *
 * 提交的物品按其 GetCurrentValue 累加到玩家 PlayerState::CarryValue。
 *
 * 沿用项目"C++ 空 Root + 蓝图加 Mesh"约定：
 *   C++ 提供 DefaultRoot + InteractionSphere；蓝图子类在 DefaultRoot 下加 StaticMesh。
 *
 * 状态机（仅服务端运行）：Closed →(随机时间)→ Open →(随机时间)→ Closed → 循环。
 */
UCLASS()
class TFPS_C_API AFPSSubmissionPoint : public AActor
{
	GENERATED_BODY()

public:
	AFPSSubmissionPoint();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 当前是否开放。 */
	UFUNCTION(BlueprintCallable, Category = "Submission")
	bool IsOpen() const { return SubmissionState == EFPSSubmissionState::Open; }

	/** 当前状态（蓝图只读）。 */
	UFUNCTION(BlueprintCallable, Category = "Submission")
	EFPSSubmissionState GetSubmissionState() const { return SubmissionState; }

	// ---- 提交（服务端权威，由 Character RPC 调用） ----

	/**
	 * 一键提交提交者背包中所有 UFPSValuableItemDef 道具。
	 * @param Submitter 提交者
	 * @return 提交成功的物品总价值（0=无物品可提交）
	 */
	float SubmitAllValuables(AFPSCharacter* Submitter);

	/**
	 * 单独提交提交者背包中第 Index 格的道具。
	 * @return 提交的物品价值（0=失败：索引无效/不是贵重品/不在范围内）
	 */
	float SubmitSingleItem(AFPSCharacter* Submitter, int32 InventoryIndex);

	// ---- 蓝图事件 ----

	/** 开放/关闭状态变化时触发（所有端，OnRep 驱动）——蓝图实现灯光/特效切换。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Submission")
	void OnSubmissionStateChanged(bool bIsOpen);

protected:
	/** C++ 默认根（蓝图子类在其下加网格体）。 */
	UPROPERTY(VisibleAnywhere, Category = "Submission")
	TObjectPtr<USceneComponent> DefaultRoot;

	/** 玩家检测范围。 */
	UPROPERTY(VisibleAnywhere, Category = "Submission")
	TObjectPtr<USphereComponent> InteractionSphere;

	/** 检测半径（蓝图可调）。 */
	UPROPERTY(EditAnywhere, Category = "Submission")
	float InteractionRadius = 200.0f;

	// ---- 随机时机配置 ----

	/** 关闭状态最短持续时间（秒）。 */
	UPROPERTY(EditAnywhere, Category = "Submission|Timing")
	float MinClosedTime = 30.0f;

	/** 关闭状态最长持续时间（秒）。 */
	UPROPERTY(EditAnywhere, Category = "Submission|Timing")
	float MaxClosedTime = 120.0f;

	/** 开放状态最短持续时间（秒）。 */
	UPROPERTY(EditAnywhere, Category = "Submission|Timing")
	float MinOpenTime = 15.0f;

	/** 开放状态最长持续时间（秒）。 */
	UPROPERTY(EditAnywhere, Category = "Submission|Timing")
	float MaxOpenTime = 45.0f;

	/** 当前状态（服务端权威，复制）。 */
	UPROPERTY(ReplicatedUsing = OnRep_SubmissionState)
	EFPSSubmissionState SubmissionState = EFPSSubmissionState::Closed;

	UFUNCTION()
	void OnRep_SubmissionState();

	// ---- 重叠事件 ----

	UFUNCTION()
	void OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);

	UFUNCTION()
	void OnSphereEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// ---- 状态机（仅服务端） ----

	FTimerHandle StateTimer;

	/** 切换到指定状态并排下一轮计时器。 */
	void SetOpen(bool bOpen);

	/** 排定下一次状态变更（根据随机区间）。 */
	void ScheduleNextStateChange();

	/** 通知所有范围内角色本点状态变化。 */
	void UpdateOverlappingCharacters();
};
