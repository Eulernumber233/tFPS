#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "FPSMainMenuGameMode.generated.h"

class AFPSMenuCameraRig;

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

protected:
	virtual void BeginPlay() override;

	/**
	 * Camera rig class to auto-spawn.
	 * Set in the Blueprint subclass (BP_MainMenuGameMode) to place a
	 * CineCameraActor + moving-spline background in the menu level.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Menu")
	TSubclassOf<AFPSMenuCameraRig> MenuCameraRigClass;
};
