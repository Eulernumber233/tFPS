// Copyright Epic Games, Inc. All Rights Reserved.

#include "WallBuilderRampActor.h"
#include "ProceduralMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "MaterialDomain.h"

AWallBuilderRampActor::AWallBuilderRampActor()
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
	RampMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("RampMesh"));
	RampMesh->SetupAttachment(CollisionBox);
	RampMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	RampMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
	RampMesh->SetCollisionObjectType(ECC_WorldStatic);
	RampMesh->bUseComplexAsSimpleCollision = true;
}

void AWallBuilderRampActor::SetRampParameters(const FRampParameters& InParams)
{
	RampParams = InParams;
	RebuildMesh();
}

void AWallBuilderRampActor::RebuildMesh()
{
	RampMesh->ClearAllMeshSections();

	FVector ActorLoc = GetActorLocation();

	// 转换为局部坐标
	FVector LS = RampParams.StartPoint - ActorLoc;
	FVector LE = RampParams.EndPoint - ActorLoc;

	FVector Dir = LE - LS;
	Dir.Z = 0.0f;
	float RunLength = Dir.Size();
	if (RunLength < 1.0f) return;
	Dir /= RunLength;

	FVector Right = FVector::CrossProduct(FVector::UpVector, Dir);
	float Hw = RampParams.Width * 0.5f;

	float ZS = RampParams.StartHeight;
	float ZE = RampParams.EndHeight;

	FVector SL = LS - Right * Hw;
	FVector SR = LS + Right * Hw;
	FVector EL = LS + Dir * RunLength - Right * Hw;
	FVector ER = LS + Dir * RunLength + Right * Hw;

	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FColor> Colors;
	TArray<FProcMeshTangent> Tangents;

	auto AddTri = [&](const FVector& A, const FVector& B, const FVector& C,
		const FVector& Normal, const FVector2D& UVBase)
	{
		int32 Base = Verts.Num();
		Verts.Add(A); Verts.Add(B); Verts.Add(C);
		Tris.Add(Base); Tris.Add(Base+1); Tris.Add(Base+2);
		Tris.Add(Base); Tris.Add(Base+2); Tris.Add(Base+1); // 背面
		for (int32 i = 0; i < 3; i++)
		{
			Normals.Add(Normal);
			Colors.Add(FColor::White);
			Tangents.Add(FProcMeshTangent(FVector::ForwardVector, false));
		}
		UVs.Add(UVBase + FVector2D(0, 0));
		UVs.Add(UVBase + FVector2D(1, 0));
		UVs.Add(UVBase + FVector2D(0, 1));
	};

	auto AddQuad = [&](const FVector& A, const FVector& B, const FVector& C, const FVector& D,
		const FVector& Normal, const FVector2D& UVBase)
	{
		int32 Base = Verts.Num();
		Verts.Add(A); Verts.Add(B); Verts.Add(C); Verts.Add(D);
		// 正面：ABC 和 ACD
		Tris.Add(Base);   Tris.Add(Base+1); Tris.Add(Base+2);
		Tris.Add(Base);   Tris.Add(Base+2); Tris.Add(Base+3);
		// 背面：ADC 和 ACB
		Tris.Add(Base);   Tris.Add(Base+3); Tris.Add(Base+2);
		Tris.Add(Base);   Tris.Add(Base+2); Tris.Add(Base+1);
		for (int32 i = 0; i < 4; i++)
		{
			Normals.Add(Normal);
			Colors.Add(FColor::White);
			Tangents.Add(FProcMeshTangent(FVector::ForwardVector, false));
		}
		float U0 = UVBase.X, V0 = UVBase.Y;
		UVs.Add(FVector2D(U0, V0));
		UVs.Add(FVector2D(U0 + 1.0f, V0));
		UVs.Add(FVector2D(U0 + 1.0f, V0 + 1.0f));
		UVs.Add(FVector2D(U0, V0 + 1.0f));
	};

	float ZLow = FMath::Min(ZS, ZE);

	// Top (sloped) - 法线朝上
	FVector SlopeDir = (FVector(EL.X, EL.Y, ZE) - FVector(SL.X, SL.Y, ZS)).GetSafeNormal();
	FVector TopNormal = FVector::CrossProduct(SlopeDir, Right).GetSafeNormal();
	if (TopNormal.Z < 0.0f) TopNormal = -TopNormal; // 确保朝上
	AddQuad(
		FVector(SL.X, SL.Y, ZS),
		FVector(SR.X, SR.Y, ZS),
		FVector(ER.X, ER.Y, ZE),
		FVector(EL.X, EL.Y, ZE),
		TopNormal,
		FVector2D(0.0f, 0.0f)
	);

	// Bottom
	AddQuad(
		FVector(SL.X, SL.Y, ZLow),
		FVector(EL.X, EL.Y, ZLow),
		FVector(ER.X, ER.Y, ZLow),
		FVector(SR.X, SR.Y, ZLow),
		FVector::DownVector,
		FVector2D(0.0f, 0.0f)
	);

	// Left side
	AddQuad(
		FVector(SL.X, SL.Y, ZLow),
		FVector(EL.X, EL.Y, ZLow),
		FVector(EL.X, EL.Y, ZE),
		FVector(SL.X, SL.Y, ZS),
		-Right,
		FVector2D(0.0f, 0.0f)
	);

	// Right side
	AddQuad(
		FVector(SR.X, SR.Y, ZS),
		FVector(ER.X, ER.Y, ZE),
		FVector(ER.X, ER.Y, ZLow),
		FVector(SR.X, SR.Y, ZLow),
		Right,
		FVector2D(0.0f, 0.0f)
	);

	// Front face (at end)
	AddQuad(
		FVector(EL.X, EL.Y, ZLow),
		FVector(ER.X, ER.Y, ZLow),
		FVector(ER.X, ER.Y, ZE),
		FVector(EL.X, EL.Y, ZE),
		Dir,
		FVector2D(0.0f, 0.0f)
	);

	// Back face (at start)
	AddQuad(
		FVector(SL.X, SL.Y, ZS),
		FVector(SR.X, SR.Y, ZS),
		FVector(SR.X, SR.Y, ZLow),
		FVector(SL.X, SL.Y, ZLow),
		-Dir,
		FVector2D(0.0f, 0.0f)
	);

	RampMesh->CreateMeshSection(0, Verts, Tris, Normals, UVs, Colors, Tangents, bCreatePhysicsCollision);

	// 强制使用双面材质
	UMaterialInterface* MatToUse = RampParams.RampMaterial;
	if (!MatToUse)
		MatToUse = UMaterial::GetDefaultMaterial(EMaterialDomain::MD_Surface);

	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(MatToUse, this);
	if (MID)
	{
		MID->BasePropertyOverrides.bOverride_TwoSided = true;
		MID->BasePropertyOverrides.TwoSided = true;
		RampMesh->SetMaterial(0, MID);
	}

	// 更新碰撞盒以匹配网格范围
	UpdateCollisionBox();
}

void AWallBuilderRampActor::UpdateCollisionBox()
{
	if (!CollisionBox || !RampMesh) return;

	// 计算坡道的实际边界框
	FVector ActorLoc = GetActorLocation();
	FVector LS = RampParams.StartPoint - ActorLoc;
	FVector LE = RampParams.EndPoint - ActorLoc;

	FVector Dir = LE - LS;
	Dir.Z = 0.0f;
	float RunLength = Dir.Size();
	if (RunLength < 1.0f) return;
	Dir /= RunLength;

	FVector Right = FVector::CrossProduct(FVector::UpVector, Dir);
	float Hw = RampParams.Width * 0.5f;

	float ZS = RampParams.StartHeight;
	float ZE = RampParams.EndHeight;
	float ZLow = FMath::Min(ZS, ZE);
	float ZHigh = FMath::Max(ZS, ZE);

	// 计算边界框的最小和最大点
	FVector MinPoint = LS - Right * Hw;
	MinPoint.Z = ZLow;
	FVector MaxPoint = LS + Dir * RunLength + Right * Hw;
	MaxPoint.Z = ZHigh;

	// 计算范围（半尺寸）
	FVector Extent = (MaxPoint - MinPoint) * 0.5f;

	// 只设置 BoxComponent 的大小，不移动位置（BoxComponent 是 Root，移动会导致 Actor 飘移）
	CollisionBox->SetBoxExtent(Extent);
}

void AWallBuilderRampActor::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RebuildMesh();
}

void AWallBuilderRampActor::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	// 当使用 Transform Gizmo 移动 Actor 时，更新 StartPoint 和 EndPoint
	FVector NewActorLoc = GetActorLocation();
	FVector OldCenter = (RampParams.StartPoint + RampParams.EndPoint) * 0.5f;
	FVector Delta = NewActorLoc - OldCenter;

	// 只有当位置真正改变时才更新
	if (!Delta.IsNearlyZero())
	{
		RampParams.StartPoint += Delta;
		RampParams.EndPoint += Delta;
		RebuildMesh();
	}
}
