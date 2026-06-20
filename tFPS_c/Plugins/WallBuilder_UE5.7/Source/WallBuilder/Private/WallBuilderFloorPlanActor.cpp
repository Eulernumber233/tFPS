// Copyright Epic Games, Inc. All Rights Reserved.

#include "WallBuilderFloorPlanActor.h"
#include "ProceduralMeshComponent.h"
#include "ImageUtils.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/Material.h"

AWallBuilderFloorPlanActor::AWallBuilderFloorPlanActor()
{
	PrimaryActorTick.bCanEverTick = false;

	QuadMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("QuadMesh"));
	RootComponent = QuadMesh;

	QuadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	QuadMesh->bUseComplexAsSimpleCollision = false;
	QuadMesh->SetCastShadow(false);
}

UMaterial* AWallBuilderFloorPlanActor::GetOrCreateBaseMaterial()
{
	// 静态缓存，避免重复创建
	static UMaterial* CachedMaterial = nullptr;
	if (CachedMaterial && IsValid(CachedMaterial))
		return CachedMaterial;

	UMaterial* Material = NewObject<UMaterial>(GetTransientPackage(), NAME_None, RF_Public | RF_Standalone);
	if (!Material)
		return nullptr;

	// 设置材质属性
	Material->BlendMode = BLEND_Translucent;
	Material->TwoSided = true;
	Material->SetShadingModel(MSM_Unlit);

	// 获取 EditorOnlyData
	UMaterialEditorOnlyData* EditorData = Material->GetEditorOnlyData();
	if (!EditorData)
	{
		UE_LOG(LogTemp, Warning, TEXT("FloorPlanActor: No EditorOnlyData available for material"));
		return nullptr;
	}

	// 创建纹理采样参数（用于 EmissiveColor，Unlit 材质的颜色来源）
	UMaterialExpressionTextureSampleParameter2D* TexExpr = NewObject<UMaterialExpressionTextureSampleParameter2D>(Material);
	TexExpr->ParameterName = FName(TEXT("TextureParam"));
	TexExpr->SamplerType = SAMPLERTYPE_Color;
	EditorData->ExpressionCollection.Expressions.Add(TexExpr);

	// 创建 Opacity 标量参数（用于控制整体透明度）
	UMaterialExpressionScalarParameter* OpacityParam = NewObject<UMaterialExpressionScalarParameter>(Material);
	OpacityParam->ParameterName = FName(TEXT("Opacity"));
	OpacityParam->DefaultValue = 1.0f;
	EditorData->ExpressionCollection.Expressions.Add(OpacityParam);

	// Unlit 材质颜色来源: EmissiveColor ← TexExpr.RGB
	EditorData->EmissiveColor.Expression = TexExpr;
	EditorData->EmissiveColor.OutputIndex = 0;
	EditorData->EmissiveColor.Mask = 0;
	EditorData->EmissiveColor.MaskR = 1;
	EditorData->EmissiveColor.MaskG = 1;
	EditorData->EmissiveColor.MaskB = 1;
	EditorData->EmissiveColor.MaskA = 0;

	// Opacity ← OpacityParam
	EditorData->Opacity.Expression = OpacityParam;
	EditorData->Opacity.OutputIndex = 0;
	EditorData->Opacity.Mask = 1;
	EditorData->Opacity.MaskA = 1;
	EditorData->Opacity.MaskR = 0;
	EditorData->Opacity.MaskG = 0;
	EditorData->Opacity.MaskB = 0;

	Material->PostEditChange();
	CachedMaterial = Material;
	return Material;
}

void AWallBuilderFloorPlanActor::GenerateQuadMesh(float Width, float Height,
	TArray<FVector>& OutVerts, TArray<int32>& OutTris,
	TArray<FVector2D>& OutUVs, TArray<FVector>& OutNormals)
{
	const float Hw = Width * 0.5f;
	const float Hh = Height * 0.5f;
	const float Z = CurrentHeight;

	float CX = CurrentPosition.X;
	float CY = CurrentPosition.Y;

	// 四个顶点：Bottom-Left, Bottom-Right, Top-Right, Top-Left
	OutVerts.SetNum(4);
	OutVerts[0] = FVector(CX - Hw, CY - Hh, Z);  // BL
	OutVerts[1] = FVector(CX + Hw, CY - Hh, Z);  // BR
	OutVerts[2] = FVector(CX + Hw, CY + Hh, Z);  // TR
	OutVerts[3] = FVector(CX - Hw, CY + Hh, Z);  // TL

	// 两个三角形（双面渲染）
	OutTris.SetNum(6);
	OutTris[0] = 0; OutTris[1] = 2; OutTris[2] = 1;
	OutTris[3] = 0; OutTris[4] = 3; OutTris[5] = 2;

	// UV - 图像坐标系 (0,0)=左下, (1,1)=右上
	OutUVs.SetNum(4);
	OutUVs[0] = FVector2D(0.0f, 1.0f);
	OutUVs[1] = FVector2D(1.0f, 1.0f);
	OutUVs[2] = FVector2D(1.0f, 0.0f);
	OutUVs[3] = FVector2D(0.0f, 0.0f);

	// 法线向上
	OutNormals.Init(FVector::UpVector, 4);
}

bool AWallBuilderFloorPlanActor::LoadImage(const FString& FilePath)
{
	if (FilePath.IsEmpty())
		return false;

	// 检查文件是否存在
	if (!FPaths::FileExists(FilePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("FloorPlanActor: File not found - %s"), *FilePath);
		return false;
	}

	// 使用 FImageUtils 导入纹理
	UTexture2D* NewTexture = FImageUtils::ImportFileAsTexture2D(FilePath);
	if (!NewTexture)
	{
		UE_LOG(LogTemp, Warning, TEXT("FloorPlanActor: Failed to import image - %s"), *FilePath);
		return false;
	}

	// 设置纹理属性
	NewTexture->SRGB = true;
	NewTexture->CompressionSettings = TC_EditorIcon;
	NewTexture->Filter = TF_Trilinear;
	NewTexture->UpdateResource();

	ImageTexture = NewTexture;
	ImagePixelSize = FVector2D((float)NewTexture->GetSizeX(), (float)NewTexture->GetSizeY());
	LoadedFilePath = FilePath;

	// 创建或更新材质实例
	UMaterial* BaseMat = GetOrCreateBaseMaterial();
	if (!BaseMat)
		return false;

	MaterialInstance = UMaterialInstanceDynamic::Create(BaseMat, this);
	if (!MaterialInstance)
		return false;

	MaterialInstance->SetTextureParameterValue(FName(TEXT("TextureParam")), ImageTexture);
	QuadMesh->SetMaterial(0, MaterialInstance);

	// 重建网格
	RebuildQuad();

	// 设置透明度
	SetOpacity(CurrentOpacity);

	UE_LOG(LogTemp, Log, TEXT("FloorPlanActor: Loaded image %s (%dx%d)"),
		*FPaths::GetCleanFilename(FilePath),
		NewTexture->GetSizeX(), NewTexture->GetSizeY());

	return true;
}

void AWallBuilderFloorPlanActor::RebuildQuad()
{
	if (!ImageTexture || !QuadMesh)
	{
		QuadMesh->ClearAllMeshSections();
		return;
	}

	float AspectRatio = ImagePixelSize.Y > 0 ? (ImagePixelSize.X / ImagePixelSize.Y) : 1.0f;
	float Width = CurrentScaleCm;
	float Height = CurrentScaleCm / AspectRatio;

	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector2D> UVs;
	TArray<FVector> Normals;

	GenerateQuadMesh(Width, Height, Verts, Tris, UVs, Normals);

	// 切线
	TArray<FProcMeshTangent> Tangents;
	Tangents.Init(FProcMeshTangent(FVector::ForwardVector, false), 4);

	// 顶点颜色
	TArray<FColor> Colors;
	Colors.Init(FColor::White, 4);

	QuadMesh->ClearAllMeshSections();
	QuadMesh->CreateMeshSection(0, Verts, Tris, Normals, UVs, Colors, Tangents, false);
}

void AWallBuilderFloorPlanActor::SetOpacity(float Opacity)
{
	CurrentOpacity = FMath::Clamp(Opacity, 0.0f, 1.0f);
	if (MaterialInstance)
	{
		MaterialInstance->SetScalarParameterValue(FName(TEXT("Opacity")), CurrentOpacity);
	}
}

void AWallBuilderFloorPlanActor::SetImageScale(float ScaleCm)
{
	CurrentScaleCm = FMath::Max(ScaleCm, 1.0f);
	RebuildQuad();
}

void AWallBuilderFloorPlanActor::SetImagePosition(const FVector2D& Position)
{
	CurrentPosition = Position;
	RebuildQuad();
}

void AWallBuilderFloorPlanActor::SetHeight(float InHeight)
{
	CurrentHeight = InHeight;
	RebuildQuad();
}
