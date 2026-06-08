#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "FPSPlayerController.generated.h"

class UFPSScoreboardWidget;
class UUserWidget;
class UInputMappingContext;
class UInputAction;

UCLASS()
class TFPS_C_API AFPSPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** Server->Client RPC: notify client to prepare for respawn */
	UFUNCTION(Client, Reliable)
	void ClientRespawn();

	/** 计分板当前是否打开（Tab 按住期间）。Character 的开火/瞄准逻辑可查询此状态。 */
	UFUNCTION(BlueprintCallable, Category = "Scoreboard")
	bool IsScoreboardOpen() const { return bScoreboardOpen; }

	/**
	 * 计分板打开 且 鼠标已被点击锁定到面板。此时左键不应流向开火。
	 * Character 的 StartFire 入口检查此标志。
	 */
	UFUNCTION(BlueprintCallable, Category = "Scoreboard")
	bool IsScoreboardMouseLocked() const { return bScoreboardOpen && bScoreboardMouseLocked; }

	/**
	 * 切换计分板鼠标锁定（右键 toggle）。仅在计分板打开时有意义：
	 * 未锁 → 锁（弹鼠标、左键改点 UI、右键继续可再 toggle）；已锁 → 解锁（回正常游戏，左键归还射击）。
	 * 由 Character 的 StartAim（右键入口）在计分板打开时调用。计分板未打开时无副作用。
	 */
	UFUNCTION(BlueprintCallable, Category = "Scoreboard")
	void ToggleScoreboardMouseLock();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ---- 计分板（Tab，纯本地 UI） ----

	/** 计分板 WBP 类（蓝图填，须继承 UFPSScoreboardWidget）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scoreboard")
	TSubclassOf<UUserWidget> ScoreboardClass;

	/**
	 * 计分板按住的物理键（蓝图可改，默认 Tab）。
	 * 开关计分板**不走 Enhanced Input 的 Started/Completed** —— 因为锁鼠标进 GameAndUI 后，
	 * 点击 UI 会让焦点变化，Enhanced Input 对正按住的 Tab 误发 Completed（导致"点一次鼠标就退回"
	 * 的 bug），且误发后真正松开时不再补发 Completed → 面板卡死。
	 * 改为每帧在 PlayerTick 里直接轮询这个物理键的按下/松开沿，绝对可靠，不受 UI 焦点影响。
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scoreboard|Input")
	FKey ScoreboardKey = EKeys::Tab;

	/** 打开计分板（GameOnly，不弹鼠标，仍可移动/开火）。Tab 按下沿触发。 */
	void OnScoreboardPressed();

	/** 关闭计分板 + 解除鼠标锁定 + 复位 GameOnly。Tab 松开沿触发。 */
	void OnScoreboardReleased();

private:
	/** 计分板 widget 实例（本地，懒创建，复用同一个）。 */
	UPROPERTY()
	TObjectPtr<UFPSScoreboardWidget> ScoreboardWidget = nullptr;

	/** 计分板是否打开（Tab 按住中）。 */
	bool bScoreboardOpen = false;

	/** 鼠标是否已点击锁定到计分板面板。 */
	bool bScoreboardMouseLocked = false;

	/** 上一帧 Tab 物理键是否按下（PlayerTick 轮询，用于检测按下/松开沿）。 */
	bool bScoreboardKeyWasDown = false;

	/** 计分板打开期间，把鼠标锁回游戏（关闭面板/松开 Tab 时调）。 */
	void SetScoreboardMouseLocked(bool bLock);
};
