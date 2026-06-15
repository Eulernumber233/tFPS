#if !UE_SERVER

#include "FPSMainMenuWidget.h"
#include "FPSGameInstance.h"
#include "FPSLocalPlayerConfig.h"
#include "Kismet/GameplayStatics.h"
#include "GenericPlatform/GenericPlatformMisc.h"

void UFPSMainMenuWidget::HostGame(int32 Port)
{
	if (UFPSGameInstance* GI = Cast<UFPSGameInstance>(GetGameInstance()))
	{
		GI->HostGame(Port, TEXT(""));
	}
}

void UFPSMainMenuWidget::JoinGame(const FString& Address)
{
	if (UFPSGameInstance* GI = Cast<UFPSGameInstance>(GetGameInstance()))
	{
		GI->JoinGame(Address);
	}
}

void UFPSMainMenuWidget::JoinDefaultServer()
{
	if (UFPSGameInstance* GI = Cast<UFPSGameInstance>(GetGameInstance()))
	{
		GI->JoinGame(GI->GetDefaultServerAddress());
	}
}

void UFPSMainMenuWidget::SavePlayerConfig(const FString& PlayerName, const FString& IconPath)
{
	FFPSLocalPlayerConfig::SaveIdentity(PlayerName, IconPath);

	// Also store in GameInstance for cross-level carry
	if (UFPSGameInstance* GI = Cast<UFPSGameInstance>(GetGameInstance()))
	{
		GI->PlayerName = PlayerName;
		GI->PlayerIconPath = IconPath;
	}
}

bool UFPSMainMenuWidget::LoadPlayerConfig(FString& OutPlayerName, FString& OutIconPath)
{
	return FFPSLocalPlayerConfig::LoadIdentity(OutPlayerName, OutIconPath);
}

FString UFPSMainMenuWidget::GetDefaultServerAddress() const
{
	if (const UFPSGameInstance* GI = Cast<UFPSGameInstance>(GetGameInstance()))
	{
		return GI->GetDefaultServerAddress();
	}
	return TEXT("127.0.0.1:7777");
}

void UFPSMainMenuWidget::QuitGame()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->ConsoleCommand("quit");
	}
}

#endif // !UE_SERVER
