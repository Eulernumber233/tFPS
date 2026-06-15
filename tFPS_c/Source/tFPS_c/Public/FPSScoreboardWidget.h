#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Texture2D.h"
#include "FPSScoreboardWidget.generated.h"

/**
 * 计分板排序键。客户端本地选择，不走网络（每端各自排自己的视图）。
 * 新增可排序字段在此追加，并在 SortPredicate 里加对应分支。
 */
UENUM(BlueprintType)
enum class EScoreboardSortKey : uint8
{
	Kills		UMETA(DisplayName = "Kills"),
	TotalDamage	UMETA(DisplayName = "Total Damage"),
	CarryValue	UMETA(DisplayName = "Carry Value")
};

/**
 * 计分板一行的数据快照（蓝图可读）。C++ 从 GameState->PlayerArray 收集 + 排序后
 * 交给 WBP 逐行铺。WBP 只读这个结构体填控件，不直接碰 PlayerState。
 */
USTRUCT(BlueprintType)
struct FScoreboardRow
{
	GENERATED_BODY()

	/** 玩家名（当前为 client1/client2/...，后续登录系统覆盖）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	FString PlayerName;

	/** 玩家头像（预留，当前可能为空 → WBP 自行用占位图）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	int32 Kills = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	int32 Deaths = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	float TotalDamage = 0.0f;

	/** 带出物品总价值（预留，当前恒 0）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	float CarryValue = 0.0f;

	/** 是否本地玩家（WBP 可据此高亮自己那一行）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	bool bIsLocalPlayer = false;
};

/**
 * 计分板 WBP 的 C++ 基类。逻辑全在 C++：收集所有玩家、按当前排序键+方向排序。
 * WBP 继承它，只摆控件（表格、排序下拉框、升降序按钮、行 WBP），
 * 按钮 OnClick 调 SetSortKey/ToggleSortOrder（内部自动 Refresh），
 * 在 OnRowsUpdated 事件里遍历 GetSortedRows() 重画表格。
 *
 * 纯本地视图：排序选择不复制，每个客户端排自己的；底层数据(Kills/Damage 等)
 * 由 PlayerState 服务端权威复制，各端拿到的数值一致。
 */
UCLASS()
class TFPS_C_API UFPSScoreboardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 重新收集 + 排序 + 触发 WBP 重画（OnRowsUpdated）。
	 * 计分板显示时、排序选项变化时、以及（可选）定时调用刷新数据。
	 */
	UFUNCTION(BlueprintCallable, Category = "Scoreboard")
	void RefreshRows();

	/** 切换排序键（下拉框 OnSelectionChanged 调）。内部自动 RefreshRows。 */
	UFUNCTION(BlueprintCallable, Category = "Scoreboard")
	void SetSortKey(EScoreboardSortKey NewKey);

	/** 翻转升/降序（升降序按钮 OnClick 调）。内部自动 RefreshRows。 */
	UFUNCTION(BlueprintCallable, Category = "Scoreboard")
	void ToggleSortOrder();

	/** 直接设置升/降序（true=降序，大的在前）。内部自动 RefreshRows。 */
	UFUNCTION(BlueprintCallable, Category = "Scoreboard")
	void SetDescending(bool bNewDescending);

	/** 当前已排序的行数据（WBP 在 OnRowsUpdated 里遍历它铺表格）。 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Scoreboard")
	const TArray<FScoreboardRow>& GetSortedRows() const { return SortedRows; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Scoreboard")
	EScoreboardSortKey GetSortKey() const { return SortKey; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Scoreboard")
	bool IsDescending() const { return bDescending; }

	/**
	 * 数据/排序更新完毕 — WBP 实现：Clear 表格 → 遍历 GetSortedRows() 生成行控件。
	 * RefreshRows 末尾触发。
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Scoreboard")
	void OnRowsUpdated();

	/** Whether this scoreboard is in PostGame mode (persistent, with exit button). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Scoreboard")
	bool IsPostGameMode() const { return bIsPostGame; }

	/** Set PostGame mode (shows exit button, disables Tab hide). */
	void SetPostGameMode(bool bPostGame);

	/** Called when player clicks "Exit" in PostGame scoreboard. Relay to Controller RPC. */
	UFUNCTION(BlueprintCallable, Category = "Scoreboard")
	void OnExitClicked();

protected:
	virtual void NativeConstruct() override;

	/** 当前排序键（本地，不复制）。默认按击杀数。 */
	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	EScoreboardSortKey SortKey = EScoreboardSortKey::Kills;

	/** 是否降序（true=大的在前，计分板默认降序）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	bool bDescending = true;

	/** 已排序的行快照。 */
	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	TArray<FScoreboardRow> SortedRows;

	/** PostGame mode: persistent display, exit button visible. */
	UPROPERTY(BlueprintReadOnly, Category = "Scoreboard")
	bool bIsPostGame = false;

private:
	/** 排序比较：按当前 SortKey 取值，bDescending 决定方向；同值用 Kills 兜底再用名字稳定排序。 */
	bool RowLess(const FScoreboardRow& A, const FScoreboardRow& B) const;
};
