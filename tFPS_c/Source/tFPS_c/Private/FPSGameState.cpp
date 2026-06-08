#include "FPSGameState.h"
#include "Net/UnrealNetwork.h"

AFPSGameState::AFPSGameState()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AFPSGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AFPSGameState, MatchStage, COND_None);
	DOREPLIFETIME_CONDITION(AFPSGameState, TimeRemaining, COND_None);
}

void AFPSGameState::OnRep_MatchStage()
{
	// Client-side match UI hook
}

void AFPSGameState::OnRep_TimeRemaining()
{
	// Client-side timer UI hook
}
