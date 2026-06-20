// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "WallBuilderWallActor.generated.h"

/**
 * 墙体参数结构体
 */
USTRUCT(BlueprintType)
struct FWallParameters
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall")
	float Height = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall")
	float Thickness = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall")
	FVector StartPoint = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall")
	FVector EndPoint = FVector(100.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall")
	UMaterialInterface* WallMaterial = nullptr;

	// === 墙角斜切参数 ===
	// 起始端斜切角度（度），0=垂直，正值=向外斜切，负值=向内斜切
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Corner")
	float StartMiterAngle = 0.0f;

	// 末端斜切角度（度）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Corner")
	float EndMiterAngle = 0.0f;

	// 是否启用自动墙角衔接
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Corner")
	bool bAutoMiter = true;
};

/**
 * 墙体开洞结构体
 */
USTRUCT(BlueprintType)
struct FWallOpening
{
	GENERATED_BODY()

	/** 洞中心沿墙体方向距 StartPoint 的距离 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Opening")
	float Position = 0.0f;

	/** 洞口宽度 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Opening")
	float Width = 90.0f;

	/** 洞口高度 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Opening")
	float Height = 210.0f;

	/** 洞口底部距地面高度 (cm)，0 = 落地的门洞 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Opening")
	float ZOffset = 0.0f;
};

/**
 * 墙体Actor - 支持开洞
 */
UCLASS(BlueprintType)
class WALLBUILDER_API AWallBuilderWallActor : public AActor
{
	GENERATED_BODY()

public:
	AWallBuilderWallActor();

	UFUNCTION(BlueprintCallable, Category = "Wall")
	void SetWallParameters(const FWallParameters& InParams);

	UFUNCTION(BlueprintCallable, Category = "Wall")
	const FWallParameters& GetWallParameters() const { return WallParams; }

	// === 开洞操作 ===
	UFUNCTION(BlueprintCallable, Category = "Opening")
	void AddOpening(const FWallOpening& Opening);

	UFUNCTION(BlueprintCallable, Category = "Opening")
	void RemoveOpening(int32 Index);

	UFUNCTION(BlueprintCallable, Category = "Opening")
	void UpdateOpening(int32 Index, const FWallOpening& NewOpening);

	UFUNCTION(BlueprintCallable, Category = "Opening")
	void ClearOpenings();

	UFUNCTION(BlueprintCallable, Category = "Opening")
	const TArray<FWallOpening>& GetOpenings() const { return Openings; }

	/** 重建网格（有洞用 ProceduralMesh，无洞用 StaticMesh） */
	UFUNCTION(BlueprintCallable, Category = "Opening")
	void RebuildMesh();

	/** 获取墙面方向（归一化） */
	FVector GetWallDirection() const;

	/** 获取墙面长度 */
	float GetWallLength() const;

	/** 更新墙角斜切（检测相邻墙体并计算斜切角度） */
	UFUNCTION(BlueprintCallable, Category = "Wall Corner")
	void UpdateMiterAngles();

	/** 静态方法：更新指定墙体及其相邻墙的斜切 */
	static void UpdateWallAndNeighbors(AWallBuilderWallActor* TargetWall, UWorld* World);

protected:
	virtual void PostInitializeComponents() override;
	virtual void PostEditMove(bool bFinished) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall")
	FWallParameters WallParams;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wall")
	UStaticMeshComponent* WallMesh;

	/** 支持开洞的 ProceduralMesh 组件（运行时动态创建，不在Details显示） */
	UPROPERTY(Transient)
	class UProceduralMeshComponent* WallProcMesh;

	UPROPERTY(EditAnywhere, Category = "Opening")
	TArray<FWallOpening> Openings;
};
