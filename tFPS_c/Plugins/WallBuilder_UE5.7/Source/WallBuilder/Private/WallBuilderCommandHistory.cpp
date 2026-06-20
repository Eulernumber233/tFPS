// Copyright Epic Games, Inc. All Rights Reserved.

#include "WallBuilderCommandHistory.h"
#include "WallBuilderWallActor.h"
#include "WallBuilderStairActor.h"
#include "WallBuilderRampActor.h"
#include "Engine/World.h"

// 单例实例
static UWallBuilderCommandHistory* GCommandHistory = nullptr;

UWallBuilderCommandHistory* UWallBuilderCommandHistory::Get()
{
    if (!GCommandHistory)
    {
        GCommandHistory = NewObject<UWallBuilderCommandHistory>();
        GCommandHistory->AddToRoot(); // 防止被垃圾回收
    }
    return GCommandHistory;
}

void UWallBuilderCommandHistory::ExecuteCommand(UWallBuilderCommand* Command)
{
    if (!Command) return;

    // 执行命令
    Command->Execute();

    // 添加到撤回栈
    UndoStack.Add(Command);

    // 清空重做栈（新操作后不能重做之前的）
    RedoStack.Empty();

    // 限制历史记录大小
    TrimHistory();
}

bool UWallBuilderCommandHistory::Undo()
{
    if (!CanUndo()) return false;

    // 取出最后一个命令
    UWallBuilderCommand* Command = UndoStack.Last();
    UndoStack.Pop();

    // 执行撤回
    Command->Undo();

    // 添加到重做栈
    RedoStack.Add(Command);

    return true;
}

bool UWallBuilderCommandHistory::Redo()
{
    if (!CanRedo()) return false;

    // 取出最后一个重做命令
    UWallBuilderCommand* Command = RedoStack.Last();
    RedoStack.Pop();

    // 执行重做
    Command->Redo();

    // 添加回撤回栈
    UndoStack.Add(Command);

    return true;
}

bool UWallBuilderCommandHistory::CanUndo() const
{
    return UndoStack.Num() > 0;
}

bool UWallBuilderCommandHistory::CanRedo() const
{
    return RedoStack.Num() > 0;
}

FString UWallBuilderCommandHistory::GetUndoDescription() const
{
    if (!CanUndo()) return TEXT("");
    return UndoStack.Last()->GetDescription();
}

FString UWallBuilderCommandHistory::GetRedoDescription() const
{
    if (!CanRedo()) return TEXT("");
    return RedoStack.Last()->GetDescription();
}

void UWallBuilderCommandHistory::ClearHistory()
{
    UndoStack.Empty();
    RedoStack.Empty();
}

void UWallBuilderCommandHistory::TrimHistory()
{
    while (UndoStack.Num() > MaxHistorySize)
    {
        UndoStack.RemoveAt(0);
    }
}

// ============================================================================
// USpawnWallCommand
// ============================================================================

void USpawnWallCommand::Execute()
{
    if (!TargetWorld.IsValid()) return;

    FVector Center = (WallParams.StartPoint + WallParams.EndPoint) * 0.5f;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    SpawnedWall = TargetWorld->SpawnActor<AWallBuilderWallActor>(Center, FRotator::ZeroRotator, SpawnParams);

    if (SpawnedWall.IsValid())
    {
        SpawnedWall->SetWallParameters(WallParams);
        CommandDescription = TEXT("Spawn Wall");
    }
}

void USpawnWallCommand::Undo()
{
    if (SpawnedWall.IsValid())
    {
        SpawnedWall->Destroy();
        SpawnedWall = nullptr;
    }
}

void USpawnWallCommand::Redo()
{
    // 重新生成墙体
    Execute();
}

// ============================================================================
// USpawnStairCommand
// ============================================================================

void USpawnStairCommand::Execute()
{
    if (!TargetWorld.IsValid()) return;

    FVector Center = (StairParams.StartPoint + StairParams.EndPoint) * 0.5f;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    SpawnedStair = TargetWorld->SpawnActor<AWallBuilderStairActor>(Center, FRotator::ZeroRotator, SpawnParams);

    if (SpawnedStair.IsValid())
    {
        SpawnedStair->SetStairParameters(StairParams);
        CommandDescription = TEXT("Spawn Stair");
    }
}

void USpawnStairCommand::Undo()
{
    if (SpawnedStair.IsValid())
    {
        SpawnedStair->Destroy();
        SpawnedStair = nullptr;
    }
}

void USpawnStairCommand::Redo()
{
    Execute();
}

// ============================================================================
// USpawnRampCommand
// ============================================================================

void USpawnRampCommand::Execute()
{
    if (!TargetWorld.IsValid()) return;

    FVector Center = (RampParams.StartPoint + RampParams.EndPoint) * 0.5f;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    SpawnedRamp = TargetWorld->SpawnActor<AWallBuilderRampActor>(Center, FRotator::ZeroRotator, SpawnParams);

    if (SpawnedRamp.IsValid())
    {
        SpawnedRamp->SetRampParameters(RampParams);
        CommandDescription = TEXT("Spawn Ramp");
    }
}

void USpawnRampCommand::Undo()
{
    if (SpawnedRamp.IsValid())
    {
        SpawnedRamp->Destroy();
        SpawnedRamp = nullptr;
    }
}

void USpawnRampCommand::Redo()
{
    Execute();
}

// ============================================================================
// UMoveAnchorCommand
// ============================================================================

void UMoveAnchorCommand::Initialize(const FVector& InOriginalPos, const FVector& InNewPos, UWorld* World)
{
    OriginalPosition = InOriginalPos;
    NewPosition = InNewPos;

    if (!World) return;

    // 收集所有受影响的Actor
    for (AActor* Actor : World->PersistentLevel->Actors)
    {
        if (!Actor || !IsValid(Actor)) continue;

        if (AWallBuilderWallActor* Wall = Cast<AWallBuilderWallActor>(Actor))
        {
            const FWallParameters& Params = Wall->GetWallParameters();
            if (Params.StartPoint.Equals(OriginalPosition) || Params.EndPoint.Equals(OriginalPosition))
            {
                AffectedActors.Add(Wall);
                OriginalWallParams.Add(Params);
            }
        }
        else if (AWallBuilderStairActor* Stair = Cast<AWallBuilderStairActor>(Actor))
        {
            const FStairParameters& Params = Stair->GetStairParameters();
            if (Params.StartPoint.Equals(OriginalPosition) || Params.EndPoint.Equals(OriginalPosition))
            {
                AffectedActors.Add(Stair);
                OriginalStairParams.Add(Params);
            }
        }
        else if (AWallBuilderRampActor* Ramp = Cast<AWallBuilderRampActor>(Actor))
        {
            const FRampParameters& Params = Ramp->GetRampParameters();
            if (Params.StartPoint.Equals(OriginalPosition) || Params.EndPoint.Equals(OriginalPosition))
            {
                AffectedActors.Add(Ramp);
                OriginalRampParams.Add(Params);
            }
        }
    }

    CommandDescription = TEXT("Move Anchor");
}

void UMoveAnchorCommand::Execute()
{
    // Execute 不做任何事情，因为 Actor 已经在拖拽过程中移动了
    // 只有 Redo 时才需要真正执行移动
}

void UMoveAnchorCommand::Undo()
{
    // 恢复所有受影响Actor的原始位置
    for (int32 i = 0; i < AffectedActors.Num(); i++)
    {
        TWeakObjectPtr<AActor> Actor = AffectedActors[i];
        if (!Actor.IsValid()) continue;

        if (i < OriginalWallParams.Num())
        {
            if (AWallBuilderWallActor* Wall = Cast<AWallBuilderWallActor>(Actor.Get()))
            {
                const FWallParameters& Params = OriginalWallParams[i];
                FVector Center = (Params.StartPoint + Params.EndPoint) * 0.5f;
                Wall->SetActorLocation(Center);
                Wall->SetWallParameters(Params);
            }
        }
        else if ((i - OriginalWallParams.Num()) < OriginalStairParams.Num())
        {
            int32 StairIdx = i - OriginalWallParams.Num();
            if (AWallBuilderStairActor* Stair = Cast<AWallBuilderStairActor>(Actor.Get()))
            {
                const FStairParameters& Params = OriginalStairParams[StairIdx];
                FVector Center = (Params.StartPoint + Params.EndPoint) * 0.5f;
                Stair->SetActorLocation(Center);
                Stair->SetStairParameters(Params);
            }
        }
        else if ((i - OriginalWallParams.Num() - OriginalStairParams.Num()) < OriginalRampParams.Num())
        {
            int32 RampIdx = i - OriginalWallParams.Num() - OriginalStairParams.Num();
            if (AWallBuilderRampActor* Ramp = Cast<AWallBuilderRampActor>(Actor.Get()))
            {
                const FRampParameters& Params = OriginalRampParams[RampIdx];
                FVector Center = (Params.StartPoint + Params.EndPoint) * 0.5f;
                Ramp->SetActorLocation(Center);
                Ramp->SetRampParameters(Params);
            }
        }
    }
}

void UMoveAnchorCommand::Redo()
{
    // 重新应用移动
    for (int32 i = 0; i < AffectedActors.Num(); i++)
    {
        TWeakObjectPtr<AActor> Actor = AffectedActors[i];
        if (!Actor.IsValid()) continue;

        if (i < OriginalWallParams.Num())
        {
            if (AWallBuilderWallActor* Wall = Cast<AWallBuilderWallActor>(Actor.Get()))
            {
                FWallParameters Params = OriginalWallParams[i];
                if (Params.StartPoint.Equals(OriginalPosition))
                    Params.StartPoint = NewPosition;
                if (Params.EndPoint.Equals(OriginalPosition))
                    Params.EndPoint = NewPosition;

                FVector Center = (Params.StartPoint + Params.EndPoint) * 0.5f;
                Wall->SetActorLocation(Center);
                Wall->SetWallParameters(Params);
            }
        }
        else if ((i - OriginalWallParams.Num()) < OriginalStairParams.Num())
        {
            int32 StairIdx = i - OriginalWallParams.Num();
            if (AWallBuilderStairActor* Stair = Cast<AWallBuilderStairActor>(Actor.Get()))
            {
                FStairParameters Params = OriginalStairParams[StairIdx];
                if (Params.StartPoint.Equals(OriginalPosition))
                    Params.StartPoint = NewPosition;
                if (Params.EndPoint.Equals(OriginalPosition))
                    Params.EndPoint = NewPosition;

                FVector Center = (Params.StartPoint + Params.EndPoint) * 0.5f;
                Stair->SetActorLocation(Center);
                Stair->SetStairParameters(Params);
            }
        }
        else if ((i - OriginalWallParams.Num() - OriginalStairParams.Num()) < OriginalRampParams.Num())
        {
            int32 RampIdx = i - OriginalWallParams.Num() - OriginalStairParams.Num();
            if (AWallBuilderRampActor* Ramp = Cast<AWallBuilderRampActor>(Actor.Get()))
            {
                FRampParameters Params = OriginalRampParams[RampIdx];
                if (Params.StartPoint.Equals(OriginalPosition))
                    Params.StartPoint = NewPosition;
                if (Params.EndPoint.Equals(OriginalPosition))
                    Params.EndPoint = NewPosition;

                FVector Center = (Params.StartPoint + Params.EndPoint) * 0.5f;
                Ramp->SetActorLocation(Center);
                Ramp->SetRampParameters(Params);
            }
        }
    }
}

// ============================================================================
// UDeleteActorCommand
// ============================================================================

void UDeleteActorCommand::Execute()
{
    // 删除操作在创建命令时已经完成，这里不需要额外操作
    CommandDescription = TEXT("Delete Actor");
}

void UDeleteActorCommand::Undo()
{
    if (!TargetWorld.IsValid()) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    switch (ActorType)
    {
    case EBuildActorType::Wall:
    {
        FVector Center = (DeletedWallParams.StartPoint + DeletedWallParams.EndPoint) * 0.5f;
        if (AWallBuilderWallActor* Wall = TargetWorld->SpawnActor<AWallBuilderWallActor>(Center, FRotator::ZeroRotator, SpawnParams))
        {
            Wall->SetWallParameters(DeletedWallParams);
        }
        break;
    }
    case EBuildActorType::Stair:
    {
        FVector Center = (DeletedStairParams.StartPoint + DeletedStairParams.EndPoint) * 0.5f;
        if (AWallBuilderStairActor* Stair = TargetWorld->SpawnActor<AWallBuilderStairActor>(Center, FRotator::ZeroRotator, SpawnParams))
        {
            Stair->SetStairParameters(DeletedStairParams);
        }
        break;
    }
    case EBuildActorType::Ramp:
    {
        FVector Center = (DeletedRampParams.StartPoint + DeletedRampParams.EndPoint) * 0.5f;
        if (AWallBuilderRampActor* Ramp = TargetWorld->SpawnActor<AWallBuilderRampActor>(Center, FRotator::ZeroRotator, SpawnParams))
        {
            Ramp->SetRampParameters(DeletedRampParams);
        }
        break;
    }
    default:
        break;
    }
}

void UDeleteActorCommand::Redo()
{
    // 重新删除 - 查找匹配的Actor并删除
    if (!TargetWorld.IsValid()) return;

    for (AActor* Actor : TargetWorld->PersistentLevel->Actors)
    {
        if (!Actor || !IsValid(Actor)) continue;

        switch (ActorType)
        {
        case EBuildActorType::Wall:
            if (AWallBuilderWallActor* Wall = Cast<AWallBuilderWallActor>(Actor))
            {
                const FWallParameters& Params = Wall->GetWallParameters();
                if (Params.StartPoint.Equals(DeletedWallParams.StartPoint) &&
                    Params.EndPoint.Equals(DeletedWallParams.EndPoint))
                {
                    Wall->Destroy();
                    return;
                }
            }
            break;
        case EBuildActorType::Stair:
            if (AWallBuilderStairActor* Stair = Cast<AWallBuilderStairActor>(Actor))
            {
                const FStairParameters& Params = Stair->GetStairParameters();
                if (Params.StartPoint.Equals(DeletedStairParams.StartPoint) &&
                    Params.EndPoint.Equals(DeletedStairParams.EndPoint))
                {
                    Stair->Destroy();
                    return;
                }
            }
            break;
        case EBuildActorType::Ramp:
            if (AWallBuilderRampActor* Ramp = Cast<AWallBuilderRampActor>(Actor))
            {
                const FRampParameters& Params = Ramp->GetRampParameters();
                if (Params.StartPoint.Equals(DeletedRampParams.StartPoint) &&
                    Params.EndPoint.Equals(DeletedRampParams.EndPoint))
                {
                    Ramp->Destroy();
                    return;
                }
            }
            break;
        default:
            break;
        }
    }
}
