// Copyright Epic Games, Inc. All Rights Reserved.

#include "WallBuilderFloorActor.h"
#include "ProceduralMeshComponent.h"

AWallBuilderFloorActor::AWallBuilderFloorActor()
{
	PrimaryActorTick.bCanEverTick = false;

	FloorMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("FloorMesh"));
	RootComponent = FloorMesh;

	FloorMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	FloorMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
	FloorMesh->bUseComplexAsSimpleCollision = true;

	FloorParams.Thickness = 20.0f;
	FloorParams.Height = 0.0f;
}

void AWallBuilderFloorActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();
}

void AWallBuilderFloorActor::SetFloorParameters(const FFloorParameters& InParams)
{
	FloorParams = InParams;
	RebuildMesh();
}

void AWallBuilderFloorActor::SetVertices(const TArray<FVector>& InVertices)
{
	Vertices = InVertices;
	RebuildMesh();
}

void AWallBuilderFloorActor::AddVertex(const FVector& Vertex)
{
	Vertices.Add(Vertex);
	RebuildMesh();
}

void AWallBuilderFloorActor::UpdateVertex(int32 Index, const FVector& NewVertex)
{
	if (Vertices.IsValidIndex(Index))
	{
		Vertices[Index] = NewVertex;
		RebuildMesh();
	}
}

FVector AWallBuilderFloorActor::GetFloorCenter() const
{
	if (Vertices.Num() == 0) return FVector::ZeroVector;
	
	FVector Sum = FVector::ZeroVector;
	for (const FVector& V : Vertices)
		Sum += V;
	return Sum / Vertices.Num();
}

bool AWallBuilderFloorActor::IsPolygonValid() const
{
	return Vertices.Num() >= 3;
}

TArray<int32> AWallBuilderFloorActor::Triangulate() const
{
	TArray<int32> Indices;
	const int32 N = Vertices.Num();
	if (N < 3) return Indices;
	if (N == 3)
	{
		Indices.Add(0); Indices.Add(1); Indices.Add(2);
		return Indices;
	}

	// 2D 叉积（Z分量）：判断三角形绕序
	auto Cross2D = [&](const FVector& A, const FVector& B, const FVector& C) -> float
	{
		return (B.X - A.X) * (C.Y - A.Y) - (C.X - A.X) * (B.Y - A.Y);
	};

	// 判断多边形绕序，确保为 CCW
	float TotalArea = 0.0f;
	for (int32 i = 0; i < N; i++)
	{
		int32 j = (i + 1) % N;
		TotalArea += Vertices[i].X * Vertices[j].Y - Vertices[j].X * Vertices[i].Y;
	}
	bool bInputIsCCW = TotalArea > 0.0f;

	// 构建顶点索引列表（统一为 CCW 顺序）
	TArray<int32> VIdx;
	VIdx.Reserve(N);
	if (bInputIsCCW)
	{
		for (int32 i = 0; i < N; i++) VIdx.Add(i);
	}
	else
	{
		for (int32 i = N - 1; i >= 0; i--) VIdx.Add(i);
	}

	// 点是否在三角形内部（严格检测，用于判断 ear）
	auto PointInTriangle = [&](int32 AIdx, int32 BIdx, int32 CIdx, int32 PIdx) -> bool
	{
		const FVector& A = Vertices[AIdx];
		const FVector& B = Vertices[BIdx];
		const FVector& C = Vertices[CIdx];
		const FVector& P = Vertices[PIdx];

		float CrossAB_AP = Cross2D(A, B, P);
		float CrossBC_BP = Cross2D(B, C, P);
		float CrossCA_CP = Cross2D(C, A, P);

		// 对于 CCW 三角形，内部点三个叉积都 >= 0
		// 允许极小负值（浮点精度）
		const float Eps = -0.001f;
		return CrossAB_AP >= Eps && CrossBC_BP >= Eps && CrossCA_CP >= Eps;
	};

	// === Ear Clipping 主算法 ===
	TArray<int32> Working = VIdx;
	while (Working.Num() > 3)
	{
		int32 M = Working.Num();
		bool bFoundEar = false;

		for (int32 i = 0; i < M && !bFoundEar; i++)
		{
			int32 Prev = Working[(i - 1 + M) % M];
			int32 Curr = Working[i];
			int32 Next = Working[(i + 1) % M];

			// Curr 必须是凸顶点（CCW 多边形中 Cross2D > 0）
			float Cross = Cross2D(Vertices[Prev], Vertices[Curr], Vertices[Next]);
			if (Cross <= 0.0f) continue;

			// 检查是否有其他顶点在三角形 (Prev,Curr,Next) 内部
			bool bIsEar = true;
			for (int32 j = 0; j < M; j++)
			{
				int32 TestIdx = Working[j];
				if (TestIdx == Prev || TestIdx == Curr || TestIdx == Next)
					continue;
				if (PointInTriangle(Prev, Curr, Next, TestIdx))
				{
					bIsEar = false;
					break;
				}
			}

			if (bIsEar)
			{
				Indices.Add(Prev);
				Indices.Add(Curr);
				Indices.Add(Next);
				Working.RemoveAt(i);
				bFoundEar = true;
			}
		}

		if (!bFoundEar)
		{
			// 回退：扇形三角化（仅用于极端情况）
			UE_LOG(LogTemp, Warning, TEXT("FloorActor: Ear clipping stuck, using fan fallback"));
			for (int32 i = 1; i < Working.Num() - 1; i++)
			{
				Indices.Add(Working[0]);
				Indices.Add(Working[i]);
				Indices.Add(Working[i + 1]);
			}
			break;
		}
	}

	// 最后三个顶点形成一个三角形
	if (Working.Num() == 3)
	{
		Indices.Add(Working[0]);
		Indices.Add(Working[1]);
		Indices.Add(Working[2]);
	}

	return Indices;
}

void AWallBuilderFloorActor::RebuildMesh()
{
	if (!FloorMesh || !IsPolygonValid()) return;
	FloorMesh->ClearAllMeshSections();

	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FProcMeshTangent> Tangents;
	TArray<FVector2D> UV0;
	TArray<FColor> Colors;

	const float UVScale = 0.01f;
	const float Thickness = FloorParams.Thickness;
	const float H = FloorParams.Height;

	// 获取多边形中心用于UV计算
	FVector Center = GetFloorCenter();

	// 添加双面四边形
	auto AddQuad = [&](const FVector& BL, const FVector& BR, const FVector& TR, const FVector& TL,
		const FVector& Normal, float U1, float U2, float V1, float V2)
	{
		int32 I = Verts.Num();
		Verts.Add(BL); Verts.Add(BR); Verts.Add(TR); Verts.Add(TL);

		// 正面
		Tris.Add(I); Tris.Add(I+2); Tris.Add(I+1);
		Tris.Add(I); Tris.Add(I+3); Tris.Add(I+2);
		// 反面
		Tris.Add(I+1); Tris.Add(I+2); Tris.Add(I);
		Tris.Add(I+2); Tris.Add(I+3); Tris.Add(I);

		Normals.Add(Normal); Normals.Add(Normal); Normals.Add(Normal); Normals.Add(Normal);

		FProcMeshTangent Tangent(FVector::CrossProduct(Normal, FVector::UpVector).GetSafeNormal(), false);
		if (FMath::Abs(FVector::DotProduct(Normal, FVector::UpVector)) > 0.99f)
			Tangent = FProcMeshTangent(FVector::CrossProduct(Normal, FVector::ForwardVector).GetSafeNormal(), false);
		Tangents.Add(Tangent); Tangents.Add(Tangent); Tangents.Add(Tangent); Tangents.Add(Tangent);

		UV0.Add(FVector2D(U1, V1)); UV0.Add(FVector2D(U2, V1));
		UV0.Add(FVector2D(U2, V2)); UV0.Add(FVector2D(U1, V2));
		Colors.Add(FColor::White); Colors.Add(FColor::White);
		Colors.Add(FColor::White); Colors.Add(FColor::White);
	};

	// 1. 顶面 (Z = H) — 双面渲染
	TArray<int32> TopTris = Triangulate();
	for (int32 Idx : TopTris)
	{
		const FVector& V = Vertices[Idx];
		Verts.Add(FVector(V.X, V.Y, H));
		Normals.Add(FVector::UpVector);
		Tangents.Add(FProcMeshTangent(FVector::ForwardVector, false));
		UV0.Add(FVector2D((V.X - Center.X)*UVScale, (V.Y - Center.Y)*UVScale));
		Colors.Add(FColor::White);
	}
	for (int32 i = 0; i < TopTris.Num(); i += 3)
	{
		// CCW (正面) + CW (背面)
		Tris.Add(i);   Tris.Add(i+1); Tris.Add(i+2);
		Tris.Add(i);   Tris.Add(i+2); Tris.Add(i+1);
	}

	// 2. 底面 (Z = H - Thickness) — 双面渲染
	int32 BaseIdx = Verts.Num();
	for (int32 Idx : TopTris)
	{
		const FVector& V = Vertices[Idx];
		Verts.Add(FVector(V.X, V.Y, H - Thickness));
		Normals.Add(FVector::DownVector);
		Tangents.Add(FProcMeshTangent(FVector::ForwardVector, false));
		UV0.Add(FVector2D((V.X - Center.X)*UVScale, (V.Y - Center.Y)*UVScale));
		Colors.Add(FColor::White);
	}
	for (int32 i = 0; i < TopTris.Num(); i += 3)
	{
		// CCW (反面视角的正面) + CW (反面视角的背面)
		Tris.Add(BaseIdx + i); Tris.Add(BaseIdx + i+2); Tris.Add(BaseIdx + i+1);
		Tris.Add(BaseIdx + i); Tris.Add(BaseIdx + i+1); Tris.Add(BaseIdx + i+2);
	}

	// 3. 侧面 - 连接顶面和底面的边
	int32 N = Vertices.Num();
	for (int32 i = 0; i < N; i++)
	{
		int32 Next = (i + 1) % N;
		const FVector& V1 = Vertices[i];
		const FVector& V2 = Vertices[Next];

		// 计算边法线（向外）
		FVector EdgeDir = V2 - V1;
		FVector EdgeNormal = FVector::CrossProduct(EdgeDir, FVector::UpVector).GetSafeNormal();

		// 四边形：V1底 -> V2底 -> V2顶 -> V1顶
		FVector BL(V1.X, V1.Y, H - Thickness);
		FVector BR(V2.X, V2.Y, H - Thickness);
		FVector TR(V2.X, V2.Y, H);
		FVector TL(V1.X, V1.Y, H);

		float EdgeLen = EdgeDir.Size();
		AddQuad(BL, BR, TR, TL, EdgeNormal, 0, EdgeLen*UVScale, 0, Thickness*UVScale);
	}

	// 创建网格
	FloorMesh->CreateMeshSection(0, Verts, Tris, Normals, UV0, Colors, Tangents, true);

	// 应用材质
	if (FloorParams.FloorMaterial)
		FloorMesh->SetMaterial(0, FloorParams.FloorMaterial);
}
