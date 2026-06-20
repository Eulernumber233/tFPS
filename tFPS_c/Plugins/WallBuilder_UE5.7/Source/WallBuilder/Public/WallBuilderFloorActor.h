// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WallBuilderFloorActor.generated.h"

/**
 * 楼板参数结构体
 */
USTRUCT(BlueprintType)
struct FFloorParameters
{
	GENERATED_BODY()

	/** 楼板厚度 (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor")
	float Thickness = 20.0f;

	/** 楼板高度 (Z坐标) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor")
	float Height = 0.0f;

	/** 楼板材质 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor")
	UMaterialInterface* FloorMaterial = nullptr;
};

/**
 * 楼板Actor - 支持多边形楼板
 */
UCLASS(BlueprintType)
class WALLBUILDER_API AWallBuilderFloorActor : public AActor
{
	GENERATED_BODY()

public:
	AWallBuilderFloorActor();

	/** 设置楼板参数 */
	UFUNCTION(BlueprintCallable, Category = "Floor")
	void SetFloorParameters(const FFloorParameters& InParams);

	/** 获取楼板参数 */
	UFUNCTION(BlueprintCallable, Category = "Floor")
	const FFloorParameters& GetFloorParameters() const { return FloorParams; }

	/** 设置顶点 */
	UFUNCTION(BlueprintCallable, Category = "Floor")
	void SetVertices(const TArray<FVector>& InVertices);

	/** 获取顶点 */
	UFUNCTION(BlueprintCallable, Category = "Floor")
	const TArray<FVector>& GetVertices() const { return Vertices; }

	/** 添加顶点 */
	UFUNCTION(BlueprintCallable, Category = "Floor")
	void AddVertex(const FVector& Vertex);

	/** 更新顶点 */
	UFUNCTION(BlueprintCallable, Category = "Floor")
	void UpdateVertex(int32 Index, const FVector& NewVertex);

	/** 重建网格 */
	UFUNCTION(BlueprintCallable, Category = "Floor")
	void RebuildMesh();

	/** 获取楼板中心点 */
	FVector GetFloorCenter() const;

	/** 获取顶点数量 */
	int32 GetVertexCount() const { return Vertices.Num(); }

protected:
	virtual void PostInitializeComponents() override;

	/** 创建三角形索引（扇形三角化） */
	TArray<int32> Triangulate() const;

	/** 检查多边形是否有效（至少3个点，不共线） */
	bool IsPolygonValid() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Floor")
	FFloorParameters FloorParams;

	/** 楼板顶点（顺时针或逆时针，XZ平面） */
	UPROPERTY(EditAnywhere, Category = "Floor")
	TArray<FVector> Vertices;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Floor")
	class UProceduralMeshComponent* FloorMesh;
};
