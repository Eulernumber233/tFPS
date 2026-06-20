// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WallBuilderFloorPlanActor.generated.h"

/**
 * 参考图Actor - 在视口中显示导入的平面图作为参照
 */
UCLASS(Transient, NotPlaceable)
class WALLBUILDER_API AWallBuilderFloorPlanActor : public AActor
{
	GENERATED_BODY()

public:
	AWallBuilderFloorPlanActor();

	/** 从文件加载参考图 */
	UFUNCTION(BlueprintCallable, Category = "FloorPlan")
	bool LoadImage(const FString& FilePath);

	/** 设置不透明度 (0.0~1.0) */
	void SetOpacity(float Opacity);

	/** 设置图像缩放（宽度cm） */
	void SetImageScale(float ScaleCm);

	/** 设置图像中心位置 (X, Y) */
	void SetImagePosition(const FVector2D& Position);

	/** 设置Z高度 */
	void SetHeight(float InHeight);

	/** 获取当前加载的纹理 */
	class UTexture2D* GetTexture() const { return ImageTexture; }

	/** 获取图像像素尺寸 */
	FVector2D GetImagePixelSize() const { return ImagePixelSize; }

	/** 重建平面网格 */
	void RebuildQuad();

protected:
	/** 创建基础材质（单例缓存） */
	static class UMaterial* GetOrCreateBaseMaterial();

	/** 创建四边形网格 */
	void GenerateQuadMesh(float Width, float Height, TArray<FVector>& OutVerts,
		TArray<int32>& OutTris, TArray<FVector2D>& OutUVs, TArray<FVector>& OutNormals);

	UPROPERTY()
	class UProceduralMeshComponent* QuadMesh;

	UPROPERTY()
	class UTexture2D* ImageTexture;

	UPROPERTY()
	class UMaterialInstanceDynamic* MaterialInstance;

	/** 图像像素尺寸 (源自加载的纹理) */
	FVector2D ImagePixelSize = FVector2D::ZeroVector;

	float CurrentScaleCm = 500.0f;
	float CurrentOpacity = 1.0f;
	float CurrentHeight = 0.0f;
	FVector2D CurrentPosition = FVector2D::ZeroVector;
	FString LoadedFilePath;
};
