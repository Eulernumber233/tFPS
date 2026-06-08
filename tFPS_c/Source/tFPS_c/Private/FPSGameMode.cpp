#include "FPSGameMode.h"
#include "FPSGameState.h"
#include "FPSPlayerState.h"
#include "FPSCharacter.h"
#include "FPSPlayerController.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "EngineUtils.h"

AFPSGameMode::AFPSGameMode()
{
	// Wire up default classes — overridable via Blueprint child
	GameStateClass = AFPSGameState::StaticClass();
	PlayerStateClass = AFPSPlayerState::StaticClass();
	PlayerControllerClass = AFPSPlayerController::StaticClass();
	DefaultPawnClass = AFPSCharacter::StaticClass();
}

void AFPSGameMode::BeginPlay()
{
	Super::BeginPlay();

	// Warmup period, then start the match
	FTimerHandle StartTimerHandle;
	GetWorldTimerManager().SetTimer(StartTimerHandle,
		FTimerDelegate::CreateUObject(this, &AFPSGameMode::StartMatch), 3.0f, false);
}

void AFPSGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// 分配默认名 client1/client2/...（计分板显示用）。后续登录系统可用
	// APlayerState::SetPlayerName 覆盖。++PlayerJoinCount 从 1 起算。
	if (NewPlayer)
	{
		if (APlayerState* PS = NewPlayer->PlayerState)
		{
			const FString DefaultName = FString::Printf(TEXT("client%d"), ++PlayerJoinCount);
			PS->SetPlayerName(DefaultName);
		}
	}
}

void AFPSGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(OneSecondTimerHandle);
	GetWorldTimerManager().ClearTimer(MatchEndTimerHandle);
	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------
// Match flow
// ---------------------------------------------------------------------------

void AFPSGameMode::StartMatch()
{
	AFPSGameState* GS = GetGameState<AFPSGameState>();
	if (GS)
	{
		GS->MatchStage = EMatchStage::Playing;
		GS->TimeRemaining = MatchDuration;
	}

	// One-second countdown tick
	GetWorldTimerManager().SetTimer(OneSecondTimerHandle,
		FTimerDelegate::CreateUObject(this, &AFPSGameMode::UpdateTimer), 1.0f, true);

	// End match after MatchDuration seconds
	GetWorldTimerManager().SetTimer(MatchEndTimerHandle,
		FTimerDelegate::CreateUObject(this, &AFPSGameMode::EndMatch), MatchDuration, false);

	// Respawn any players who were waiting in the warmup
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && PC->GetPawn() == nullptr)
		{
			RestartPlayer(PC);
		}
	}
}

void AFPSGameMode::EndMatch()
{
	GetWorldTimerManager().ClearTimer(OneSecondTimerHandle);
	GetWorldTimerManager().ClearTimer(MatchEndTimerHandle);

	AFPSGameState* GS = GetGameState<AFPSGameState>();
	if (GS)
	{
		GS->MatchStage = EMatchStage::Ended;
		GS->TimeRemaining = 0.0f;
	}

	// Determine the winner
	AFPSPlayerState* Best = nullptr;
	int32 BestKills = -1;

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC)
		{
			if (AFPSPlayerState* PS = PC->GetPlayerState<AFPSPlayerState>())
			{
				if (PS->Kills > BestKills)
				{
					BestKills = PS->Kills;
					Best = PS;
				}
			}

			// Disable input for all players
			PC->SetIgnoreMoveInput(true);
			PC->SetIgnoreLookInput(true);
		}
	}

	if (Best)
	{
		UE_LOG(LogTemp, Log, TEXT(">>>>> Match Over! Winner: %s with %d kills <<<<<"),
			*Best->GetPlayerName(), Best->Kills);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT(">>>>> Match Over! No winner <<<<<"));
	}
}

void AFPSGameMode::UpdateTimer()
{
	AFPSGameState* GS = GetGameState<AFPSGameState>();
	if (GS && GS->MatchStage == EMatchStage::Playing)
	{
		GS->TimeRemaining = FMath::Max(0.0f, GS->TimeRemaining - 1.0f);
	}
}

// ---------------------------------------------------------------------------
// Death & Respawn
// ---------------------------------------------------------------------------

void AFPSGameMode::OnPlayerDied(AController* Victim, AController* Killer)
{
	if (!Victim)
		return;

	// Notify the client that death happened (plays UI / sound)
	if (AFPSPlayerController* PC = Cast<AFPSPlayerController>(Victim))
	{
		PC->ClientRespawn();
	}

	// 复活延迟从角色身上读取（蓝图可调，留参数），死亡相机在这段时间播放。
	float RespawnDelay = 2.0f;
	if (AFPSCharacter* Char = Cast<AFPSCharacter>(Victim->GetPawn()))
		RespawnDelay = Char->GetRespawnDelay();

	// Schedule respawn after the delay. 不销毁 Pawn —— 沿用同一个角色，复活时传送复位。
	FTimerHandle RespawnTimerHandle;
	GetWorldTimerManager().SetTimer(RespawnTimerHandle,
		FTimerDelegate::CreateLambda([this, Victim]() { RespawnPlayer(Victim); }),
		RespawnDelay, false);
}

void AFPSGameMode::RespawnPlayer(AController* Player)
{
	// Don't respawn if match ended while the timer was ticking
	AFPSGameState* GS = GetGameState<AFPSGameState>();
	if (GS && GS->MatchStage != EMatchStage::Playing)
		return;

	if (!IsValid(Player))
		return;

	AFPSCharacter* Char = Cast<AFPSCharacter>(Player->GetPawn());
	if (!Char)
	{
		// 没有 Pawn（例如热身阶段从未生成过）—— 退回标准生成。
		RestartPlayer(Player);
		return;
	}

	// 选随机出生点，把现有 Pawn 传送过去并复位（不销毁重建）。
	AActor* Start = ChoosePlayerStart(Player);
	const FVector Loc = Start ? Start->GetActorLocation() : Char->GetActorLocation();
	const FRotator Rot = Start ? Start->GetActorRotation() : Char->GetActorRotation();
	Char->Respawn(Loc, Rot);
}

// ---------------------------------------------------------------------------
// Player Start selection
// ---------------------------------------------------------------------------

AActor* AFPSGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	// Gather all player starts
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

	// Pick from free starts if any, otherwise any start
	TArray<APlayerStart*>& Pool = FreeStarts.Num() > 0 ? FreeStarts : Starts;
	return Pool[FMath::RandRange(0, Pool.Num() - 1)];
}
