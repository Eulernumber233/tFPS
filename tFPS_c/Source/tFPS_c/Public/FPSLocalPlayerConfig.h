#pragma once

#include "CoreMinimal.h"

/**
 * Local player identity config (name + icon path).
 * Saved to <Project>/Saved/PlayerConfig.json.
 * Read on main menu open, written when player changes name/icon.
 */
struct TFPS_C_API FFPSLocalPlayerConfig
{
	/** Save player identity to Saved/PlayerConfig.json. */
	static void SaveIdentity(const FString& PlayerName, const FString& IconPath);

	/** Load player identity from Saved/PlayerConfig.json. Returns true if file exists. */
	static bool LoadIdentity(FString& OutPlayerName, FString& OutIconPath);

	static FString GetConfigFilePath();
};
