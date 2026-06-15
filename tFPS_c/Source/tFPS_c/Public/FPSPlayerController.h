#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "FPSPlayerController.generated.h"

class UFPSScoreboardWidget;
class UFPSInGameMenuWidget;
class UUserWidget;
class UInputMappingContext;
class UInputAction;

UCLASS()
class TFPS_C_API AFPSPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** Server→Client RPC: notify client to prepare for respawn */
	UFUNCTION(Client, Reliable)
	void ClientRespawn();

	/** Server→Client RPC: force-show post-game scoreboard with exit button. */
	UFUNCTION(Client, Reliable)
	void ClientShowPostGameScoreboard();

	/** Client→Server RPC: player clicked "Exit" on post-game scoreboard. */
	UFUNCTION(Server, Reliable)
	void ServerNotifyExitClicked();

	/** Client→Server RPC: send desired player name on connect. */
	UFUNCTION(Server, Reliable)
	void ServerSetPlayerIdentity(const FString& DesiredName);

	/** Whether scoreboard is currently open (Tab held). */
	UFUNCTION(BlueprintCallable, Category = "Scoreboard")
	bool IsScoreboardOpen() const { return bScoreboardOpen; }

	UFUNCTION(BlueprintCallable, Category = "Scoreboard")
	bool IsScoreboardMouseLocked() const { return bScoreboardOpen && bScoreboardMouseLocked; }

	UFUNCTION(BlueprintCallable, Category = "Scoreboard")
	void ToggleScoreboardMouseLock();

	/** Show ESC in-game menu (overlay with Continue / Exit Room). */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void ShowInGameMenu();

	/** Hide ESC in-game menu. */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	void HideInGameMenu();

	/** Whether the in-game menu is currently open. */
	UFUNCTION(BlueprintCallable, Category = "Menu")
	bool IsInGameMenuOpen() const { return bInGameMenuOpen; }

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ---- Scoreboard (Tab, local UI) ----

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scoreboard")
	TSubclassOf<UUserWidget> ScoreboardClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scoreboard|Input")
	FKey ScoreboardKey = EKeys::Tab;

	void OnScoreboardPressed();
	void OnScoreboardReleased();

	// ---- In-game menu (ESC) ----

	/** ESC menu WBP class (must inherit UFPSInGameMenuWidget). Set in Blueprint. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Menu")
	TSubclassOf<UFPSInGameMenuWidget> InGameMenuClass;

	/** ESC key for toggling in-game menu (default Escape). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Menu|Input")
	FKey InGameMenuKey = EKeys::Escape;

	/** Whether the in-game menu is open. */
	bool bInGameMenuOpen = false;

	/** Physical key state tracking for ESC edge detection. */
	bool bInGameMenuKeyWasDown = false;

private:
	UPROPERTY()
	TObjectPtr<UFPSScoreboardWidget> ScoreboardWidget = nullptr;

	bool bScoreboardOpen = false;
	bool bScoreboardMouseLocked = false;
	bool bScoreboardKeyWasDown = false;

	void SetScoreboardMouseLocked(bool bLock);

	/** In-game menu widget instance (lazy-created, reused). */
	UPROPERTY()
	TObjectPtr<UFPSInGameMenuWidget> InGameMenuWidget = nullptr;

	/** Post-game scoreboard widget instance. */
	UPROPERTY()
	TObjectPtr<UFPSScoreboardWidget> PostGameScoreboardWidget = nullptr;
};
