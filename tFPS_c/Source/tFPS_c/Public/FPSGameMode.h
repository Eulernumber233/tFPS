#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "FPSGameMode.generated.h"

UCLASS()
class TFPS_C_API AFPSGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	AFPSGameMode();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 玩家登入：分配默认名 client1/client2/...（计分板/UI 用，后续登录系统可覆盖）。 */
	virtual void PostLogin(APlayerController* NewPlayer) override;

	/** Called by AFPSCharacter::Die — schedules a 3-second respawn */
	void OnPlayerDied(AController* Victim, AController* Killer);

protected:
	/** Match length in seconds (default 5 minutes) */
	UPROPERTY(EditDefaultsOnly, Category = "Match")
	float MatchDuration = 300.0f;

	/** Begin the playing phase */
	void StartMatch();

	/** End the match and declare a winner */
	void EndMatch();

	/** Decrement the match timer every second */
	void UpdateTimer();

	/** Respawn a specific player */
	void RespawnPlayer(AController* Player);

	/** Pick a random unoccupied player start */
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	FTimerHandle MatchEndTimerHandle;
	FTimerHandle OneSecondTimerHandle;

	/** 已登入玩家计数，用于生成 client1/client2/... 默认名（服务端）。 */
	int32 PlayerJoinCount = 0;
};
