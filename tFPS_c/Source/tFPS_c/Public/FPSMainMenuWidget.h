#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FPSMainMenuWidget.generated.h"

/**
 * Main menu C++ base class.
 * Blueprint inherits this and places buttons, input fields, icon selector.
 *
 * Background: the menu level has a CineCameraActor following a LevelSequence
 * for the animated background. No additional C++ logic needed.
 */
UCLASS()
class TFPS_C_API UFPSMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Host a listen server (Create Room button). Port from input field. */
	UFUNCTION(BlueprintCallable, Category = "MainMenu")
	void HostGame(int32 Port);

	/** Join a server at the given address (IP:Port from input field). */
	UFUNCTION(BlueprintCallable, Category = "MainMenu")
	void JoinGame(const FString& Address);

	/** Join the default hardcoded server (Join Default Server button). */
	UFUNCTION(BlueprintCallable, Category = "MainMenu")
	void JoinDefaultServer();

	/** Save player name and icon path to local config file. */
	UFUNCTION(BlueprintCallable, Category = "MainMenu")
	void SavePlayerConfig(const FString& PlayerName, const FString& IconPath);

	/** Load player name and icon path from local config file. Returns true if found. */
	UFUNCTION(BlueprintCallable, Category = "MainMenu")
	bool LoadPlayerConfig(FString& OutPlayerName, FString& OutIconPath);

	/** Get the default server address from GameInstance. */
	UFUNCTION(BlueprintCallable, Category = "MainMenu")
	FString GetDefaultServerAddress() const;

	/** Quit the application. */
	UFUNCTION(BlueprintCallable, Category = "MainMenu")
	void QuitGame();
};
