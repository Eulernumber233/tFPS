#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "FPSGameState.generated.h"

UENUM(BlueprintType)
enum class EMatchStage : uint8
{
	Waiting		UMETA(DisplayName = "Waiting"),
	Playing		UMETA(DisplayName = "Playing"),
	Ended		UMETA(DisplayName = "Ended")
};

UCLASS()
class TFPS_C_API AFPSGameState : public AGameState
{
	GENERATED_BODY()

public:
	AFPSGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Current match phase */
	UPROPERTY(ReplicatedUsing = OnRep_MatchStage, BlueprintReadOnly, Category = "Match")
	EMatchStage MatchStage = EMatchStage::Waiting;

	/** Seconds remaining in the current match */
	UPROPERTY(ReplicatedUsing = OnRep_TimeRemaining, BlueprintReadOnly, Category = "Match")
	float TimeRemaining = 0.0f;

	/** Called on clients when MatchStage replicates */
	UFUNCTION()
	void OnRep_MatchStage();

	/** Called on clients when TimeRemaining replicates */
	UFUNCTION()
	void OnRep_TimeRemaining();
};
