// Copyright Epic Games, Inc. All Rights Reserved.

#include "WallBuilderStairActor.h"
#include "ProceduralMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "MaterialDomain.h"

AWallBuilderStairActor::AWallBuilderStairActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// BoxComponent 仅用于编辑器选择射线（Visibility通道），不阻挡角色移动
	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	RootComponent = CollisionBox;
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionBox->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	CollisionBox->SetCollisionResponseToChannel(ECC_Visibility, ECollisionResponse::ECR_Block);
	CollisionBox->SetBoxExtent(FVector(50.0f));
	CollisionBox->SetVisibility(false);

	// ProceduralMesh 负责渲染 + 精确物理碰撞
	StairMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("StairMesh"));
	StairMesh->SetupAttachment(CollisionBox);
	StairMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	StairMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
	StairMesh->SetCollisionObjectType(ECC_WorldStatic);
	StairMesh->bUseComplexAsSimpleCollision = true;
}

void AWallBuilderStairActor::SetStairParameters(const FStairParameters& InParams)
{
	StairParams = InParams;
	RebuildMesh();
}

void AWallBuilderStairActor::RebuildMesh()
{
	StairMesh->ClearAllMeshSections();

	const FVector& S = StairParams.StartPoint;
	const FVector& E = StairParams.EndPoint;
	FVector ActorLoc = GetActorLocation();

	FVector LS = S - ActorLoc;
	FVector LE = E - ActorLoc;

	float Width = StairParams.Width;
	float TotalH = StairParams.TotalHeight;
	int32 N = FMath::Max(StairParams.StepCount, 1);

	FVector Dir2D = LE - LS;
	Dir2D.Z = 0.0f;
	float RunLength = Dir2D.Size();
	if (RunLength < 1.0f) return;
	Dir2D /= RunLength;

	FVector Right = FVector::CrossProduct(FVector::UpVector, Dir2D);
	float Hw = Width * 0.5f;
	float StepDepth = RunLength / (float)N;
	float StepHeight = TotalH / (float)N;

	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> Colors;
	TArray<FProcMeshTangent> Tangents;

	// 添加双面四边形（A→B→C→D 逆时针绕序）
	auto AddQuad = [&](const FVector& A, const FVector& B, const FVector& C, const FVector& D,
		const FVector& Normal)
	{
		int32 Base = Verts.Num();
		Verts.Append({A, B, C, D});
		// 正面：ABC 和 ACD
		Tris.Append({Base, Base+1, Base+2, Base, Base+2, Base+3});
		// 背面：ADC 和 ACB
		Tris.Append({Base, Base+3, Base+2, Base, Base+2, Base+1});
		for (int32 i = 0; i < 4; i++)
		{
			Normals.Add(Normal);
			Colors.Add(FColor::White);
			Tangents.Add(FProcMeshTangent(FVector::ForwardVector, false));
		}
		UVs.Append({FVector2D(0,0), FVector2D(1,0), FVector2D(1,1), FVector2D(0,1)});
	};

	// 生成每个台阶
	for (int32 i = 0; i < N; i++)
	{
		float D0 = (float)i * StepDepth;
		float D1 = (float)(i + 1) * StepDepth;
		float Z0 = (float)i * StepHeight;
		float Z1 = (float)(i + 1) * StepHeight;

		// 四个角点（局部坐标，XY来自LS+Dir偏移，Z来自台阶高度）
		FVector BL = LS + Dir2D * D0 - Right * Hw; // Bottom-Left
		FVector BR = LS + Dir2D * D0 + Right * Hw; // Bottom-Right
		FVector TL = LS + Dir2D * D1 - Right * Hw; // Top-Left (沿Run方向)
		FVector TR = LS + Dir2D * D1 + Right * Hw; // Top-Right

		// 1. 踏面（水平顶面）- 在 Z1 高度
		AddQuad(
			FVector(BL.X, BL.Y, Z1),
			FVector(BR.X, BR.Y, Z1),
			FVector(TR.X, TR.Y, Z1),
			FVector(TL.X, TL.Y, Z1),
			FVector::UpVector
		);

		// 2. 竖面（立面）- 法线朝起点方向(-Dir)
		AddQuad(
			FVector(BL.X, BL.Y, Z0),
			FVector(BR.X, BR.Y, Z0),
			FVector(BR.X, BR.Y, Z1),
			FVector(BL.X, BL.Y, Z1),
			-Dir2D
		);

		// 3. 左侧面 - 从 Z0 到 Z1
		AddQuad(
			FVector(BL.X, BL.Y, Z0),
			FVector(BL.X, BL.Y, Z1),
			FVector(TL.X, TL.Y, Z1),
			FVector(TL.X, TL.Y, Z0),
			-Right
		);

		// 4. 右侧面 - 从 Z0 到 Z1
		AddQuad(
			FVector(BR.X, BR.Y, Z0),
			FVector(TR.X, TR.Y, Z0),
			FVector(TR.X, TR.Y, Z1),
			FVector(BR.X, BR.Y, Z1),
			Right
		);
	}

	// 底面封闭
	{
		FVector BL = LS - Right * Hw;
		FVector BR = LS + Right * Hw;
		FVector TL = LS + Dir2D * RunLength - Right * Hw;
		FVector TR = LS + Dir2D * RunLength + Right * Hw;
		float ZBot = 0.0f;

		AddQuad(
			FVector(BL.X, BL.Y, ZBot),
			FVector(TL.X, TL.Y, ZBot),
			FVector(TR.X, TR.Y, ZBot),
			FVector(BR.X, BR.Y, ZBot),
			FVector::DownVector
		);

		// 最后一个台阶的背面 — 法线朝终点方向(+Dir)
		float ZN = TotalH;
		AddQuad(
			FVector(TL.X, TL.Y, 0.0f),
			FVector(TR.X, TR.Y, 0.0f),
			FVector(TR.X, TR.Y, ZN),
			FVector(TL.X, TL.Y, ZN),
			Dir2D
		);
	}

	// 整体左侧封闭面（阶梯状轮廓）
	{
		// 左侧多边形顶点（从下往上，阶梯状）
		TArray<FVector> LeftProfile;
		// 起点：左下角
		FVector StartLeft = LS - Right * Hw;
		LeftProfile.Add(FVector(StartLeft.X, StartLeft.Y, 0.0f));
		// 每个台阶的左上角
		for (int32 i = 0; i < N; i++)
		{
			float D = (float)(i + 1) * StepDepth;
			float Z = (float)(i + 1) * StepHeight;
			FVector P = LS + Dir2D * D - Right * Hw;
			LeftProfile.Add(FVector(P.X, P.Y, Z));
		}
		// 终点：右下角（回到起点闭合）
		FVector EndLeft = LS + Dir2D * RunLength - Right * Hw;
		LeftProfile.Add(FVector(EndLeft.X, EndLeft.Y, 0.0f));

		// 用三角形扇形绘制左侧封闭面
		int32 NumPts = LeftProfile.Num();
		for (int32 i = 0; i < NumPts - 2; i++)
		{
			int32 Base = Verts.Num();
			Verts.Add(LeftProfile[0]);
			Verts.Add(LeftProfile[i + 1]);
			Verts.Add(LeftProfile[i + 2]);
			// 正面
			Tris.Add(Base); Tris.Add(Base + 1); Tris.Add(Base + 2);
			// 背面
			Tris.Add(Base); Tris.Add(Base + 2); Tris.Add(Base + 1);
			for (int32 j = 0; j < 3; j++)
			{
				Normals.Add(-Right);
				Colors.Add(FColor::White);
				Tangents.Add(FProcMeshTangent(Dir2D, false));
			}
			UVs.Add(FVector2D(0, 0));
			UVs.Add(FVector2D(1, 0));
			UVs.Add(FVector2D(0.5f, 1));
		}
	}

	// 整体右侧封闭面（阶梯状轮廓）
	{
		TArray<FVector> RightProfile;
		// 起点：右下角
		FVector StartRight = LS + Right * Hw;
		RightProfile.Add(FVector(StartRight.X, StartRight.Y, 0.0f));
		// 每个台阶的右上角
		for (int32 i = 0; i < N; i++)
		{
			float D = (float)(i + 1) * StepDepth;
			float Z = (float)(i + 1) * StepHeight;
			FVector P = LS + Dir2D * D + Right * Hw;
			RightProfile.Add(FVector(P.X, P.Y, Z));
		}
		// 终点：右下角（回到起点闭合）
		FVector EndRight = LS + Dir2D * RunLength + Right * Hw;
		RightProfile.Add(FVector(EndRight.X, EndRight.Y, 0.0f));

		// 用三角形扇形绘制右侧封闭面
		int32 NumPts = RightProfile.Num();
		for (int32 i = 0; i < NumPts - 2; i++)
		{
			int32 Base = Verts.Num();
			Verts.Add(RightProfile[0]);
			Verts.Add(RightProfile[i + 2]);
			Verts.Add(RightProfile[i + 1]);
			// 正面
			Tris.Add(Base); Tris.Add(Base + 1); Tris.Add(Base + 2);
			// 背面
			Tris.Add(Base); Tris.Add(Base + 2); Tris.Add(Base + 1);
			for (int32 j = 0; j < 3; j++)
			{
				Normals.Add(Right);
				Colors.Add(FColor::White);
				Tangents.Add(FProcMeshTangent(Dir2D, false));
			}
			UVs.Add(FVector2D(0, 0));
			UVs.Add(FVector2D(1, 0));
			UVs.Add(FVector2D(0.5f, 1));
		}
	}

	StairMesh->CreateMeshSection(0, Verts, Tris, Normals, UVs, Colors, Tangents, bCreatePhysicsCollision);

	// 双面材质
	UMaterialInterface* MatToUse = StairParams.StairMaterial;
	if (!MatToUse)
		MatToUse = UMaterial::GetDefaultMaterial(EMaterialDomain::MD_Surface);

	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(MatToUse, this);
	if (MID)
	{
		MID->BasePropertyOverrides.bOverride_TwoSided = true;
		MID->BasePropertyOverrides.TwoSided = true;
		StairMesh->SetMaterial(0, MID);
	}

	// 更新碰撞盒以匹配网格范围
	UpdateCollisionBox();
}

void AWallBuilderStairActor::UpdateCollisionBox()
{
	if (!CollisionBox || !StairMesh) return;

	// 计算楼梯的实际边界框
	const FVector& S = StairParams.StartPoint;
	const FVector& E = StairParams.EndPoint;
	FVector ActorLoc = GetActorLocation();

	FVector LS = S - ActorLoc;
	FVector LE = E - ActorLoc;

	float Width = StairParams.Width;
	float TotalH = StairParams.TotalHeight;

	FVector Dir2D = LE - LS;
	Dir2D.Z = 0.0f;
	float RunLength = Dir2D.Size();
	if (RunLength < 1.0f) return;
	Dir2D /= RunLength;

	FVector Right = FVector::CrossProduct(FVector::UpVector, Dir2D);
	float Hw = Width * 0.5f;

	// 计算边界框的最小和最大点
	FVector MinPoint = LS - Right * Hw;
	FVector MaxPoint = LS + Dir2D * RunLength + Right * Hw;
	MaxPoint.Z = TotalH;

	// 计算范围（半尺寸）
	FVector Extent = (MaxPoint - MinPoint) * 0.5f;

	// 只设置 BoxComponent 的大小，不移动位置（BoxComponent 是 Root，移动会导致 Actor 飘移）
	CollisionBox->SetBoxExtent(Extent);
}

void AWallBuilderStairActor::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RebuildMesh();
}

void AWallBuilderStairActor::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	// 当使用 Transform Gizmo 移动 Actor 时，更新 StartPoint 和 EndPoint
	FVector NewActorLoc = GetActorLocation();
	FVector OldCenter = (StairParams.StartPoint + StairParams.EndPoint) * 0.5f;
	FVector Delta = NewActorLoc - OldCenter;

	// 只有当位置真正改变时才更新
	if (!Delta.IsNearlyZero())
	{
		StairParams.StartPoint += Delta;
		StairParams.EndPoint += Delta;
		RebuildMesh();
	}
}
