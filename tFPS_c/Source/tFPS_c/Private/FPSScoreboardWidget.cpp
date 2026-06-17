#if !UE_SERVER

#include "FPSScoreboardWidget.h"
#include "FPSPlayerState.h"
#include "FPSPlayerController.h"
#include "FPSGameState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

void UFPSScoreboardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 创建即拉一次数据（计分板被显示时通常重新创建或重新刷新）。
	RefreshRows();
}

void UFPSScoreboardWidget::SetSortKey(EScoreboardSortKey NewKey)
{
	if (SortKey == NewKey)
		return;

	SortKey = NewKey;
	RefreshRows();
}

void UFPSScoreboardWidget::ToggleSortOrder()
{
	bDescending = !bDescending;
	RefreshRows();
}

void UFPSScoreboardWidget::SetDescending(bool bNewDescending)
{
	if (bDescending == bNewDescending)
		return;

	bDescending = bNewDescending;
	RefreshRows();
}

bool UFPSScoreboardWidget::RowLess(const FScoreboardRow& A, const FScoreboardRow& B) const
{
	// 取当前排序键的比较值。
	auto KeyValue = [this](const FScoreboardRow& R) -> double
	{
		switch (SortKey)
		{
		case EScoreboardSortKey::TotalDamage: return R.TotalDamage;
		case EScoreboardSortKey::CarryValue:  return R.CarryValue;
		case EScoreboardSortKey::Kills:
		default:                              return static_cast<double>(R.Kills);
		}
	};

	const double VA = KeyValue(A);
	const double VB = KeyValue(B);

	// 主键相等时用 Kills 兜底，再用名字保证稳定、确定的顺序（不受平台 sort 实现影响）。
	if (VA != VB)
		return bDescending ? (VA > VB) : (VA < VB);

	if (A.Kills != B.Kills)
		return bDescending ? (A.Kills > B.Kills) : (A.Kills < B.Kills);

	return A.PlayerName < B.PlayerName;
}

void UFPSScoreboardWidget::RefreshRows()
{
	SortedRows.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		OnRowsUpdated();
		return;
	}

	AFPSGameState* GS = World->GetGameState<AFPSGameState>();
	if (!GS)
	{
		OnRowsUpdated();
		return;
	}

	// 本地玩家的 PlayerState（用于标记"我"那一行）。
	APlayerState* LocalPS = nullptr;
	if (APlayerController* PC = GetOwningPlayer())
		LocalPS = PC->PlayerState;

	// 遍历所有玩家（GameState->PlayerArray 在所有端都复制）。
	for (APlayerState* PS : GS->PlayerArray)
	{
		AFPSPlayerState* FPS = Cast<AFPSPlayerState>(PS);
		if (!FPS)
			continue;

		// 跳过纯观察者（无对局数据），避免空行。FPS 当前没有观察者概念，保留判断以防未来加入。
		if (FPS->IsOnlyASpectator())
			continue;

		FScoreboardRow Row;
		Row.PlayerName    = FPS->GetPlayerName();
		Row.Icon          = FPS->PlayerIcon;
		Row.Kills         = FPS->Kills;
		Row.Deaths        = FPS->Deaths;
		Row.TotalDamage   = FPS->TotalDamage;
		Row.CarryValue    = FPS->CarryValue;
		Row.IconIndex     = FPS->IconIndex;
		Row.bIsLocalPlayer = (FPS == LocalPS);

		SortedRows.Add(Row);
	}

	SortedRows.Sort([this](const FScoreboardRow& A, const FScoreboardRow& B)
	{
		return RowLess(A, B);
	});

	// 通知 WBP 重画表格。
	OnRowsUpdated();
}

void UFPSScoreboardWidget::SetPostGameMode(bool bPostGame)
{
	bIsPostGame = bPostGame;
}

void UFPSScoreboardWidget::OnExitClicked()
{
	if (AFPSPlayerController* PC = Cast<AFPSPlayerController>(GetOwningPlayer()))
	{
		PC->ServerNotifyExitClicked();
	}
}

#endif // !UE_SERVER
