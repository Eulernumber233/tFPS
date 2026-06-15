#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "FPSGameInstance.generated.h"

/**
 * Cross-level game instance.
 * Persists player identity (name, icon) between MainMenu and Gameplay levels.
 * Handles HostGame, JoinGame, ReturnToMainMenu across levels.
 */
UCLASS()
class TFPS_C_API UFPSGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	/** Host a listen server on the given port and travel to the gameplay map. */
	UFUNCTION(BlueprintCallable, Category = "GameInstance")
	void HostGame(int32 Port, const FString& MapName);

	/** Join a remote server at the given address. */
	UFUNCTION(BlueprintCallable, Category = "GameInstance")
	void JoinGame(const FString& Address);

	/** Return to the main menu level (disconnect if connected). */
	UFUNCTION(BlueprintCallable, Category = "GameInstance")
	void ReturnToMainMenu(APlayerController* PC);

	/** Hardcoded default server address (IP:Port). Blueprint can read for "Join Default Server" button. */
	UFUNCTION(BlueprintCallable, Category = "GameInstance")
	FString GetDefaultServerAddress() const { return DefaultServerAddress; }

	/** Player name set in main menu (carried across levels). */
	UPROPERTY(BlueprintReadWrite, Category = "Identity")
	FString PlayerName;

	/** Player icon path set in main menu. */
	UPROPERTY(BlueprintReadWrite, Category = "Identity")
	FString PlayerIconPath;

protected:
	/** Default server address (hardcoded cloud server). */
	UPROPERTY(EditDefaultsOnly, Category = "Network")
	FString DefaultServerAddress = TEXT("127.0.0.1:7777");

	/** Gameplay map name to travel to. */
	UPROPERTY(EditDefaultsOnly, Category = "Network")
	FString GameplayMapName = TEXT("Test_NetFPS");

	/** Main menu map name. */
	UPROPERTY(EditDefaultsOnly, Category = "Network")
	FString MainMenuMapName = TEXT("MainMenuMap");
};
