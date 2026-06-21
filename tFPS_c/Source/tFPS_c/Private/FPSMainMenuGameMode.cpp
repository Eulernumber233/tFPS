#include "FPSMainMenuGameMode.h"
#if !UE_SERVER
#include "FPSMainMenuWidget.h"
#include "FPSMenuCameraRig.h"
#endif
#include "FPSPlayerController.h"
#include "FPSGameInstance.h"
#if !UE_SERVER
#include "Blueprint/UserWidget.h"
#endif
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

AFPSMainMenuGameMode::AFPSMainMenuGameMode()
{
	// No default pawn — menu level doesn't spawn characters
	DefaultPawnClass = nullptr;

	// PlayerControllerClass is left unset here.
	// Set it in the Blueprint subclass or Project Settings → Maps & Modes.
}

void AFPSMainMenuGameMode::BeginPlay()
{
	Super::BeginPlay();

#if !UE_SERVER
	// Menu is a mouse-driven UI — show the cursor and don't lock it
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Mode);
		PC->bShowMouseCursor = true;
	}

	// Auto-spawn the camera rig if a class is configured
	if (MenuCameraRigClass && GetWorld())
	{
		AFPSMenuCameraRig* Rig = GetWorld()->SpawnActor<AFPSMenuCameraRig>(
			MenuCameraRigClass, FVector::ZeroVector, FRotator::ZeroRotator);

		// The rig auto-begins orbiting in its own BeginPlay if bAutoActivate is set.
		// If the rig wasn't created with a pre-placed CameraActor reference,
		// the level designer can also place one and wire them up via the editor.
	}
#endif
}
