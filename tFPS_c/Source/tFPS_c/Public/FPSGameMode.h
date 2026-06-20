#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "FPSGameState.h"
#include "FPSGameMode.generated.h"

UCLASS()
class TFPS_C_API AFPSGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	AFPSGameMode();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Player login: assign default name, handle phase transitions, max-player / join-grace checks. */
	virtual void PostLogin(APlayerController* NewPlayer) override;

	/** Player logout: track player count and phase transitions (back to Preparation if count < min). */
	virtual void Logout(AController* Exiting) override;

	/** Called by AFPSCharacter::Die — phase-aware: only counts kills/deaths in Playing. */
	void OnPlayerDied(AController* Victim, AController* Killer);

	/** Post-game: player clicked "Exit" on the scoreboard. When all exit, skip to Preparation. */
	void OnPlayerClickedExit(APlayerController* PC);

	/** Server receives desired player name from client (via RPC relay from PlayerController). */
	void OnPlayerIdentityReceived(APlayerController* PC, const FString& DesiredName);

	/** Server receives avatar icon index from client. */
	void OnPlayerIconReceived(APlayerController* PC, int32 IconIndex);

protected:
	// ---- Phase configuration (EditDefaultsOnly, overridable via Blueprint) ----

	UPROPERTY(EditDefaultsOnly, Category = "Match|Phases")
	int32 MinPlayersToStart = 2;

	UPROPERTY(EditDefaultsOnly, Category = "Match|Phases")
	float CountdownDuration = 15.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Match|Phases")
	float PlayingDuration = 300.0f;

	/** Seconds after Playing starts during which new players can still join. */
	UPROPERTY(EditDefaultsOnly, Category = "Match|Phases")
	float JoinGracePeriod = 60.0f;

	/** Maximum players in the match. New connections beyond this are rejected or spectate. */
	UPROPERTY(EditDefaultsOnly, Category = "Match|Phases")
	int32 MaxPlayers = 5;

	/** Seconds to show post-game scoreboard before auto-cycling to Preparation. */
	UPROPERTY(EditDefaultsOnly, Category = "Match|Phases")
	float PostGameDuration = 15.0f;

	// ---- Phase state machine ----

	/** Master phase transition. Clears phase timers, sets GS->MatchStage, dispatches to enter function. */
	void TransitionToPhase(EMatchStage NewPhase);

	void EnterPreparation();
	void EnterCountdown();
	void EnterPlaying();
	void EnterPostGame();

	/** 1-second tick during Playing and Countdown phases. */
	void PhaseOneSecondTick();

	/** Respawn a specific player (only valid in Preparation/Countdown/Playing). */
	void RespawnPlayer(AController* Player);

	/** Pick a random unoccupied player start. */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	/** Clear all players' inventories, reset K/D/damage, give default weapons, teleport to spawns. */
	void ClearAllInventoriesAndStats();

	/** Destroy all pickups / weapon pickups / dropped items on the map (for PostGame cleanup). */
	void ResetLevelPickups();

	/** Count active (non-spectator) players. */
	int32 GetActivePlayerCount() const;

	/** Kick or spectate a player who cannot join the current phase. */
	void RejectOrSpectatePlayer(APlayerController* PC, const FString& Reason);

	/** Teleport all players to random PlayerStarts and reset their state. */
	void ResetAllPlayers();

	// ---- Timers ----

	FTimerHandle OneSecondTimerHandle;
	FTimerHandle JoinGraceTimerHandle;
	FTimerHandle PostGameTimerHandle;
	FTimerHandle RespawnTimerHandle; // kept per-death via lambda, not stored phase-wide

	/** Join grace period active (new players can join during Playing). */
	bool bJoinGraceActive = false;

	/** Post-game: count of players who clicked exit. */
	int32 PlayersClickedExit = 0;

};
