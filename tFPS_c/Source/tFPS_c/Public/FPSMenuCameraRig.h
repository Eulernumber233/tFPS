#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FPSMenuCameraRig.generated.h"

class USplineComponent;
class ACineCameraActor;

/** How the camera behaves when it reaches the end of the spline. */
UENUM(BlueprintType)
enum class EFPSMenuCameraLoopMode : uint8
{
	Loop,       // Jump back to start
	PingPong    // Reverse direction and go back
};

/** How the camera rotates while moving along the spline. */
UENUM(BlueprintType)
enum class EFPSMenuCameraRotationMode : uint8
{
	FollowSpline,   // Face along the spline direction
	LookAtTarget,   // Stay focused on a target actor
	Fixed           // Keep the original camera rotation
};

/**
 * Moves a CineCameraActor along a spline path for animated menu backgrounds.
 *
 * Place a CineCameraActor in the level and point this rig at it.
 * On BeginPlay the rig auto-sets the local player's view target to the camera.
 * In Tick it advances the camera along the spline.
 */
UCLASS()
class TFPS_C_API AFPSMenuCameraRig : public AActor
{
	GENERATED_BODY()

public:
	AFPSMenuCameraRig();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** The spline defines the camera track. Add points in the editor. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USplineComponent> SplineComponent;

	// ---- Configuration ----

	/** The CineCameraActor to move. Drag from the level in the editor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	TObjectPtr<ACineCameraActor> CameraActor;

	/** Movement speed along the spline (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float OrbitSpeed = 200.0f;

	/** Auto-start orbiting on BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	bool bAutoActivate = true;

	/** Blend time when switching view target to this camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float ViewBlendTime = 0.5f;

	/** Camera FOV override (0 = use CineCameraActor's own FOV). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float FOV = 90.0f;

	/** Loop behavior when reaching spline end. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	EFPSMenuCameraLoopMode LoopMode = EFPSMenuCameraLoopMode::Loop;

	/** How the camera rotates while moving. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	EFPSMenuCameraRotationMode RotationMode = EFPSMenuCameraRotationMode::FollowSpline;

	/** Target to look at when RotationMode is LookAtTarget. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	TObjectPtr<AActor> LookAtTarget;

	/** Extra pitch offset added to the final rotation (degrees). Negative = look down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float PitchOffset = -15.0f;

	// ---- Runtime state ----

	/** Whether the camera is currently moving. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime")
	bool bIsOrbiting = false;

	// ---- Blueprint API ----

	/** Start or resume camera movement. */
	UFUNCTION(BlueprintCallable, Category = "MenuCamera")
	void StartOrbit();

	/** Pause camera movement. */
	UFUNCTION(BlueprintCallable, Category = "MenuCamera")
	void StopOrbit();

private:
	/** Distance traveled along the spline so far. */
	float CurrentDistance = 0.0f;

	/** +1 for forward, -1 for backward (used in PingPong mode). */
	float PingPongDirection = 1.0f;
};
