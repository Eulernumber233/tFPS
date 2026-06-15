#include "FPSPlayerState.h"
#include "Net/UnrealNetwork.h"

void AFPSPlayerState::ResetStats()
{
	if (!HasAuthority())
		return;

	Kills = 0;
	Deaths = 0;
	TotalDamage = 0.0f;
	CarryValue = 0.0f;
	OnRep_Kills();
	OnRep_Deaths();
	OnRep_TotalDamage();
	OnRep_CarryValue();
}

void AFPSPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AFPSPlayerState, Kills, COND_None);
	DOREPLIFETIME_CONDITION(AFPSPlayerState, Deaths, COND_None);
	DOREPLIFETIME_CONDITION(AFPSPlayerState, TotalDamage, COND_None);
	DOREPLIFETIME_CONDITION(AFPSPlayerState, CarryValue, COND_None);
	DOREPLIFETIME_CONDITION(AFPSPlayerState, PlayerIcon, COND_None);
}

void AFPSPlayerState::AddKill(int32 Amount)
{
	if (!HasAuthority())
		return;

	Kills += Amount;
}

void AFPSPlayerState::AddDeath(int32 Amount)
{
	if (!HasAuthority())
		return;

	Deaths += Amount;
}

void AFPSPlayerState::AddDamage(float Amount)
{
	if (!HasAuthority() || Amount <= 0.0f)
		return;

	TotalDamage += Amount;
}

void AFPSPlayerState::AddCarryValue(float Amount)
{
	if (!HasAuthority() || Amount <= 0.0f)
		return;

	CarryValue += Amount;
}

void AFPSPlayerState::OnRep_Kills()
{
	// Client-side UI update hook
}

void AFPSPlayerState::OnRep_Deaths()
{
	// Client-side UI update hook
}

void AFPSPlayerState::OnRep_TotalDamage()
{
	// Client-side UI update hook（计分板打开时由 ScoreboardWidget 主动重拉，无需此处推送）
}

void AFPSPlayerState::OnRep_CarryValue()
{
	// Client-side UI update hook
}
