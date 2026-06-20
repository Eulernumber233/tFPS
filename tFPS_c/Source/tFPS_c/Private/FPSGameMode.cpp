#include "FPSGameMode.h"
#include "FPSGameState.h"
#include "FPSPlayerState.h"
#include "FPSCharacter.h"
#include "FPSPlayerController.h"
#include "FPSWeapon.h"
#include "FPSPickup.h"
#include "FPSInventoryComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h"

AFPSGameMode::AFPSGameMode()
{
	GameStateClass = AFPSGameState::StaticClass();
	PlayerStateClass = AFPSPlayerState::StaticClass();
	DefaultPawnClass = AFPSCharacter::StaticClass();

	// PlayerControllerClass left unset — set via BP subclass or Project Settings.
}

void AFPSGameMode::BeginPlay()
{
	Super::BeginPlay();
	TransitionToPhase(EMatchStage::Preparation);
}

void AFPSGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(OneSecondTimerHandle);
	GetWorldTimerManager().ClearTimer(JoinGraceTimerHandle);
	GetWorldTimerManager().ClearTimer(PostGameTimerHandle);
	Super::EndPlay(EndPlayReason);
}

// ============================================================================
// Phase State Machine
// ============================================================================

void AFPSGameMode::TransitionToPhase(EMatchStage NewPhase)
{
	AFPSGameState* GS = GetGameState<AFPSGameState>();
	if (!GS) return;

	// Clear all phase-specific timers
	GetWorldTimerManager().ClearTimer(OneSecondTimerHandle);
	GetWorldTimerManager().ClearTimer(JoinGraceTimerHandle);
	GetWorldTimerManager().ClearTimer(PostGameTimerHandle);

	GS->MatchStage = NewPhase;
	GS->OnRep_MatchStage(); // fire on server (listen server host needs manual broadcast)

	UE_LOG(LogTemp, Log, TEXT("[GameMode] Phase → %d, ActivePlayers=%d"),
		(int32)NewPhase, GetActivePlayerCount());

	switch (NewPhase)
	{
	case EMatchStage::Preparation: EnterPreparation(); break;
	case EMatchStage::Countdown:   EnterCountdown();   break;
	case EMatchStage::Playing:     EnterPlaying();     break;
	case EMatchStage::PostGame:    EnterPostGame();    break;
	}
}

void AFPSGameMode::EnterPreparation()
{
	AFPSGameState* GS = GetGameState<AFPSGameState>();
	if (!GS) return;

	GS->TimeRemaining = -1.0f;
	GS->CountdownSeconds = 0;
	PlayersClickedExit = 0;
	bJoinGraceActive = false;

	// Reset input for all players (arriving from MainMenu or PostGame)
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = It->Get())
		{
			PC->SetIgnoreMoveInput(false);
			PC->SetIgnoreLookInput(false);
			PC->SetInputMode(FInputModeGameOnly());
			PC->bShowMouseCursor = false;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[GameMode] Preparation — waiting for %d+ players"), MinPlayersToStart);
}

void AFPSGameMode::EnterCountdown()
{
	AFPSGameState* GS = GetGameState<AFPSGameState>();
	if (!GS) return;

	GS->CountdownSeconds = FMath::CeilToInt(CountdownDuration);
	GS->TimeRemaining = CountdownDuration;
	GS->OnRep_CountdownSeconds(); // fire on server

	// 1-second tick for countdown
	GetWorldTimerManager().SetTimer(OneSecondTimerHandle,
		FTimerDelegate::CreateUObject(this, &AFPSGameMode::PhaseOneSecondTick), 1.0f, true);

	UE_LOG(LogTemp, Log, TEXT("[GameMode] Countdown — %d seconds"), GS->CountdownSeconds);
}

void AFPSGameMode::EnterPlaying()
{
	AFPSGameState* GS = GetGameState<AFPSGameState>();
	if (!GS) return;

	// Clear all player inventories, reset stats, give default weapons
	ClearAllInventoriesAndStats();

	GS->TimeRemaining = PlayingDuration;
	GS->CountdownSeconds = 0;

	// 1-second tick for Playing timer
	GetWorldTimerManager().SetTimer(OneSecondTimerHandle,
		FTimerDelegate::CreateUObject(this, &AFPSGameMode::PhaseOneSecondTick), 1.0f, true);

	// Join grace period: new players can join for the first N seconds
	bJoinGraceActive = true;
	GetWorldTimerManager().SetTimer(JoinGraceTimerHandle, [this]()
	{
		bJoinGraceActive = false;
		UE_LOG(LogTemp, Log, TEXT("[GameMode] Join grace period ended"));
	}, JoinGracePeriod, false);

	UE_LOG(LogTemp, Log, TEXT("[GameMode] Playing — %d seconds, join grace=%d seconds"),
		(int32)PlayingDuration, (int32)JoinGracePeriod);
}

void AFPSGameMode::EnterPostGame()
{
	AFPSGameState* GS = GetGameState<AFPSGameState>();
	if (!GS) return;

	GS->TimeRemaining = PostGameDuration;
	PlayersClickedExit = 0;
	bJoinGraceActive = false;

	// Determine winner
	AFPSPlayerState* Best = nullptr;
	int32 BestKills = -1;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) continue;

		// Disable input
		PC->SetIgnoreMoveInput(true);
		PC->SetIgnoreLookInput(true);

		// Force stop fire/aim on all characters
		if (AFPSCharacter* Char = Cast<AFPSCharacter>(PC->GetPawn()))
		{
			Char->ForceStopFireAndAim();
		}

		if (AFPSPlayerState* PS = PC->GetPlayerState<AFPSPlayerState>())
		{
			if (PS->Kills > BestKills)
			{
				BestKills = PS->Kills;
				Best = PS;
			}
		}

		// Notify controller to show post-game scoreboard
		if (AFPSPlayerController* FPS_PC = Cast<AFPSPlayerController>(PC))
		{
			FPS_PC->ClientShowPostGameScoreboard();
		}
	}

	if (Best)
	{
		UE_LOG(LogTemp, Log, TEXT(">>>>> Match Over! Winner: %s with %d kills <<<<<"),
			*Best->GetPlayerName(), Best->Kills);
	}

	// Auto-transition back to Preparation after PostGameDuration
	GetWorldTimerManager().SetTimer(PostGameTimerHandle, [this]()
	{
		ResetLevelPickups();
		ResetAllPlayers();
		TransitionToPhase(EMatchStage::Preparation);
	}, PostGameDuration, false);

	UE_LOG(LogTemp, Log, TEXT("[GameMode] PostGame — scoreboard shown, cycling in %d seconds"),
		(int32)PostGameDuration);
}

// ============================================================================
// 1-second tick
// ============================================================================

void AFPSGameMode::PhaseOneSecondTick()
{
	AFPSGameState* GS = GetGameState<AFPSGameState>();
	if (!GS) return;

	switch (GS->MatchStage)
	{
	case EMatchStage::Countdown:
	{
		GS->CountdownSeconds = FMath::Max(0, GS->CountdownSeconds - 1);
		GS->TimeRemaining = (float)GS->CountdownSeconds;
		GS->OnRep_CountdownSeconds();

		// 倒计时消息推送
		if (GS->CountdownSeconds > 0)
		{
			AFPSCharacter::BroadcastGameMessage(this,
				FString::Printf(TEXT("%d 秒后开始"), GS->CountdownSeconds),
				EFPSMessageWeight::Warning, 1.2f);
		}

		if (GS->CountdownSeconds <= 0)
		{
			TransitionToPhase(EMatchStage::Playing);
		}
		break;
	}

	case EMatchStage::Playing:
	{
		GS->TimeRemaining = FMath::Max(0.0f, GS->TimeRemaining - 1.0f);

		if (GS->TimeRemaining <= 0.0f)
		{
			TransitionToPhase(EMatchStage::PostGame);
		}
		else if (GetActivePlayerCount() == 0)
		{
			// Everyone left — end the match early
			UE_LOG(LogTemp, Log, TEXT("[GameMode] All players left, ending match early"));
			TransitionToPhase(EMatchStage::PostGame);
		}
		break;
	}

	default:
		break;
	}
}

// ============================================================================
// Player Join / Leave
// ============================================================================

void AFPSGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	if (!NewPlayer) return;

	AFPSGameState* GS = GetGameState<AFPSGameState>();
	if (!GS) return;

	// Always assign short default name (overwrite system username).
	// May be overwritten later by OnPlayerIdentityReceived (from main menu input).
	if (APlayerState* PS = NewPlayer->PlayerState)
	{
		static const TCHAR Charset[] = TEXT("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
		constexpr int32 CharsetLen = UE_ARRAY_COUNT(Charset) - 1;
		const int32 Length = FMath::RandRange(7, 10);
		FString RandomName;
		RandomName.Reserve(Length);
		for (int32 i = 0; i < Length; ++i)
		{
			RandomName.AppendChar(Charset[FMath::RandRange(0, CharsetLen - 1)]);
		}
		PS->SetPlayerName(RandomName);
	}

	switch (GS->MatchStage)
	{
	case EMatchStage::Preparation:
	case EMatchStage::Countdown:
	{
		// Check max players
		if (GetActivePlayerCount() > MaxPlayers)
		{
			RejectOrSpectatePlayer(NewPlayer, TEXT("Server is full"));
			return;
		}

		// Spawn pawn if needed
		if (NewPlayer->GetPawn() == nullptr)
		{
			RestartPlayer(NewPlayer);
		}

		// If in Preparation and we hit min players, start countdown
		if (GS->MatchStage == EMatchStage::Preparation && GetActivePlayerCount() >= MinPlayersToStart)
		{
			TransitionToPhase(EMatchStage::Countdown);
		}
		break;
	}

	case EMatchStage::Playing:
	{
		// Check max players
		if (GetActivePlayerCount() > MaxPlayers)
		{
			RejectOrSpectatePlayer(NewPlayer, TEXT("Server is full"));
			return;
		}

		// Check join grace period
		if (!bJoinGraceActive)
		{
			RejectOrSpectatePlayer(NewPlayer, TEXT("Match already started — join period ended"));
			return;
		}

		// Spawn and give default gear
		if (NewPlayer->GetPawn() == nullptr)
		{
			RestartPlayer(NewPlayer);
		}
		break;
	}

	case EMatchStage::PostGame:
	{
		// Spectate only — match is over, they'll join next round
		RejectOrSpectatePlayer(NewPlayer, TEXT("Match ended — wait for next round"));
		break;
	}
	}

	// 新玩家加入消息广播（非观战者才推送）
	if (NewPlayer->PlayerState && !NewPlayer->PlayerState->IsOnlyASpectator())
	{
		AFPSCharacter::BroadcastGameMessage(this,
			FString::Printf(TEXT("%s 加入了游戏（当前人数：%d）"), *NewPlayer->PlayerState->GetPlayerName(), GetActivePlayerCount(), *NewPlayer->PlayerState->GetPlayerName()),
			EFPSMessageWeight::Info, 5.0f);
	}
}

void AFPSGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);

	AFPSGameState* GS = GetGameState<AFPSGameState>();
	if (!GS) return;

	const int32 ActiveCount = GetActivePlayerCount(); // after Super::Logout, this player is already removed

	switch (GS->MatchStage)
	{
	case EMatchStage::Countdown:
	{
		// If player count drops below min, abort countdown
		if (ActiveCount < MinPlayersToStart)
		{
			UE_LOG(LogTemp, Log, TEXT("[GameMode] Player left during Countdown, back to Preparation"));
			TransitionToPhase(EMatchStage::Preparation);
		}
		break;
	}

	case EMatchStage::Playing:
	{
		if (ActiveCount == 0)
		{
			UE_LOG(LogTemp, Log, TEXT("[GameMode] All players left during Playing, ending match"));
			TransitionToPhase(EMatchStage::PostGame);
		}
		break;
	}

	case EMatchStage::PostGame:
	{
		// Track exits — if all remaining players click exit, skip timer
		PlayersClickedExit++;
		if (ActiveCount == 0 || PlayersClickedExit >= ActiveCount)
		{
			UE_LOG(LogTemp, Log, TEXT("[GameMode] All players exited PostGame, cycling now"));
			ResetLevelPickups();
			ResetAllPlayers();
			TransitionToPhase(EMatchStage::Preparation);
		}
		break;
	}

	default:
		break;
	}
}

void AFPSGameMode::OnPlayerClickedExit(APlayerController* PC)
{
	if (!PC) return;

	AFPSGameState* GS = GetGameState<AFPSGameState>();
	if (!GS || GS->MatchStage != EMatchStage::PostGame) return;

	PlayersClickedExit++;

	const int32 ActiveCount = GetActivePlayerCount();
	if (PlayersClickedExit >= ActiveCount)
	{
		UE_LOG(LogTemp, Log, TEXT("[GameMode] All players clicked exit, cycling immediately"));
		GetWorldTimerManager().ClearTimer(PostGameTimerHandle);
		ResetLevelPickups();
		ResetAllPlayers();
		TransitionToPhase(EMatchStage::Preparation);
	}
}

// ============================================================================
// Player Identity
// ============================================================================

void AFPSGameMode::OnPlayerIdentityReceived(APlayerController* PC, const FString& DesiredName)
{
	if (!PC || DesiredName.IsEmpty()) return;

	APlayerState* PS = PC->PlayerState;
	if (!PS) return;

	// Check for duplicate names and append suffix if needed
	FString FinalName = DesiredName;
	AFPSGameState* GS = GetGameState<AFPSGameState>();

	if (GS)
	{
		int32 Suffix = 1;
		bool bDuplicate = true;
		FString Candidate = FinalName;

		while (bDuplicate)
		{
			bDuplicate = false;
			for (APlayerState* Other : GS->PlayerArray)
			{
				if (Other && Other != PS && Other->GetPlayerName() == Candidate)
				{
					bDuplicate = true;
					Candidate = FString::Printf(TEXT("%s%d"), *DesiredName, ++Suffix);
					break;
				}
			}
		}
		FinalName = Candidate;
	}

	PS->SetPlayerName(FinalName);
	UE_LOG(LogTemp, Log, TEXT("[GameMode] Player identity: '%s' → '%s'"), *DesiredName, *FinalName);
}

void AFPSGameMode::OnPlayerIconReceived(APlayerController* PC, int32 InIconIndex)
{
	if (!PC || !HasAuthority()) return;

	AFPSPlayerState* PS = PC->GetPlayerState<AFPSPlayerState>();
	if (!PS) return;

	PS->IconIndex = FMath::Clamp(InIconIndex, 0, 5);
}

// ============================================================================
// Death & Respawn
// ============================================================================

void AFPSGameMode::OnPlayerDied(AController* Victim, AController* Killer)
{
	if (!Victim) return;

	AFPSGameState* GS = GetGameState<AFPSGameState>();
	if (!GS) return;

	// Only count kills/deaths during Playing phase
	if (GS->MatchStage == EMatchStage::Playing)
	{
		if (Killer && Killer != Victim)
		{
			if (AFPSPlayerState* KillerPS = Killer->GetPlayerState<AFPSPlayerState>())
			{
				KillerPS->AddKill(1);
			}
		}

		if (AFPSPlayerState* VictimPS = Victim->GetPlayerState<AFPSPlayerState>())
		{
			VictimPS->AddDeath(1);
		}
	}

	// Notify client
	if (AFPSPlayerController* PC = Cast<AFPSPlayerController>(Victim))
	{
		PC->ClientRespawn();
	}

	// Respawn delay: short in preparation/countdown, normal in playing
	float RespawnDelay = 2.0f;
	if (GS->MatchStage == EMatchStage::Preparation || GS->MatchStage == EMatchStage::Countdown)
	{
		RespawnDelay = 1.0f;
	}
	else if (AFPSCharacter* Char = Cast<AFPSCharacter>(Victim->GetPawn()))
	{
		RespawnDelay = Char->GetRespawnDelay();
	}

	// Don't respawn in PostGame
	if (GS->MatchStage == EMatchStage::PostGame)
		return;

	FTimerHandle TimerHandle;
	GetWorldTimerManager().SetTimer(TimerHandle,
		FTimerDelegate::CreateLambda([this, Victim]() { RespawnPlayer(Victim); }),
		RespawnDelay, false);
}

void AFPSGameMode::RespawnPlayer(AController* Player)
{
	AFPSGameState* GS = GetGameState<AFPSGameState>();
	if (!GS) return;

	// Don't respawn in PostGame
	if (GS->MatchStage == EMatchStage::PostGame)
		return;

	if (!IsValid(Player)) return;

	AFPSCharacter* Char = Cast<AFPSCharacter>(Player->GetPawn());
	if (!Char)
	{
		RestartPlayer(Player);
		return;
	}

	AActor* Start = ChoosePlayerStart(Player);
	const FVector Loc = Start ? Start->GetActorLocation() : Char->GetActorLocation();
	const FRotator Rot = Start ? Start->GetActorRotation() : Char->GetActorRotation();
	Char->Respawn(Loc, Rot);
}

// ============================================================================
// Inventory / Stats Clearing on Playing Start
// ============================================================================

void AFPSGameMode::ClearAllInventoriesAndStats()
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) continue;

		// Reset player stats
		if (AFPSPlayerState* PS = PC->GetPlayerState<AFPSPlayerState>())
		{
			PS->ResetStats();
		}

		AFPSCharacter* Char = Cast<AFPSCharacter>(PC->GetPawn());
		if (!Char) continue;

		// Clear inventory
		if (UFPSInventoryComponent* Inv = Char->GetInventory())
		{
			Inv->ServerClear();
		}

		// ResetLoadout() destroys old weapons, spawns new default, resets ammo, sets slot=0
		Char->ResetLoadout();
	}

	// Reset all player positions
	ResetAllPlayers();
}

void AFPSGameMode::ResetAllPlayers()
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) continue;

		if (AFPSCharacter* Char = Cast<AFPSCharacter>(PC->GetPawn()))
		{
			AActor* Start = ChoosePlayerStart(PC);
			if (Start)
			{
				Char->SetActorLocation(Start->GetActorLocation());
				Char->SetActorRotation(Start->GetActorRotation());
			}
			Char->ServerApplyHeal(Char->GetMaxHealth()); // full heal
		}
	}
}

void AFPSGameMode::ResetLevelPickups()
{
	// Destroy all pickups on the map
	for (TActorIterator<AFPSPickup> It(GetWorld()); It; ++It)
	{
		if (*It) (*It)->Destroy();
	}

	// Destroy all dropped weapons (IsOnGround weapons)
	for (TActorIterator<AFPSWeapon> It(GetWorld()); It; ++It)
	{
		if (*It && (*It)->IsOnGround())
		{
			(*It)->Destroy();
		}
	}
}

// ============================================================================
// Utilities
// ============================================================================

int32 AFPSGameMode::GetActivePlayerCount() const
{
	int32 Count = 0;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (It->Get() && !It->Get()->PlayerState->IsOnlyASpectator())
		{
			Count++;
		}
	}
	return Count;
}

void AFPSGameMode::RejectOrSpectatePlayer(APlayerController* PC, const FString& Reason)
{
	UE_LOG(LogTemp, Warning, TEXT("[GameMode] Rejecting/spectating player: %s"), *Reason);

	// Make them a spectator — they'll join next round
	PC->ChangeState(NAME_Spectating);
	PC->ClientGotoState(NAME_Spectating);
}

// ============================================================================
// Player Start selection
// ============================================================================

AActor* AFPSGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	TArray<APlayerStart*> Starts;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		Starts.Add(Cast<APlayerStart>(*It));
	}

	if (Starts.Num() == 0)
	{
		return nullptr;
	}

	// Filter out occupied starts
	TArray<APlayerStart*> FreeStarts;
	for (APlayerStart* Start : Starts)
	{
		bool bOccupied = false;
		for (TActorIterator<APawn> PawnIt(GetWorld()); PawnIt; ++PawnIt)
		{
			if (*PawnIt && (*PawnIt)->GetDistanceTo(Start) < 200.0f)
			{
				bOccupied = true;
				break;
			}
		}
		if (!bOccupied)
		{
			FreeStarts.Add(Start);
		}
	}

	TArray<APlayerStart*>& Pool = FreeStarts.Num() > 0 ? FreeStarts : Starts;
	return Pool[FMath::RandRange(0, Pool.Num() - 1)];
}
