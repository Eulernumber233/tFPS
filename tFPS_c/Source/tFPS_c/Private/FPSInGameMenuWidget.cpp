#if !UE_SERVER

#include "FPSInGameMenuWidget.h"
#include "FPSPlayerController.h"
#include "FPSGameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

void UFPSInGameMenuWidget::ContinueGame()
{
	if (AFPSPlayerController* PC = Cast<AFPSPlayerController>(GetOwningPlayer()))
	{
		PC->HideInGameMenu();
	}
}

void UFPSInGameMenuWidget::ExitRoom()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;

	// Return to main menu via GameInstance
	if (UFPSGameInstance* GI = Cast<UFPSGameInstance>(GetGameInstance()))
	{
		GI->ReturnToMainMenu(PC);
	}
}

#endif // !UE_SERVER
