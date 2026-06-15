#include "FPSGameInstance.h"
#include "FPSLocalPlayerConfig.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

void UFPSGameInstance::HostGame(int32 Port, const FString& MapName)
{
	UWorld* World = GetWorld();
	if (!World) return;

	const FString Map = MapName.IsEmpty() ? GameplayMapName : MapName;
	const FString Options = FString::Printf(TEXT("?listen?Port=%d"), Port);

	World->ServerTravel(Map + Options);
}

void UFPSGameInstance::JoinGame(const FString& Address)
{
	APlayerController* PC = GetFirstLocalPlayerController();
	if (!PC) return;

	PC->ClientTravel(Address, TRAVEL_Absolute);
}

void UFPSGameInstance::ReturnToMainMenu(APlayerController* PC)
{
	if (!PC) return;

	UWorld* World = GetWorld();
	if (!World) return;

	// Listen server host: ServerTravel brings all clients back to menu
	if (World->GetNetMode() == NM_ListenServer)
	{
		World->ServerTravel(MainMenuMapName);
	}
	else
	{
		// Client: just disconnect and travel locally
		PC->ClientTravel(MainMenuMapName, TRAVEL_Absolute);
	}
}
