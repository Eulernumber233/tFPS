#include "FPSPlayerController.h"
#if !UE_SERVER
#include "FPSScoreboardWidget.h"
#include "FPSInGameMenuWidget.h"
#include "FPSCharacter.h"
#include "Blueprint/UserWidget.h"
#endif
#include "FPSGameMode.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"

// ============================================================================
// RPC Implementations
// ============================================================================

void AFPSPlayerController::ClientRespawn_Implementation()
{
}

void AFPSPlayerController::ClientShowPostGameScoreboard_Implementation()
{
#if !UE_SERVER
	if (!IsLocalController()) return;

	// Close regular scoreboard if open
	if (bScoreboardOpen)
	{
		OnScoreboardReleased();
	}

	// Close in-game menu if open
	if (bInGameMenuOpen)
	{
		HideInGameMenu();
	}

	// Reuse ScoreboardWidget in post-game mode (persistent, not Tab-hold-based)
	if (PostGameScoreboardWidget)
	{
		PostGameScoreboardWidget->RemoveFromParent();
		PostGameScoreboardWidget = nullptr;
	}

	if (!ScoreboardClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PostGame] ScoreboardClass not set in BP Controller"));
		return;
	}

	PostGameScoreboardWidget = CreateWidget<UFPSScoreboardWidget>(this, ScoreboardClass);
	if (PostGameScoreboardWidget)
	{
		PostGameScoreboardWidget->SetPostGameMode(true);
		PostGameScoreboardWidget->AddToViewport(100);
		PostGameScoreboardWidget->SetVisibility(ESlateVisibility::Visible);
		PostGameScoreboardWidget->RefreshRows();
	}

	// Lock input for scoreboard interaction
	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	bShowMouseCursor = true;
#endif
}

void AFPSPlayerController::ServerNotifyExitClicked_Implementation()
{
	// Relay to GameMode
	if (AFPSGameMode* GM = GetWorld()->GetAuthGameMode<AFPSGameMode>())
	{
		GM->OnPlayerClickedExit(this);
	}
}

void AFPSPlayerController::ServerSetPlayerIdentity_Implementation(const FString& DesiredName)
{
	if (AFPSGameMode* GM = GetWorld()->GetAuthGameMode<AFPSGameMode>())
	{
		GM->OnPlayerIdentityReceived(this, DesiredName);
	}
}

// ============================================================================
// BeginPlay / SetupInput / Tick / EndPlay
// ============================================================================

void AFPSPlayerController::BeginPlay()
{
	Super::BeginPlay();
}

void AFPSPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
}

void AFPSPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (!IsLocalController())
		return;

	// Tab key: scoreboard toggle
	const bool bTabDown = IsInputKeyDown(ScoreboardKey);

	if (bTabDown && !bScoreboardKeyWasDown)
	{
		OnScoreboardPressed();
	}
	else if (!bTabDown && bScoreboardKeyWasDown)
	{
		OnScoreboardReleased();
	}

	bScoreboardKeyWasDown = bTabDown;

	// ESC key: in-game menu toggle
	const bool bEscDown = IsInputKeyDown(InGameMenuKey);

	if (bEscDown && !bInGameMenuKeyWasDown)
	{
		if (bInGameMenuOpen)
			HideInGameMenu();
		else
			ShowInGameMenu();
	}
	else if (!bEscDown && bInGameMenuKeyWasDown)
	{
		// ESC released — do nothing (toggle is on press edge)
	}

	bInGameMenuKeyWasDown = bEscDown;
}

void AFPSPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
#if !UE_SERVER
	if (ScoreboardWidget)
	{
		ScoreboardWidget->RemoveFromParent();
		ScoreboardWidget = nullptr;
	}
	if (InGameMenuWidget)
	{
		InGameMenuWidget->RemoveFromParent();
		InGameMenuWidget = nullptr;
	}
	if (PostGameScoreboardWidget)
	{
		PostGameScoreboardWidget->RemoveFromParent();
		PostGameScoreboardWidget = nullptr;
	}
#endif
	Super::EndPlay(EndPlayReason);
}

// ============================================================================
// Scoreboard (Tab)
// ============================================================================

void AFPSPlayerController::OnScoreboardPressed()
{
#if !UE_SERVER
	if (!IsLocalController() || bScoreboardOpen)
		return;

	// Don't open scoreboard if in-game menu is open
	if (bInGameMenuOpen) return;

	if (!ScoreboardClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Scoreboard] ScoreboardClass not set in BP Controller"));
		return;
	}

	if (AFPSCharacter* Char = Cast<AFPSCharacter>(GetPawn()))
	{
		if (Char->IsInventoryOpen())
			Char->CloseInventory();
	}

	if (!ScoreboardWidget)
	{
		ScoreboardWidget = CreateWidget<UFPSScoreboardWidget>(this, ScoreboardClass);
	}

	if (ScoreboardWidget)
	{
		if (!ScoreboardWidget->IsInViewport())
			ScoreboardWidget->AddToViewport();
		ScoreboardWidget->SetVisibility(ESlateVisibility::Visible);
		ScoreboardWidget->RefreshRows();
	}

	bScoreboardOpen = true;
	bScoreboardMouseLocked = false;

	if (AFPSCharacter* Char = Cast<AFPSCharacter>(GetPawn()))
	{
		if (Char->IsAiming())
			Char->ForceStopFireAndAim();
	}

	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;
#endif
}

void AFPSPlayerController::ToggleScoreboardMouseLock()
{
#if !UE_SERVER
	if (!bScoreboardOpen) return;
	SetScoreboardMouseLocked(!bScoreboardMouseLocked);
#endif
}

void AFPSPlayerController::OnScoreboardReleased()
{
#if !UE_SERVER
	if (!bScoreboardOpen) return;

	bScoreboardOpen = false;
	SetScoreboardMouseLocked(false);

	if (ScoreboardWidget)
		ScoreboardWidget->SetVisibility(ESlateVisibility::Collapsed);
#endif
}

void AFPSPlayerController::SetScoreboardMouseLocked(bool bLock)
{
#if !UE_SERVER
	bScoreboardMouseLocked = bLock;

	if (bLock)
	{
		if (AFPSCharacter* Char = Cast<AFPSCharacter>(GetPawn()))
			Char->ForceStopFireAndAim();

		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		SetInputMode(Mode);
		bShowMouseCursor = true;
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
		bShowMouseCursor = false;
	}
#endif
}

// ============================================================================
// In-Game Menu (ESC)
// ============================================================================

void AFPSPlayerController::ShowInGameMenu()
{
#if !UE_SERVER
	if (!IsLocalController() || bInGameMenuOpen) return;

	// Close scoreboard if open
	if (bScoreboardOpen)
	{
		OnScoreboardReleased();
	}

	if (!InGameMenuClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[InGameMenu] InGameMenuClass not set in BP Controller"));
		return;
	}

	if (!InGameMenuWidget)
	{
		InGameMenuWidget = CreateWidget<UFPSInGameMenuWidget>(this, InGameMenuClass);
	}

	if (InGameMenuWidget)
	{
		InGameMenuWidget->AddToViewport(200);
		InGameMenuWidget->SetVisibility(ESlateVisibility::Visible);
	}

	bInGameMenuOpen = true;

	// Stop any held fire/aim
	if (AFPSCharacter* Char = Cast<AFPSCharacter>(GetPawn()))
	{
		Char->ForceStopFireAndAim();
	}

	FInputModeGameAndUI Mode;
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);
	bShowMouseCursor = true;
#endif
}

void AFPSPlayerController::HideInGameMenu()
{
#if !UE_SERVER
	if (!bInGameMenuOpen) return;

	bInGameMenuOpen = false;

	if (InGameMenuWidget)
	{
		InGameMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;
#endif
}
