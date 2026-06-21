#if !UE_SERVER

#include "FPSMenuCameraRig.h"
#include "Components/SplineComponent.h"
#include "CineCameraActor.h"
#include "Kismet/GameplayStatics.h"

AFPSMenuCameraRig::AFPSMenuCameraRig()
{
	PrimaryActorTick.bCanEverTick = true;

	SplineComponent = CreateDefaultSubobject<USplineComponent>(TEXT("SplineComponent"));
	SetRootComponent(SplineComponent);
}

void AFPSMenuCameraRig::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoActivate && CameraActor)
	{
		StartOrbit();

		// Set the local player's view target to the CineCameraActor
		if (UWorld* World = GetWorld())
		{
			if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
			{
				PC->SetViewTargetWithBlend(CameraActor, ViewBlendTime);

				// Override FOV (CineCameraActor uses focal length, harder to tune)
				if (FOV > 0.0f && PC->PlayerCameraManager)
				{
					PC->PlayerCameraManager->SetFOV(FOV);
				}
			}
		}
	}
}

void AFPSMenuCameraRig::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bIsOrbiting || !CameraActor)
	{
		return;
	}

	const int32 NumPoints = SplineComponent->GetNumberOfSplinePoints();
	if (NumPoints < 2)
	{
		return;
	}

	const float SplineLength = SplineComponent->GetSplineLength();
	if (SplineLength <= 0.0f)
	{
		return;
	}

	// Advance distance along spline
	CurrentDistance += OrbitSpeed * DeltaTime * PingPongDirection;

	// Handle loop / ping-pong at boundaries
	if (LoopMode == EFPSMenuCameraLoopMode::Loop)
	{
		CurrentDistance = FMath::Fmod(CurrentDistance, SplineLength);
		if (CurrentDistance < 0.0f)
		{
			CurrentDistance += SplineLength;
		}
	}
	else // PingPong
	{
		if (CurrentDistance >= SplineLength)
		{
			CurrentDistance = SplineLength;
			PingPongDirection = -1.0f;
		}
		else if (CurrentDistance <= 0.0f)
		{
			CurrentDistance = 0.0f;
			PingPongDirection = 1.0f;
		}
	}

	// Get transform at the current distance
	CameraActor->SetActorLocation(
		SplineComponent->GetLocationAtDistanceAlongSpline(CurrentDistance, ESplineCoordinateSpace::World));

	// Rotation
	FRotator FinalRotation = CameraActor->GetActorRotation();  // fallback: Fixed mode

	switch (RotationMode)
	{
	case EFPSMenuCameraRotationMode::FollowSpline:
		FinalRotation = SplineComponent->GetRotationAtDistanceAlongSpline(
			CurrentDistance, ESplineCoordinateSpace::World);
		FinalRotation.Pitch += PitchOffset;
		break;

	case EFPSMenuCameraRotationMode::LookAtTarget:
		if (LookAtTarget)
		{
			const FVector CameraLocation = CameraActor->GetActorLocation();
			const FVector TargetLocation = LookAtTarget->GetActorLocation();
			FinalRotation = (TargetLocation - CameraLocation).Rotation();
		}
		break;

	case EFPSMenuCameraRotationMode::Fixed:
	default:
		break;
	}

	CameraActor->SetActorRotation(FinalRotation);
}

void AFPSMenuCameraRig::StartOrbit()
{
	bIsOrbiting = true;
}

void AFPSMenuCameraRig::StopOrbit()
{
	bIsOrbiting = false;
}

#endif // !UE_SERVER
