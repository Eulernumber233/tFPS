#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "FPSGameState.generated.h"

UENUM(BlueprintType)
enum class EMatchStage : uint8
{
	Preparation	UMETA(DisplayName = "Preparation"),
	Countdown	UMETA(DisplayName = "Countdown"),
	Playing		UMETA(DisplayName = "Playing"),
	PostGame	UMETA(DisplayName = "PostGame")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchStageChanged, EMatchStage, NewStage);

UCLASS()
class TFPS_C_API AFPSGameState : public AGameState
{
	GENERATED_BODY()

public:
	AFPSGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Current match phase */
	UPROPERTY(ReplicatedUsing = OnRep_MatchStage, BlueprintReadOnly, Category = "Match")
	EMatchStage MatchStage = EMatchStage::Preparation;

	/** Seconds remaining (Playing phase = match timer; Countdown phase = countdown seconds). -1 = no timer. */
	UPROPERTY(ReplicatedUsing = OnRep_TimeRemaining, BlueprintReadOnly, Category = "Match")
	float TimeRemaining = -1.0f;

	/** Countdown seconds (separate from TimeRemaining for UI clarity; decremented each second in Countdown phase). */
	UPROPERTY(ReplicatedUsing = OnRep_CountdownSeconds, BlueprintReadOnly, Category = "Match")
	int32 CountdownSeconds = 0;

	/** Broadcast on all clients when MatchStage changes. UI widgets subscribe to this. */
	UPROPERTY(BlueprintAssignable, Category = "Match")
	FOnMatchStageChanged OnMatchStageChanged;

	/** Called on clients when MatchStage replicates */
	UFUNCTION()
	void OnRep_MatchStage();

	/** Called on clients when TimeRemaining replicates */
	UFUNCTION()
	void OnRep_TimeRemaining();

	/** Called on clients when CountdownSeconds replicates */
	UFUNCTION()
	void OnRep_CountdownSeconds();
};
