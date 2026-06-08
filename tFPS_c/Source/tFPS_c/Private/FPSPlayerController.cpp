#include "FPSPlayerController.h"
#include "FPSScoreboardWidget.h"
#include "FPSCharacter.h"
#include "Blueprint/UserWidget.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"

void AFPSPlayerController::ClientRespawn_Implementation()
{
}

void AFPSPlayerController::BeginPlay()
{
	Super::BeginPlay();
	// 计分板开关改为 PlayerTick 轮询物理键（见 ScoreboardKey 注释），不再注册 IMC / 绑 IA。
}

void AFPSPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	// 计分板不绑 Enhanced Input —— 开关靠 PlayerTick 轮询 ScoreboardKey；
	// 右键 toggle 复用角色 InputAim，由 AFPSCharacter::StartAim 分流调 ToggleScoreboardMouseLock。
}

void AFPSPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (!IsLocalController())
		return;

	// 轮询计分板物理键的按下/松开沿。直接读物理键状态，不受 UI 焦点 / GameAndUI 影响 ——
	// 这是绕开 Enhanced Input 在 GameAndUI 下对 Tab 误发 Completed 的关键。
	const bool bDownNow = IsInputKeyDown(ScoreboardKey);

	if (bDownNow && !bScoreboardKeyWasDown)
	{
		OnScoreboardPressed();   // 按下沿
	}
	else if (!bDownNow && bScoreboardKeyWasDown)
	{
		OnScoreboardReleased();  // 松开沿
	}

	bScoreboardKeyWasDown = bDownNow;
}

void AFPSPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ScoreboardWidget)
	{
		ScoreboardWidget->RemoveFromParent();
		ScoreboardWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void AFPSPlayerController::OnScoreboardPressed()
{
	if (!IsLocalController() || bScoreboardOpen)
		return;

	if (!ScoreboardClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Scoreboard] ScoreboardClass 未在蓝图 Controller 指定"));
		return;
	}

	// 计分板与背包互斥：Tab 期间不允许开背包；若背包开着，先关掉它。
	if (AFPSCharacter* Char = Cast<AFPSCharacter>(GetPawn()))
	{
		if (Char->IsInventoryOpen())
			Char->CloseInventory();
	}

	// 懒创建并复用同一个 widget 实例。
	if (!ScoreboardWidget)
	{
		ScoreboardWidget = CreateWidget<UFPSScoreboardWidget>(this, ScoreboardClass);
	}

	if (ScoreboardWidget)
	{
		if (!ScoreboardWidget->IsInViewport())
			ScoreboardWidget->AddToViewport();
		ScoreboardWidget->SetVisibility(ESlateVisibility::Visible);
		ScoreboardWidget->RefreshRows();   // 显示瞬间拉最新数据
	}

	bScoreboardOpen = true;
	bScoreboardMouseLocked = false;

	// 打开瞬间停掉可能正按住的瞄准（右键此刻起被挪作 toggle，不应残留瞄准状态）。
	// 开火不停 —— 第一阶段允许继续射击。
	if (AFPSCharacter* Char = Cast<AFPSCharacter>(GetPawn()))
	{
		if (Char->IsAiming())
			Char->ForceStopFireAndAim();
	}

	// 第一阶段：纯查看。GameOnly，不弹鼠标，仍可移动/开火。右键此刻起 toggle 鼠标锁定。
	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;
}

void AFPSPlayerController::ToggleScoreboardMouseLock()
{
	// 右键 toggle：仅计分板打开时有意义。未锁 → 锁；已锁 → 解锁。
	if (!bScoreboardOpen)
		return;

	SetScoreboardMouseLocked(!bScoreboardMouseLocked);
}

void AFPSPlayerController::OnScoreboardReleased()
{
	if (!bScoreboardOpen)
		return;

	bScoreboardOpen = false;

	// 解除鼠标锁定并复位输入模式。
	SetScoreboardMouseLocked(false);

	if (ScoreboardWidget)
		ScoreboardWidget->SetVisibility(ESlateVisibility::Collapsed);
}

void AFPSPlayerController::SetScoreboardMouseLocked(bool bLock)
{
	bScoreboardMouseLocked = bLock;

	if (bLock)
	{
		// 锁定瞬间兜底停掉可能正按住的开火/瞄准（避免锁鼠标后角色还在持续开火）。
		if (AFPSCharacter* Char = Cast<AFPSCharacter>(GetPawn()))
			Char->ForceStopFireAndAim();

		// 第二阶段：锁鼠标到面板。GameAndUI + 显示鼠标。
		// 此时左键点击进入 UI（点排序按钮），不再触发开火
		//（Character::StartFire 入口会查 IsScoreboardMouseLocked() 直接 return）。
		// 键盘移动仍生效（GameAndUI 不拦键盘）。
		// 注意：不调 SetWidgetToFocus —— 设了固定焦点 widget 后，点击 ComboBox/按钮使焦点转移，
		// GameAndUI 可能因焦点离开而把控制权交还游戏导致鼠标被收回（即"点一次就退回"的元凶之一）。
		// 让 Slate 自行管理焦点更稳。
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		SetInputMode(Mode);
		bShowMouseCursor = true;
	}
	else
	{
		// 回到第一阶段（仍打开看着）或彻底关闭：GameOnly，不弹鼠标。
		SetInputMode(FInputModeGameOnly());
		bShowMouseCursor = false;
	}
}
