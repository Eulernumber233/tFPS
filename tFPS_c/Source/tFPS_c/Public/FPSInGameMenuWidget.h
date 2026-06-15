#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FPSInGameMenuWidget.generated.h"

/**
 * ESC in-game menu C++ base class.
 * Blueprint inherits this, places Continue/ExitRoom buttons.
 */
UCLASS()
class TFPS_C_API UFPSInGameMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Continue game — close menu and return to gameplay. */
	UFUNCTION(BlueprintCallable, Category = "InGameMenu")
	void ContinueGame();

	/** Exit room — disconnect and return to main menu. */
	UFUNCTION(BlueprintCallable, Category = "InGameMenu")
	void ExitRoom();
};
