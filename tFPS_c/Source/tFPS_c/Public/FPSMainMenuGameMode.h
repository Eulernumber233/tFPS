#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "FPSMainMenuGameMode.generated.h"

/**
 * Lightweight GameMode for the main menu level.
 * Does NOT spawn pawns or handle combat — just provides a blank canvas
 * for the menu UI (camera path background + UMG menu widget).
 */
UCLASS()
class TFPS_C_API AFPSMainMenuGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	AFPSMainMenuGameMode();
};
