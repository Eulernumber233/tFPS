// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"

// 包含参数结构体定义
#include "WallBuilderWallActor.h"
#include "WallBuilderStairActor.h"
#include "WallBuilderRampActor.h"

#include "WallBuilderCommandHistory.generated.h"

/**
 * Actor类型枚举
 */
UENUM(BlueprintType)
enum class EBuildActorType : uint8
{
    None,
    Wall,
    Stair,
    Ramp
};

/**
 * 命令类型枚举
 */
UENUM(BlueprintType)
enum class ECommandType : uint8
{
    None,
    SpawnWall,          // 生成墙体
    SpawnStair,         // 生成楼梯
    SpawnRamp,          // 生成坡道
    MoveAnchor,         // 移动锚点
    DeleteActor,        // 删除Actor
    ModifyActor         // 修改Actor参数
};

/**
 * 命令接口 - 所有可撤回操作的基础接口
 */
UCLASS(Abstract)
class WALLBUILDER_API UWallBuilderCommand : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY()
    ECommandType CommandType = ECommandType::None;

    UPROPERTY()
    FString CommandDescription;

    // 执行命令（首次执行）
    virtual void Execute() {}

    // 撤回命令
    virtual void Undo() {}

    // 重做命令
    virtual void Redo() { Execute(); }

    // 获取命令描述
    virtual FString GetDescription() const { return CommandDescription; }
};

/**
 * 生成墙体命令
 */
UCLASS()
class WALLBUILDER_API USpawnWallCommand : public UWallBuilderCommand
{
    GENERATED_BODY()

public:
    UPROPERTY()
    FWallParameters WallParams;

    UPROPERTY()
    TWeakObjectPtr<AWallBuilderWallActor> SpawnedWall;

    UPROPERTY()
    TWeakObjectPtr<UWorld> TargetWorld;

    virtual void Execute() override;
    virtual void Undo() override;
    virtual void Redo() override;
};

/**
 * 生成楼梯命令
 */
UCLASS()
class WALLBUILDER_API USpawnStairCommand : public UWallBuilderCommand
{
    GENERATED_BODY()

public:
    UPROPERTY()
    FStairParameters StairParams;

    UPROPERTY()
    TWeakObjectPtr<AWallBuilderStairActor> SpawnedStair;

    UPROPERTY()
    TWeakObjectPtr<UWorld> TargetWorld;

    virtual void Execute() override;
    virtual void Undo() override;
    virtual void Redo() override;
};

/**
 * 生成坡道命令
 */
UCLASS()
class WALLBUILDER_API USpawnRampCommand : public UWallBuilderCommand
{
    GENERATED_BODY()

public:
    UPROPERTY()
    FRampParameters RampParams;

    UPROPERTY()
    TWeakObjectPtr<AWallBuilderRampActor> SpawnedRamp;

    UPROPERTY()
    TWeakObjectPtr<UWorld> TargetWorld;

    virtual void Execute() override;
    virtual void Undo() override;
    virtual void Redo() override;
};

/**
 * 移动锚点命令（影响多个Actor）
 */
UCLASS()
class WALLBUILDER_API UMoveAnchorCommand : public UWallBuilderCommand
{
    GENERATED_BODY()

public:
    UPROPERTY()
    FVector OriginalPosition;

    UPROPERTY()
    FVector NewPosition;

    // 受影响的Actor列表（存储参数用于撤回）
    UPROPERTY()
    TArray<TWeakObjectPtr<AActor>> AffectedActors;

    // 每个Actor的原始参数
    UPROPERTY()
    TArray<FWallParameters> OriginalWallParams;

    UPROPERTY()
    TArray<FStairParameters> OriginalStairParams;

    UPROPERTY()
    TArray<FRampParameters> OriginalRampParams;

    virtual void Execute() override;
    virtual void Undo() override;
    virtual void Redo() override;

    // 初始化命令，收集受影响Actor的当前状态
    void Initialize(const FVector& InOriginalPos, const FVector& InNewPos, UWorld* World);
};

/**
 * 删除Actor命令
 */
UCLASS()
class WALLBUILDER_API UDeleteActorCommand : public UWallBuilderCommand
{
    GENERATED_BODY()

public:
    UPROPERTY()
    FWallParameters DeletedWallParams;

    UPROPERTY()
    FStairParameters DeletedStairParams;

    UPROPERTY()
    FRampParameters DeletedRampParams;

    UPROPERTY()
    EBuildActorType ActorType = EBuildActorType::None;

    UPROPERTY()
    TWeakObjectPtr<UWorld> TargetWorld;

    virtual void Execute() override;
    virtual void Undo() override;
    virtual void Redo() override;
};

/**
 * 命令历史管理器 - 单例模式管理所有撤回重做
 */
UCLASS()
class WALLBUILDER_API UWallBuilderCommandHistory : public UObject
{
    GENERATED_BODY()

public:
    // 获取单例实例
    static UWallBuilderCommandHistory* Get();

    // 执行并记录命令
    void ExecuteCommand(UWallBuilderCommand* Command);

    // 撤回
    bool Undo();

    // 重做
    bool Redo();

    // 是否可以撤回
    bool CanUndo() const;

    // 是否可以重做
    bool CanRedo() const;

    // 获取下一个撤回命令的描述
    FString GetUndoDescription() const;

    // 获取下一个重做命令的描述
    FString GetRedoDescription() const;

    // 清空历史
    void ClearHistory();

    // 设置最大历史记录数
    void SetMaxHistorySize(int32 MaxSize) { MaxHistorySize = MaxSize; }

private:
    UPROPERTY()
    TArray<UWallBuilderCommand*> UndoStack;

    UPROPERTY()
    TArray<UWallBuilderCommand*> RedoStack;

    int32 MaxHistorySize = 50;

    // 当前命令索引（用于优化）
    int32 CurrentIndex = -1;

    void TrimHistory();
};
