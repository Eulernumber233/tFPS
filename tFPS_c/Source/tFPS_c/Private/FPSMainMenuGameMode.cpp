#include "FPSMainMenuGameMode.h"
#if !UE_SERVER
#include "FPSMainMenuWidget.h"
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

	// Use the same PlayerController for menu (C++ provides the logic base)
	PlayerControllerClass = AFPSPlayerController::StaticClass();
}
