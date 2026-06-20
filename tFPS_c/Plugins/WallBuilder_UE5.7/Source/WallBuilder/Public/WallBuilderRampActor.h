// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "WallBuilderRampActor.generated.h"

USTRUCT(BlueprintType)
struct FRampParameters
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ramp")
	FVector StartPoint = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ramp")
	FVector EndPoint = FVector(100.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ramp")
	float Width = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ramp")
	float StartHeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ramp")
	float EndHeight = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ramp")
	UMaterialInterface* RampMaterial = nullptr;
};

/**
 * 斜坡 Actor - 两点绘制生成斜坡
 */
UCLASS(BlueprintType)
class WALLBUILDER_API AWallBuilderRampActor : public AActor
{
	GENERATED_BODY()

public:
	AWallBuilderRampActor();

	UFUNCTION(BlueprintCallable, Category = "Ramp")
	void SetRampParameters(const FRampParameters& InParams);

	UFUNCTION(BlueprintCallable, Category = "Ramp")
	const FRampParameters& GetRampParameters() const { return RampParams; }
/** 设置是否在 RebuildMesh 时创建物理碰撞（预览Actor设为false） */
	void SetCreateCollision(bool bCreate) { bCreatePhysicsCollision = bCreate; }

	/** 重建斜坡网格 */
	UFUNCTION(BlueprintCallable, Category = "Ramp")
	void RebuildMesh();

	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditMove(bool bFinished) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ramp")
	class UBoxComponent* CollisionBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ramp")
	class UProceduralMeshComponent* RampMesh;

	UPROPERTY(EditAnywhere, Category = "Ramp")
	FRampParameters RampParams;

	bool bCreatePhysicsCollision = true;

	/** 更新碰撞盒以匹配网格包围盒 */
	void UpdateCollisionBox();
};
