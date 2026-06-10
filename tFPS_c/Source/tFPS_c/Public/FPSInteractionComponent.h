#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FPSInteractionComponent.generated.h"

/** 交互目标类型——新增类型在此追加。 */
UENUM(BlueprintType)
enum class EFPSInteractionType : uint8
{
	Pickup			UMETA(DisplayName = "Pickup"),
	WeaponPickup	UMETA(DisplayName = "WeaponPickup"),
	SubmissionPoint	UMETA(DisplayName = "SubmissionPoint")
};

/** 单个交互条目——UI 根据此结构展示提示框列表。 */
USTRUCT(BlueprintType)
struct FFPSInteractionEntry
{
	GENERATED_BODY()

	/** 发出交互邀请的源 Actor（Pickup / Weapon / SubmissionPoint）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<AActor> Source = nullptr;

	/** 交互类型——UI 据此决定图标/颜色；Interact() 据此路由行为。 */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	EFPSInteractionType Type = EFPSInteractionType::Pickup;

	/** 提示文字（如 "Pick up 大血包" / "Swap weapon" / "Submit valuables"）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FText PromptText;

	/** 排序优先级（数越大越靠前）。武器=100，提交点=50，拾取物=0。同一优先级按注册顺序。 */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	int32 Priority = 0;
};

/** 交互列表变化事件——UI 订阅后重建提示框列表。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInteractionsChanged);

/**
 * 交互管理器（挂在 AFPSCharacter 上）。
 *
 * 管理所有 F 键交互目标的列表：地上道具、枪支、提交点等。
 * 当有多个同时存在时，玩家用滚轮切换当前选中项，F 键只对选中项生效。
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class TFPS_C_API UFPSInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFPSInteractionComponent();

	/** 注册一个交互源。若同一 Source 已注册则忽略。新条目按优先级插入排序位置。 */
	void RegisterInteraction(AActor* Source, EFPSInteractionType Type, const FText& Prompt, int32 Priority);

	/** 注销一个交互源。若当前选中条目被移除，自动选择相邻条目。 */
	void UnregisterInteraction(AActor* Source);

	/** 选中下一个交互条目（循环：末尾→开头）。无条目时无操作。 */
	void CycleNext();

	/** 选中上一个交互条目（循环：开头→末尾）。无条目时无操作。 */
	void CyclePrev();

	/** 当前选中的源 Actor（无条目时为 nullptr）。 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	AActor* GetActiveTarget() const;

	/** 当前选中条目的类型。无条目时返回 Pickup（调用方先判 GetEntryCount>0）。 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	EFPSInteractionType GetActiveType() const;

	/** 当前选中条目的提示文字。无条目时返回空文本。 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	FText GetActivePrompt() const;

	/** 交互条目列表（只读，UI 遍历画提示框）。 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	const TArray<FFPSInteractionEntry>& GetEntries() const { return Entries; }

	/** 当前选中索引（-1=无条目）。 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	int32 GetSelectedIndex() const { return SelectedIndex; }

	/** 当前条目总数。 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	int32 GetEntryCount() const { return Entries.Num(); }

	/** 条目列表变化事件——UI 订阅重建提示框。 */
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnInteractionsChanged OnInteractionsChanged;

private:
	/** 按优先级降序插入。 */
	void InsertSorted(const FFPSInteractionEntry& Entry);

	TArray<FFPSInteractionEntry> Entries;

	int32 SelectedIndex = INDEX_NONE;
};
