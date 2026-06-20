// Copyright Epic Games, Inc. All Rights Reserved.

#include "WallBuilderWallActor.h"
#include "ProceduralMeshComponent.h"
#include "Kismet/GameplayStatics.h"

AWallBuilderWallActor::AWallBuilderWallActor()
{
	PrimaryActorTick.bCanEverTick = false;

	WallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallMesh"));
	RootComponent = WallMesh;

	WallProcMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("WallProcMesh"));
	WallProcMesh->SetupAttachment(RootComponent);
	WallProcMesh->SetVisibility(false);
	WallProcMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WallProcMesh->bUseComplexAsSimpleCollision = true;
	WallProcMesh->SetMaterial(0, nullptr);

	WallParams.Height = 300.0f;
	WallParams.Thickness = 20.0f;
}

void AWallBuilderWallActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (!WallMesh->GetStaticMesh())
	{
		if (UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("StaticMesh'/Engine/BasicShapes/Cube.Cube'")))
			WallMesh->SetStaticMesh(CubeMesh);
	}
}

FVector AWallBuilderWallActor::GetWallDirection() const
{
	FVector Dir = WallParams.EndPoint - WallParams.StartPoint;
	Dir.Z = 0.0f;
	float Len = Dir.Size();
	return Len > 0.01f ? Dir / Len : FVector::ForwardVector;
}

float AWallBuilderWallActor::GetWallLength() const
{
	FVector Dir = WallParams.EndPoint - WallParams.StartPoint;
	Dir.Z = 0.0f;
	return Dir.Size();
}

void AWallBuilderWallActor::SetWallParameters(const FWallParameters& InParams)
{
	WallParams = InParams;

	if (!WallMesh->GetStaticMesh())
	{
		if (UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("StaticMesh'/Engine/BasicShapes/Cube.Cube'")))
			WallMesh->SetStaticMesh(CubeMesh);
	}

	FVector Center = (WallParams.StartPoint + WallParams.EndPoint) * 0.5f;
	Center.Z = WallParams.StartPoint.Z + WallParams.Height * 0.5f;

	FVector Direction = WallParams.EndPoint - WallParams.StartPoint;
	float Length = Direction.Size2D();

	SetActorLocation(Center);
	if (Length > 0.1f)
		SetActorRotation(FRotationMatrix::MakeFromX(Direction.GetSafeNormal2D()).Rotator());

	// 始终使用 ProceduralMesh 以支持斜切
	WallMesh->SetVisibility(false);
	WallMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WallProcMesh->SetVisibility(true);

	RebuildMesh();
}

// ===== 开洞操作 =====

void AWallBuilderWallActor::AddOpening(const FWallOpening& Opening)
{
	Openings.Add(Opening);
	if (Openings.Num() == 1)
	{
		WallMesh->SetVisibility(false);
		WallMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WallProcMesh->SetVisibility(true);
	}
	RebuildMesh();
}

void AWallBuilderWallActor::RemoveOpening(int32 Index)
{
	if (!Openings.IsValidIndex(Index)) return;
	Openings.RemoveAt(Index);

	if (Openings.Num() == 0)
	{
		WallProcMesh->ClearAllMeshSections();
		WallProcMesh->SetVisibility(false);
		WallProcMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WallMesh->SetVisibility(true);
		WallMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	else
	{
		RebuildMesh();
	}
}

void AWallBuilderWallActor::UpdateOpening(int32 Index, const FWallOpening& NewOpening)
{
	if (!Openings.IsValidIndex(Index)) return;
	Openings[Index] = NewOpening;
	RebuildMesh();
}

void AWallBuilderWallActor::ClearOpenings()
{
	Openings.Empty();
	WallProcMesh->ClearAllMeshSections();
	WallProcMesh->SetVisibility(false);
	WallProcMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WallMesh->SetVisibility(true);
	WallMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}

// ===== RebuildMesh =====

void AWallBuilderWallActor::RebuildMesh()
{
	if (!WallProcMesh) return;
	WallProcMesh->ClearAllMeshSections();

	float Length = GetWallLength();
	if (Length < 0.1f) return;

	float HalfLen = Length * 0.5f;
	float HalfT = WallParams.Thickness * 0.5f;
	float H = WallParams.Height;
	float H2 = H * 0.5f;

	// 计算斜切偏移
	float StartMiterRad = FMath::DegreesToRadians(WallParams.StartMiterAngle);
	float EndMiterRad = FMath::DegreesToRadians(WallParams.EndMiterAngle);
	float StartMiterOffset = HalfT * FMath::Tan(StartMiterRad);
	float EndMiterOffset = HalfT * FMath::Tan(EndMiterRad);

	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FProcMeshTangent> Tangents;
	TArray<FVector2D> UV0;
	TArray<FColor> Colors;

	auto AddQuad = [&](const FVector& BL, const FVector& BR, const FVector& TR, const FVector& TL,
		float uL, float uR, float vB, float vT, const FVector& Normal)
	{
		int32 I = Verts.Num();
		Verts.Add(BL); Verts.Add(BR); Verts.Add(TR); Verts.Add(TL);

		Tris.Add(I); Tris.Add(I+2); Tris.Add(I+1);
		Tris.Add(I); Tris.Add(I+3); Tris.Add(I+2);
		Tris.Add(I+1); Tris.Add(I+2); Tris.Add(I);
		Tris.Add(I+2); Tris.Add(I+3); Tris.Add(I);

		Normals.Add(Normal); Normals.Add(Normal); Normals.Add(Normal); Normals.Add(Normal);

		FProcMeshTangent Tangent(FVector::CrossProduct(Normal, FVector::UpVector).GetSafeNormal(), false);
		if (FMath::Abs(FVector::DotProduct(Normal, FVector::UpVector)) > 0.99f)
			Tangent = FProcMeshTangent(FVector::CrossProduct(Normal, FVector::ForwardVector).GetSafeNormal(), false);
		Tangents.Add(Tangent); Tangents.Add(Tangent); Tangents.Add(Tangent); Tangents.Add(Tangent);

		UV0.Add(FVector2D(uL, vB)); UV0.Add(FVector2D(uR, vB));
		UV0.Add(FVector2D(uR, vT)); UV0.Add(FVector2D(uL, vT));
		Colors.Add(FColor::White); Colors.Add(FColor::White);
		Colors.Add(FColor::White); Colors.Add(FColor::White);
	};

	const float UVScale = 0.01f;

	const FVector NormalFront(0, 1, 0);
	const FVector NormalBack(0, -1, 0);
	const FVector NormalTop(0, 0, 1);
	const FVector NormalBottom(0, 0, -1);
	const FVector NormalLeft(-1, 0, 0);
	const FVector NormalRight(1, 0, 0);

	// 本地开洞坐标
	struct FLocalOp { float L, R, B, T; };
	TArray<FLocalOp> LocalOps;
	for (const FWallOpening& Op : Openings)
	{
		float Cx = Op.Position - HalfLen;
		LocalOps.Add({Cx - Op.Width*0.5f, Cx + Op.Width*0.5f, Op.ZOffset - H2, Op.ZOffset + Op.Height - H2});
	}

	// X断点
	TArray<float> XBr;
	XBr.Add(-HalfLen); XBr.Add(HalfLen);
	for (const FLocalOp& LO : LocalOps) { XBr.Add(LO.L); XBr.Add(LO.R); }
	XBr.Sort();

	auto OverlapsOp = [&](float X1, float X2) -> int32 {
		for (int32 i = 0; i < LocalOps.Num(); i++)
			if (X1 < LocalOps[i].R - 0.01f && X2 > LocalOps[i].L + 0.01f) return i;
		return -1;
	};

	// 前面 (Y = +HalfT) - 带斜切
	for (int32 i = 0; i < XBr.Num()-1; i++)
	{
		float X1 = XBr[i], X2 = XBr[i+1];
		if (FMath::IsNearlyEqual(X1, X2)) continue;

		// 应用斜切偏移
		float X1Miter = X1;
		float X2Miter = X2;
		if (X1 <= -HalfLen + 0.01f) X1Miter = X1 + StartMiterOffset;
		if (X2 >= HalfLen - 0.01f) X2Miter = X2 + EndMiterOffset;

		float uL = (X1+HalfLen)*UVScale, uR = (X2+HalfLen)*UVScale;
		int32 OpI = OverlapsOp(X1, X2);
		if (OpI >= 0)
		{
			const FLocalOp& LO = LocalOps[OpI];
			if (LO.B > -H2+0.1f) AddQuad(FVector(X1Miter,HalfT,-H2), FVector(X2Miter,HalfT,-H2), FVector(X2Miter,HalfT,LO.B), FVector(X1Miter,HalfT,LO.B), uL,uR, 0,(LO.B+H2)*UVScale, NormalFront);
			if (LO.T < H2-0.1f) AddQuad(FVector(X1Miter,HalfT,LO.T), FVector(X2Miter,HalfT,LO.T), FVector(X2Miter,HalfT,H2), FVector(X1Miter,HalfT,H2), uL,uR, (LO.T+H2)*UVScale,H*UVScale, NormalFront);
		}
		else AddQuad(FVector(X1Miter,HalfT,-H2), FVector(X2Miter,HalfT,-H2), FVector(X2Miter,HalfT,H2), FVector(X1Miter,HalfT,H2), uL,uR, 0,H*UVScale, NormalFront);
	}

	// 背面 (Y = -HalfT) - 带斜切
	for (int32 i = 0; i < XBr.Num()-1; i++)
	{
		float X1 = XBr[i], X2 = XBr[i+1];
		if (FMath::IsNearlyEqual(X1, X2)) continue;

		float X1Miter = X1;
		float X2Miter = X2;
		if (X1 <= -HalfLen + 0.01f) X1Miter = X1 - StartMiterOffset;
		if (X2 >= HalfLen - 0.01f) X2Miter = X2 - EndMiterOffset;

		float uL = (X1+HalfLen)*UVScale, uR = (X2+HalfLen)*UVScale;
		int32 OpI = OverlapsOp(X1, X2);
		if (OpI >= 0)
		{
			const FLocalOp& LO = LocalOps[OpI];
			if (LO.B > -H2+0.1f) AddQuad(FVector(X1Miter,-HalfT,-H2), FVector(X2Miter,-HalfT,-H2), FVector(X2Miter,-HalfT,LO.B), FVector(X1Miter,-HalfT,LO.B), uL,uR, 0,(LO.B+H2)*UVScale, NormalBack);
			if (LO.T < H2-0.1f) AddQuad(FVector(X1Miter,-HalfT,LO.T), FVector(X2Miter,-HalfT,LO.T), FVector(X2Miter,-HalfT,H2), FVector(X1Miter,-HalfT,H2), uL,uR, (LO.T+H2)*UVScale,H*UVScale, NormalBack);
		}
		else AddQuad(FVector(X1Miter,-HalfT,-H2), FVector(X2Miter,-HalfT,-H2), FVector(X2Miter,-HalfT,H2), FVector(X1Miter,-HalfT,H2), uL,uR, 0,H*UVScale, NormalBack);
	}

	// 洞内四侧面
	for (const FLocalOp& LO : LocalOps)
	{
		float u0 = (LO.L+HalfLen)*UVScale, u1 = (LO.R+HalfLen)*UVScale;
		float vHeight = (LO.T-LO.B)*UVScale;
		float vThick = WallParams.Thickness*UVScale;
		AddQuad(FVector(LO.L,-HalfT,LO.T), FVector(LO.R,-HalfT,LO.T), FVector(LO.R,HalfT,LO.T), FVector(LO.L,HalfT,LO.T), u0,u1, vThick,0, NormalBottom);
		AddQuad(FVector(LO.L,-HalfT,LO.B), FVector(LO.R,-HalfT,LO.B), FVector(LO.R,HalfT,LO.B), FVector(LO.L,HalfT,LO.B), u0,u1, 0,vThick, NormalTop);
		AddQuad(FVector(LO.L,-HalfT,LO.B), FVector(LO.L,-HalfT,LO.T), FVector(LO.L,HalfT,LO.T), FVector(LO.L,HalfT,LO.B), 0,vThick, 0,vHeight, NormalRight);
		AddQuad(FVector(LO.R,-HalfT,LO.B), FVector(LO.R,-HalfT,LO.T), FVector(LO.R,HalfT,LO.T), FVector(LO.R,HalfT,LO.B), 0,vThick, 0,vHeight, NormalLeft);
	}

	// 顶面 (Z = +H2)
	for (int32 i = 0; i < XBr.Num()-1; i++)
	{
		float X1 = XBr[i], X2 = XBr[i+1];
		if (FMath::IsNearlyEqual(X1, X2)) continue;
		int32 OpI = OverlapsOp(X1, X2);
		if (OpI >= 0 && LocalOps[OpI].T >= H2-0.1f) continue;

		float X1Miter = X1, X2Miter = X2;
		if (X1 <= -HalfLen + 0.01f) X1Miter = X1 + StartMiterOffset;
		if (X2 >= HalfLen - 0.01f) X2Miter = X2 + EndMiterOffset;
		float X1MiterBack = X1, X2MiterBack = X2;
		if (X1 <= -HalfLen + 0.01f) X1MiterBack = X1 - StartMiterOffset;
		if (X2 >= HalfLen - 0.01f) X2MiterBack = X2 - EndMiterOffset;

		float u0 = (X1+HalfLen)*UVScale, u1 = (X2+HalfLen)*UVScale;
		AddQuad(FVector(X1MiterBack,-HalfT,H2), FVector(X2MiterBack,-HalfT,H2), FVector(X2Miter,HalfT,H2), FVector(X1Miter,HalfT,H2), u0,u1, 0,WallParams.Thickness*UVScale, NormalTop);
	}

	// 底面 (Z = -H2)
	for (int32 i = 0; i < XBr.Num()-1; i++)
	{
		float X1 = XBr[i], X2 = XBr[i+1];
		if (FMath::IsNearlyEqual(X1, X2)) continue;
		int32 OpI = OverlapsOp(X1, X2);
		if (OpI >= 0 && LocalOps[OpI].B <= -H2+0.1f) continue;

		float X1Miter = X1, X2Miter = X2;
		if (X1 <= -HalfLen + 0.01f) X1Miter = X1 + StartMiterOffset;
		if (X2 >= HalfLen - 0.01f) X2Miter = X2 + EndMiterOffset;
		float X1MiterBack = X1, X2MiterBack = X2;
		if (X1 <= -HalfLen + 0.01f) X1MiterBack = X1 - StartMiterOffset;
		if (X2 >= HalfLen - 0.01f) X2MiterBack = X2 - EndMiterOffset;

		float u0 = (X1+HalfLen)*UVScale, u1 = (X2+HalfLen)*UVScale;
		AddQuad(FVector(X1MiterBack,-HalfT,-H2), FVector(X2MiterBack,-HalfT,-H2), FVector(X2Miter,HalfT,-H2), FVector(X1Miter,HalfT,-H2), u0,u1, 0,WallParams.Thickness*UVScale, NormalBottom);
	}

	// 左侧面 (X = -HalfLen) - 斜切面
	auto GetSideOpening = [&](float XEdge) -> TPair<bool, FLocalOp> {
		for (const FLocalOp& LO : LocalOps)
		{
			if ((XEdge < 0 && LO.L <= XEdge+0.1f) || (XEdge > 0 && LO.R >= XEdge-0.1f))
				return TPair<bool, FLocalOp>(true, LO);
		}
		return TPair<bool, FLocalOp>(false, FLocalOp());
	};

	auto SideL = GetSideOpening(-HalfLen);
	if (!SideL.Key)
	{
		// 斜切侧面
		float XFront = -HalfLen + StartMiterOffset;
		float XBack = -HalfLen - StartMiterOffset;
		AddQuad(FVector(XBack,-HalfT,-H2), FVector(XBack,-HalfT,H2), FVector(XFront,HalfT,H2), FVector(XFront,HalfT,-H2), 0,WallParams.Thickness*UVScale, 0,H*UVScale, NormalLeft);
	}
	else
	{
		FLocalOp LO = SideL.Value;
		float XFront = -HalfLen + StartMiterOffset;
		float XBack = -HalfLen - StartMiterOffset;
		if (LO.B > -H2+0.1f) AddQuad(FVector(XBack,-HalfT,-H2), FVector(XBack,-HalfT,LO.B), FVector(XFront,HalfT,LO.B), FVector(XFront,HalfT,-H2), 0,WallParams.Thickness*UVScale, 0,(LO.B+H2)*UVScale, NormalLeft);
		if (LO.T < H2-0.1f) AddQuad(FVector(XBack,-HalfT,LO.T), FVector(XBack,-HalfT,H2), FVector(XFront,HalfT,H2), FVector(XFront,HalfT,LO.T), 0,WallParams.Thickness*UVScale, (LO.T+H2)*UVScale,H*UVScale, NormalLeft);
	}

	// 右侧面 (X = +HalfLen) - 斜切面
	auto SideR = GetSideOpening(HalfLen);
	if (!SideR.Key)
	{
		float XFront = HalfLen + EndMiterOffset;
		float XBack = HalfLen - EndMiterOffset;
		AddQuad(FVector(XBack,-HalfT,-H2), FVector(XBack,-HalfT,H2), FVector(XFront,HalfT,H2), FVector(XFront,HalfT,-H2), 0,WallParams.Thickness*UVScale, 0,H*UVScale, NormalRight);
	}
	else
	{
		FLocalOp LO = SideR.Value;
		float XFront = HalfLen + EndMiterOffset;
		float XBack = HalfLen - EndMiterOffset;
		if (LO.B > -H2+0.1f) AddQuad(FVector(XBack,-HalfT,-H2), FVector(XBack,-HalfT,LO.B), FVector(XFront,HalfT,LO.B), FVector(XFront,HalfT,-H2), 0,WallParams.Thickness*UVScale, 0,(LO.B+H2)*UVScale, NormalRight);
		if (LO.T < H2-0.1f) AddQuad(FVector(XBack,-HalfT,LO.T), FVector(XBack,-HalfT,H2), FVector(XFront,HalfT,H2), FVector(XFront,HalfT,LO.T), 0,WallParams.Thickness*UVScale, (LO.T+H2)*UVScale,H*UVScale, NormalRight);
	}

	WallProcMesh->CreateMeshSection(0, Verts, Tris, Normals, UV0, Colors, Tangents, true);

	WallProcMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	WallProcMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
	WallProcMesh->bUseComplexAsSimpleCollision = true;

	if (UMaterialInterface* Mat = WallParams.WallMaterial ? WallParams.WallMaterial : WallMesh->GetMaterial(0))
		WallProcMesh->SetMaterial(0, Mat);
}

void AWallBuilderWallActor::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	// 当使用 Transform Gizmo 移动 Actor 时，更新 StartPoint 和 EndPoint
	FVector NewActorLoc = GetActorLocation();
	FVector OldCenter = (WallParams.StartPoint + WallParams.EndPoint) * 0.5f;
	FVector Delta = NewActorLoc - OldCenter;

	// 只有当位置真正改变时才更新
	if (!Delta.IsNearlyZero())
	{
		WallParams.StartPoint += Delta;
		WallParams.EndPoint += Delta;
		RebuildMesh();
	}
}

// ===== 墙角斜切 =====

void AWallBuilderWallActor::UpdateMiterAngles()
{
	if (!GetWorld() || !WallParams.bAutoMiter)
		return;

	UWorld* World = GetWorld();
	float Threshold = WallParams.Thickness * 1.5f; // 相邻判定阈值

	FVector MyStart = WallParams.StartPoint; MyStart.Z = 0;
	FVector MyEnd = WallParams.EndPoint; MyEnd.Z = 0;
	FVector MyDir = (MyEnd - MyStart).GetSafeNormal();

	// 查找起始端相邻墙体
	float StartMiter = 0.0f;
	float EndMiter = 0.0f;

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(World, AWallBuilderWallActor::StaticClass(), FoundActors);
	for (AActor* FoundActor : FoundActors)
	{
		AWallBuilderWallActor* OtherWall = Cast<AWallBuilderWallActor>(FoundActor);
		if (!OtherWall || OtherWall == this) continue;

		const FWallParameters& OtherParams = OtherWall->GetWallParameters();
		FVector OtherStart = OtherParams.StartPoint; OtherStart.Z = 0;
		FVector OtherEnd = OtherParams.EndPoint; OtherEnd.Z = 0;
		FVector OtherDir = (OtherEnd - OtherStart).GetSafeNormal();

		// 检查起始端是否连接
		float DistToStart = FMath::Min(FVector::Dist(MyStart, OtherStart), FVector::Dist(MyStart, OtherEnd));
		if (DistToStart < Threshold)
		{
			// 判断另一面墙的哪个端点连接，并获取指向连接点的方向
			FVector OtherDirAtJoint;
			if (FVector::Dist(MyStart, OtherStart) < Threshold)
				OtherDirAtJoint = (OtherEnd - OtherStart).GetSafeNormal(); // OtherStart连接，方向背向连接点
			else
				OtherDirAtJoint = (OtherStart - OtherEnd).GetSafeNormal(); // OtherEnd连接，方向背向连接点

			// MyDir 是背向起始端的，OtherDirAtJoint 也是背向连接点的
			// 计算两面墙在连接点处的夹角
			float Dot = FVector::DotProduct(MyDir, OtherDirAtJoint);
			float AngleRad = FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f));
			float AngleDeg = FMath::RadiansToDegrees(AngleRad);

			// 斜切角度 = (180 - 夹角) / 2
			StartMiter = -(180.0f - AngleDeg) * 0.5f;

			// 判断内角还是外角
			FVector Cross = FVector::CrossProduct(MyDir, OtherDirAtJoint);
			if (Cross.Z > 0) StartMiter = -StartMiter;
		}

		// 检查末端是否连接
		float DistToEnd = FMath::Min(FVector::Dist(MyEnd, OtherStart), FVector::Dist(MyEnd, OtherEnd));
		if (DistToEnd < Threshold)
		{
			FVector OtherDirAtJoint;
			if (FVector::Dist(MyEnd, OtherStart) < Threshold)
				OtherDirAtJoint = (OtherEnd - OtherStart).GetSafeNormal();
			else
				OtherDirAtJoint = (OtherStart - OtherEnd).GetSafeNormal();

			// MyDir 在末端是指向末端的，需要反转
			FVector MyDirAtEnd = -MyDir;
			float Dot = FVector::DotProduct(MyDirAtEnd, OtherDirAtJoint);
			float AngleRad = FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f));
			float AngleDeg = FMath::RadiansToDegrees(AngleRad);

			EndMiter = -(180.0f - AngleDeg) * 0.5f;
			FVector Cross = FVector::CrossProduct(MyDirAtEnd, OtherDirAtJoint);
			if (Cross.Z > 0) EndMiter = -EndMiter;
		}
	}

	WallParams.StartMiterAngle = StartMiter;
	WallParams.EndMiterAngle = EndMiter;
}

void AWallBuilderWallActor::UpdateWallAndNeighbors(AWallBuilderWallActor* TargetWall, UWorld* World)
{
	if (!TargetWall || !World) return;

	float Threshold = TargetWall->GetWallParameters().Thickness * 1.5f;
	FVector TargetStart = TargetWall->WallParams.StartPoint; TargetStart.Z = 0;
	FVector TargetEnd = TargetWall->WallParams.EndPoint; TargetEnd.Z = 0;

	// 更新目标墙
	TargetWall->UpdateMiterAngles();
	TargetWall->RebuildMesh();

	// 更新相邻墙
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(World, AWallBuilderWallActor::StaticClass(), FoundActors);
	for (AActor* FoundActor : FoundActors)
	{
		AWallBuilderWallActor* OtherWall = Cast<AWallBuilderWallActor>(FoundActor);
		if (!OtherWall || OtherWall == TargetWall) continue;

		FVector OtherStart = OtherWall->WallParams.StartPoint; OtherStart.Z = 0;
		FVector OtherEnd = OtherWall->WallParams.EndPoint; OtherEnd.Z = 0;

		float Dist = FMath::Min(
			FMath::Min(FVector::Dist(TargetStart, OtherStart), FVector::Dist(TargetStart, OtherEnd)),
			FMath::Min(FVector::Dist(TargetEnd, OtherStart), FVector::Dist(TargetEnd, OtherEnd))
		);

		if (Dist < Threshold)
		{
			OtherWall->UpdateMiterAngles();
			OtherWall->RebuildMesh();
		}
	}
}
