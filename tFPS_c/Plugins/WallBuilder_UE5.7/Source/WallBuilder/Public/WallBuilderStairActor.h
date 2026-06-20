// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "WallBuilderStairActor.generated.h"

/**
 * 楼梯参数结构体
 */
USTRUCT(BlueprintType)
struct FStairParameters
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stair")
	FVector StartPoint = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stair")
	FVector EndPoint = FVector(100.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stair")
	float Width = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stair")
	float TotalHeight = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stair")
	int32 StepCount = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stair")
	UMaterialInterface* StairMaterial = nullptr;
};

/**
 * 楼梯 Actor - 两点绘制生成楼梯
 */
UCLASS(BlueprintType)
class WALLBUILDER_API AWallBuilderStairActor : public AActor
{
	GENERATED_BODY()

public:
	AWallBuilderStairActor();

	UFUNCTION(BlueprintCallable, Category = "Stair")
	void SetStairParameters(const FStairParameters& InParams);

	UFUNCTION(BlueprintCallable, Category = "Stair")
	const FStairParameters& GetStairParameters() const { return StairParams; }

	/** 设置是否在 RebuildMesh 时创建物理碰撞（预览Actor设为false） */
	void SetCreateCollision(bool bCreate) { bCreatePhysicsCollision = bCreate; }

	/** 重建楼梯网格 */
	UFUNCTION(BlueprintCallable, Category = "Stair")
	void RebuildMesh();

	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditMove(bool bFinished) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stair")
	class UBoxComponent* CollisionBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stair")
	class UProceduralMeshComponent* StairMesh;

	UPROPERTY(EditAnywhere, Category = "Stair")
	FStairParameters StairParams;

	bool bCreatePhysicsCollision = true;

	/** 更新碰撞盒以匹配网格包围盒 */
	void UpdateCollisionBox();
};
