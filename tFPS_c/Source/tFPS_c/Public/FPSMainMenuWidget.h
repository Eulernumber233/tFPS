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
	/**  */
	UFUNCTION(BlueprintCallable, Category = "MainMenu")
	void HostGame(int32 Port);

	/** Join a server. IP and Port are combined into "IP:Port" internally. */
	UFUNCTION(BlueprintCallable, Category = "MainMenu")
	void JoinGame(const FString& IP, int32 Port);

	/** Join the default hardcoded server (Join Default Server button). */
	UFUNCTION(BlueprintCallable, Category = "MainMenu")
	void JoinDefaultServer();

	/** Save player name and icon path to local config file. */
	UFUNCTION(BlueprintCallable, Category = "MainMenu")
	void SavePlayerConfig(const FString& PlayerName, const FString& IconPath);

	/** �ӱ��������ļ�����������ƺ�ͼ��·��������ҵ��򷵻�true */
	UFUNCTION(BlueprintCallable, Category = "MainMenu")
	bool LoadPlayerConfig(FString& OutPlayerName, FString& OutIconPath);

	/** Get the default server address from GameInstance. */
	UFUNCTION(BlueprintCallable, Category = "MainMenu")
	FString GetDefaultServerAddress() const;

	/** 关闭应用 */
	UFUNCTION(BlueprintCallable, Category = "MainMenu")
	void QuitGame();
};
