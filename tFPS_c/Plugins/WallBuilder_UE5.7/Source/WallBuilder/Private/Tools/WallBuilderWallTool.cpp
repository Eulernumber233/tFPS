// Copyright Epic Games, Inc. All Rights Reserved.

#include "WallBuilderWallTool.h"
#include "WallBuilderEditorMode.h"
#include "WallBuilderStairActor.h"
#include "WallBuilderRampActor.h"
#include "InteractiveToolManager.h"
#include "ToolBuilderUtil.h"
#include "BaseBehaviors/ClickDragBehavior.h"
#include "BaseBehaviors/MouseHoverBehavior.h"

#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "Engine/HitResult.h"
#include "Engine/Engine.h"
#include "CanvasTypes.h"
#include "CanvasItem.h"

#define LOCTEXT_NAMESPACE "WallBuilderWallTool"

//////////////////////////////////////////////////////////////////////////
// ToolBuilder
//////////////////////////////////////////////////////////////////////////

UInteractiveTool* UWallBuilderWallToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UWallBuilderWallTool* NewTool = NewObject<UWallBuilderWallTool>(SceneState.ToolManager);
	NewTool->SetWorld(SceneState.World);
	if (ParentMode)
	{
		NewTool->SetParentMode(ParentMode);
	}
	return NewTool;
}

//////////////////////////////////////////////////////////////////////////
// ToolProperties
//////////////////////////////////////////////////////////////////////////

UWallBuilderWallToolProperties::UWallBuilderWallToolProperties()
{
	WallHeight = 300;
	WallThickness = 20;
	WallMaterial = nullptr;
	CloseThreshold = 50;
	bEnableAngleSnap = false;
	SnapAngle = 15;
	bEnableLengthSnap = false;
	SnapLength = 100;
	bShowAnchors = false;
	bUndo = false;
	bRedo = false;
	CurrentLength = 0.0f;
	WallCount = 0;
	StatusText = TEXT("Click to set start point");
}

//////////////////////////////////////////////////////////////////////////
// Tool
//////////////////////////////////////////////////////////////////////////

void UWallBuilderWallTool::SetWorld(UWorld* World)
{
	check(World);
	TargetWorld = World;
}

void UWallBuilderWallTool::SetParentMode(UWallBuilderEditorMode* InMode)
{
	ParentMode = InMode;
}

// 扫描世界中的所有墙体 Actor（比 TActorIterator 更可靠）
void UWallBuilderWallTool::RefreshAnchorsFromWorld()
{
	PersistentAnchors.Empty();
	if (TargetWorld && TargetWorld->PersistentLevel)
	{
		for (AActor* Actor : TargetWorld->PersistentLevel->Actors)
		{
			if (Actor && IsValid(Actor))
			{
				if (AWallBuilderWallActor* W = Cast<AWallBuilderWallActor>(Actor))
				{
					const FWallParameters& P = W->GetWallParameters();
					PersistentAnchors.AddUnique(P.StartPoint);
					PersistentAnchors.AddUnique(P.EndPoint);
				}
				else if (AWallBuilderStairActor* S = Cast<AWallBuilderStairActor>(Actor))
				{
					const FStairParameters& P = S->GetStairParameters();
					PersistentAnchors.AddUnique(P.StartPoint);
					PersistentAnchors.AddUnique(P.EndPoint);
				}
				else if (AWallBuilderRampActor* R = Cast<AWallBuilderRampActor>(Actor))
				{
					const FRampParameters& P = R->GetRampParameters();
					PersistentAnchors.AddUnique(P.StartPoint);
					PersistentAnchors.AddUnique(P.EndPoint);
				}
			}
		}
	}
}

void UWallBuilderWallTool::Setup()
{
	UInteractiveTool::Setup();

	UClickDragInputBehavior* ClickDragBehavior = NewObject<UClickDragInputBehavior>();
	ClickDragBehavior->Initialize(this);
	AddInputBehavior(ClickDragBehavior);

	ULocalMouseHoverBehavior* HoverBehavior = NewObject<ULocalMouseHoverBehavior>();
	HoverBehavior->Initialize();
	HoverBehavior->BeginHitTestFunc = [this](const FInputDeviceRay& PressPos) -> FInputRayHit
	{
		FVector Temp;
		return FindRayHit(PressPos.WorldRay, Temp);
	};
	HoverBehavior->OnBeginHoverFunc = [this](const FInputDeviceRay& DevicePos)
	{
		FVector HitPos;
		FInputRayHit Hit = FindRayHit(DevicePos.WorldRay, HitPos);
		if (Hit.bHit)
		{
			// 仅开启显示锚点时检查锚点高亮
			bHasHighlightedAnchor = false;
			if (Properties->bShowAnchors)
			{
				for (const FVector& AP : PersistentAnchors)
				{
					if (FVector::Dist2D(HitPos, AP) < Properties->CloseThreshold)
					{
						HighlightedAnchor = AP;
						bHasHighlightedAnchor = true;
						break;
					}
				}
			}
			if (bIsDrawing && bHasValidStart)
			{
				FVector Pos = EndPoint;
				if (Properties->bShowAnchors)
				{
					bool bSnapped = false;
					Pos = SnapToAnchor(HitPos, bSnapped);
					if (!bSnapped) Pos = SnapEndPointWithLength(HitPos);
				} else {
 					Pos = SnapEndPointWithLength(HitPos);
 				}
 				EndPoint = Pos;
				UpdatePreview();
 			}
 		}
 	};
 	HoverBehavior->OnUpdateHoverFunc = [this](const FInputDeviceRay& DevicePos) -> bool
	{
		FVector HitPos;
		FInputRayHit Hit = FindRayHit(DevicePos.WorldRay, HitPos);
		if (Hit.bHit)
		{
			// 仅开启显示锚点时检查锚点高亮
			bHasHighlightedAnchor = false;
			if (Properties->bShowAnchors)
			{
				for (const FVector& AP : PersistentAnchors)
				{
					if (FVector::Dist2D(HitPos, AP) < Properties->CloseThreshold)
					{
						HighlightedAnchor = AP;
						bHasHighlightedAnchor = true;
						break;
					}
				}
			}
			if (bIsDrawing && bHasValidStart)
			{
				FVector Pos = EndPoint;
				if (Properties->bShowAnchors)
				{
					bool bSnapped = false;
					Pos = SnapToAnchor(HitPos, bSnapped);
					if (!bSnapped) Pos = SnapEndPointWithLength(HitPos);
				} else {
					Pos = SnapEndPointWithLength(HitPos);
				}
				EndPoint = Pos;				UpdatePreview();
			}
		}
		return true;
	};
	HoverBehavior->OnEndHoverFunc = [this]()
	{
		bHasHighlightedAnchor = false;
	};
	AddInputBehavior(HoverBehavior);

	Properties = NewObject<UWallBuilderWallToolProperties>(this, "Wall Settings");
	AddToolPropertySource(Properties);

	if (ParentMode)
	{
		Properties->WallHeight = ParentMode->SavedWallHeight;
		Properties->WallThickness = ParentMode->SavedWallThickness;
		Properties->WallMaterial = ParentMode->SavedWallMaterial;
		Properties->CloseThreshold = ParentMode->SavedCloseThreshold;
		Properties->bEnableAngleSnap = ParentMode->SavedEnableAngleSnap;
		Properties->SnapAngle = ParentMode->SavedSnapAngle;
		Properties->bEnableLengthSnap = ParentMode->SavedEnableLengthSnap;
		Properties->SnapLength = ParentMode->SavedSnapLength;
		Properties->bShowAnchors = ParentMode->SavedShowAnchors;
	}

	bIsDrawing = false;
	bHasValidStart = false;
	bHasFirstWall = false;
	bHasHighlightedAnchor = false;
	SpawnedWalls.Empty();
	PersistentAnchors.Empty();

	// 扫描世界中的已有墙体的端点作为锚点
	RefreshAnchorsFromWorld();
}

FInputRayHit UWallBuilderWallTool::CanBeginClickDragSequence(const FInputDeviceRay& PressPos)
{
	FVector Temp;
	return FindRayHit(PressPos.WorldRay, Temp);
}

void UWallBuilderWallTool::OnClickPress(const FInputDeviceRay& PressPos)
{
	FVector HitPos;
	FInputRayHit HitResult = FindRayHit(PressPos.WorldRay, HitPos);

	if (HitResult.bHit)
	{
		if (!bIsDrawing)
		{
			// 新绘制前保留旧墙锚点（所有世界中的墙体）
			if (Properties->bShowAnchors)
			{
				RefreshAnchorsFromWorld();
			}
			else
			{
				PersistentAnchors.Empty();
			}

			bHasHighlightedAnchor = false;
			bool bSnapped = false;
			StartPoint = Properties->bShowAnchors ? SnapToAnchor(HitPos, bSnapped) : HitPos;
			EndPoint = StartPoint;
			FirstStartPoint = StartPoint;
			bIsDrawing = true;
			bHasValidStart = true;
			bHasFirstWall = false;
			SpawnedWalls.Empty();
			Properties->StatusText = TEXT("Click to set end point");
		}
		else
		{
			if (bHasFirstWall && IsNearStartPoint(HitPos))
			{
				EndPoint = FirstStartPoint;
				SpawnWallActor();
				Properties->StatusText = FString::Printf(TEXT("Closed! %d walls"), SpawnedWalls.Num());
				StopDrawing();
				return;
			}

			bool bSnapped = false;
			FVector SnappedHit = Properties->bShowAnchors ? SnapToAnchor(HitPos, bSnapped) : HitPos;
			if (!bSnapped || !Properties->bShowAnchors)
			{
				EndPoint = SnapEndPointWithLength(HitPos);
			}
			else
			{
				EndPoint = SnappedHit;
			}
			SpawnWallActor();

			StartPoint = EndPoint;
			EndPoint = StartPoint;
			bHasFirstWall = true;
			Properties->StatusText = FString::Printf(TEXT("Click next point (%d walls)"), SpawnedWalls.Num());
		}
	}
}

void UWallBuilderWallTool::OnClickDrag(const FInputDeviceRay& DragPos)
{
	if (bIsDrawing)
	{
		FVector HitPos;
		FInputRayHit HitResult = FindRayHit(DragPos.WorldRay, HitPos);

		if (HitResult.bHit)
		{
			FVector Pos;
			if (Properties->bShowAnchors)
			{
				bool bSnapped = false;
				Pos = SnapToAnchor(HitPos, bSnapped);
				if (!bSnapped) Pos = SnapEndPointWithLength(HitPos);
			}
			else
			{
				Pos = SnapEndPointWithLength(HitPos);
			}
			EndPoint = Pos;
			UpdatePreview();
		}
	}
}

void UWallBuilderWallTool::OnClickRelease(const FInputDeviceRay& ReleasePos)
{
}

FInputRayHit UWallBuilderWallTool::FindRayHit(const FRay& WorldRay, FVector& HitPos)
{
	// 先用物理碰撞检测世界中的物体
	FCollisionObjectQueryParams QueryParams(FCollisionObjectQueryParams::AllObjects);
	FHitResult Result;
	bool bHitWorld = TargetWorld->LineTraceSingleByObjectType(Result, WorldRay.Origin, WorldRay.PointAt(999999), QueryParams);

	if (bHitWorld)
	{
		HitPos = Result.ImpactPoint;
		return FInputRayHit(Result.Distance);
	}

	// 未击中任何物体时，回退到 Z=0 地面平面（适用于顶视图等正交视角）
	float T = 0.0f;
	if (FMath::Abs(WorldRay.Direction.Z) > 0.001f)
	{
		T = -WorldRay.Origin.Z / WorldRay.Direction.Z;
		if (T > 0.0f)
		{
			HitPos = WorldRay.Origin + WorldRay.Direction * T;
			return FInputRayHit(T);
		}
	}

	return FInputRayHit();
}

void UWallBuilderWallTool::UpdatePreview()
{
	if (PreviewWallActor && bHasValidStart)
	{
		FWallParameters Params;
		Params.StartPoint = StartPoint;
		Params.EndPoint = EndPoint;
		Params.Height = Properties->WallHeight;
		Params.Thickness = Properties->WallThickness;
		Params.WallMaterial = Properties->WallMaterial;

		PreviewWallActor->SetWallParameters(Params);
		Properties->CurrentLength = FVector::Distance(StartPoint, EndPoint);

		if (bHasFirstWall && IsNearStartPoint(EndPoint))
		{
			Properties->StatusText = TEXT("Click to close the loop!");
		}
		else if (Properties->bEnableAngleSnap)
		{
			FVector Dir = (EndPoint - StartPoint).GetSafeNormal2D();
			float AngleDeg = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
			float Len = FVector::Dist2D(StartPoint, EndPoint);
			FString AngleInfo = FString::Printf(TEXT("Angle: %.1f deg"), AngleDeg);
			if (Properties->bEnableLengthSnap)
			{
				Properties->StatusText = FString::Printf(TEXT("Snap ON | %s | Length snap: %.0f cm (%d walls)"), *AngleInfo, Len, SpawnedWalls.Num());
			}
			else
			{
				Properties->StatusText = FString::Printf(TEXT("Snap ON | %s (%d walls)"), *AngleInfo, SpawnedWalls.Num());
			}
		}
		else if (Properties->bEnableLengthSnap)
		{
			float Len = FVector::Dist2D(StartPoint, EndPoint);
			Properties->StatusText = FString::Printf(TEXT("Length snap: %.0f / %d cm (%d walls)"), Len, Properties->SnapLength, SpawnedWalls.Num());
		}
	}
}

void UWallBuilderWallTool::SpawnWallActor()
{
	if (!TargetWorld || !bHasValidStart)
	{
		return;
	}

	float Length = FVector::Distance(StartPoint, EndPoint);
	if (Length < 1.0f)
	{
		return;
	}

	// 创建命令
	USpawnWallCommand* Command = NewObject<USpawnWallCommand>(this);
	Command->TargetWorld = TargetWorld;
	Command->WallParams.StartPoint = StartPoint;
	Command->WallParams.EndPoint = EndPoint;
	Command->WallParams.Height = Properties->WallHeight;
	Command->WallParams.Thickness = Properties->WallThickness;
	Command->WallParams.WallMaterial = Properties->WallMaterial;

	// 执行命令并记录到历史
	UWallBuilderCommandHistory::Get()->ExecuteCommand(Command);

	// 记录生成的墙体
	if (Command->SpawnedWall.IsValid())
	{
		Command->SpawnedWall->SetActorLabel(FString::Printf(TEXT("Wall_%d"), SpawnedWalls.Num()));
		SpawnedWalls.Add(Command->SpawnedWall.Get());
		Properties->WallCount = SpawnedWalls.Num();

		// 更新墙角斜切（新墙与相邻墙）
		AWallBuilderWallActor::UpdateWallAndNeighbors(Command->SpawnedWall.Get(), TargetWorld);
	}

	// 直接记录锚点，不依赖从世界读回
	PersistentAnchors.AddUnique(StartPoint);
	PersistentAnchors.AddUnique(EndPoint);
}

void UWallBuilderWallTool::StopDrawing()
{
	bIsDrawing = false;
	bHasValidStart = false;
	bHasFirstWall = false;
	StartPoint = FVector::ZeroVector;
	EndPoint = FVector::ZeroVector;
	FirstStartPoint = FVector::ZeroVector;
	Properties->CurrentLength = 0.0f;

	// 刷新锚点（包含所有世界中的墙体，不限当前会话）
	if (Properties->bShowAnchors)
	{
		RefreshAnchorsFromWorld();
	}
	else
	{
		PersistentAnchors.Empty();
	}

	if (SpawnedWalls.Num() > 0)
	{
		Properties->StatusText = FString::Printf(TEXT("Done. %d walls. Click to start new."), SpawnedWalls.Num());
	}
	else
	{
		Properties->StatusText = TEXT("Click to set start point");
	}
}

bool UWallBuilderWallTool::IsNearStartPoint(const FVector& TestPoint) const
{
	if (!bHasFirstWall) return false;
	return FVector::Dist2D(TestPoint, FirstStartPoint) < Properties->CloseThreshold;
}

FVector UWallBuilderWallTool::SnapToAnchor(const FVector& RawPos, bool& bOutSnapped) const
{
	bOutSnapped = false;
	float BestDist = Properties->CloseThreshold;
	FVector Best = RawPos;
	for (const FVector& AP : PersistentAnchors)
	{
		float D = FVector::Dist2D(RawPos, AP);
		if (D < BestDist)
		{
			BestDist = D;
			Best = AP;
			bOutSnapped = true;
		}
	}
	return Best;
}

FVector UWallBuilderWallTool::SnapEndPointWithLength(const FVector& RawEnd) const
{
	if (!bHasValidStart) return RawEnd;

	FVector Dir2D = RawEnd - StartPoint;
	float Dist = Dir2D.Size2D();
	if (Dist < 1.0f) return RawEnd;

	// 先角度吸附（如果启用）
	float AngleDeg = FMath::RadiansToDegrees(FMath::Atan2(Dir2D.Y, Dir2D.X));
	if (Properties->bEnableAngleSnap)
	{
		float SnapStep = (float)Properties->SnapAngle;
		float SnappedAng = FMath::RoundToFloat(AngleDeg / SnapStep) * SnapStep;
		if (FMath::Abs(AngleDeg - SnappedAng) <= SnapStep * 0.5f)
		{
			AngleDeg = SnappedAng;
		}
	}

	float AngleRad = FMath::DegreesToRadians(AngleDeg);

	// 再长度吸附（如果启用）
	float FinalDist = Dist;
	if (Properties->bEnableLengthSnap)
	{
		float SnapUnit = (float)Properties->SnapLength;
		FinalDist = FMath::RoundToFloat(Dist / SnapUnit) * SnapUnit;
		if (FinalDist < 1.0f) FinalDist = SnapUnit;
	}

	return StartPoint + FVector(FMath::Cos(AngleRad), FMath::Sin(AngleRad), 0.0f) * FinalDist;
}

void UWallBuilderWallTool::DrawAnchorCross(FPrimitiveDrawInterface* PDI, const FVector& Pos, FColor Col) const
{
	const float S = 15.0f;
	PDI->DrawLine(Pos + FVector( S,  S, 0), Pos + FVector(-S, -S, 0), Col, SDPG_Foreground, 2.0f, 0.0f, true);
	PDI->DrawLine(Pos + FVector( S, -S, 0), Pos + FVector(-S,  S, 0), Col, SDPG_Foreground, 2.0f, 0.0f, true);
	PDI->DrawLine(Pos + FVector( S, 0, 0), Pos + FVector(-S, 0, 0), Col, SDPG_Foreground, 2.0f, 0.0f, true);
	PDI->DrawLine(Pos + FVector(0,  S, 0), Pos + FVector(0, -S, 0), Col, SDPG_Foreground, 2.0f, 0.0f, true);
	PDI->DrawPoint(Pos, Col, 6.0f, SDPG_Foreground);
}

FVector UWallBuilderWallTool::SnapEndPoint(const FVector& RawEnd) const
{
	if (!Properties->bEnableAngleSnap || !bHasValidStart) return RawEnd;

	FVector Dir2D = RawEnd - StartPoint;
	float Dist = Dir2D.Size2D();
	if (Dist < 1.0f) return RawEnd;

	float AngleDeg = FMath::RadiansToDegrees(FMath::Atan2(Dir2D.Y, Dir2D.X));
	float SnapStep = Properties->SnapAngle;
	float SnappedAngle = FMath::RoundToFloat(AngleDeg / SnapStep) * SnapStep;
	float Diff = FMath::Abs(AngleDeg - SnappedAngle);

	if (Diff <= SnapStep * 0.5f)
	{
		float SnapRad = FMath::DegreesToRadians(SnappedAngle);
		return StartPoint + FVector(FMath::Cos(SnapRad), FMath::Sin(SnapRad), 0.0f) * Dist;
	}
	return RawEnd;
}

void UWallBuilderWallTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	FPrimitiveDrawInterface* PDI = RenderAPI->GetPrimitiveDrawInterface();

	// 仅开启显示锚点时画锚点
	if (Properties->bShowAnchors && PersistentAnchors.Num() > 0)
	{
		for (const FVector& AP : PersistentAnchors)
		{
			if (bHasHighlightedAnchor && AP.Equals(HighlightedAnchor))
			{
				// 高亮锚点：更大更亮
				const float BS = 25.0f;
				FColor HC(0, 255, 100);
				PDI->DrawLine(AP + FVector( BS,  BS, 0), AP + FVector(-BS, -BS, 0), HC, SDPG_Foreground, 3.0f, 0.0f, true);
				PDI->DrawLine(AP + FVector( BS, -BS, 0), AP + FVector(-BS,  BS, 0), HC, SDPG_Foreground, 3.0f, 0.0f, true);
				PDI->DrawLine(AP + FVector( BS, 0, 0), AP + FVector(-BS, 0, 0), HC, SDPG_Foreground, 3.0f, 0.0f, true);
				PDI->DrawLine(AP + FVector(0,  BS, 0), AP + FVector(0, -BS, 0), HC, SDPG_Foreground, 3.0f, 0.0f, true);
				PDI->DrawPoint(AP, HC, 10.0f, SDPG_Foreground);
			}
			else
			{
				DrawAnchorCross(PDI, AP, FColor::Yellow);
			}
		}
	}

	if (!bIsDrawing || !bHasValidStart)
	{
		if (bHasFirstWall && SpawnedWalls.Num() > 0)
		{
			PDI->DrawPoint(FirstStartPoint, FColor::Yellow, 10.0f, SDPG_Foreground);
		}
		return;
	}

	float Len = FVector::Dist2D(StartPoint, EndPoint);
	if (Len < 1.0f) return;

	FVector Direction = (EndPoint - StartPoint).GetSafeNormal2D();
	FVector Right = FVector::CrossProduct(FVector::UpVector, Direction).GetSafeNormal() * (Properties->WallThickness * 0.5f);
	float Height = Properties->WallHeight;

	FVector V0 = StartPoint - Right;
	FVector V1 = StartPoint + Right;
	FVector V2 = EndPoint + Right;
	FVector V3 = EndPoint - Right;
	FVector V4 = V0 + FVector(0, 0, Height);
	FVector V5 = V1 + FVector(0, 0, Height);
	FVector V6 = V2 + FVector(0, 0, Height);
	FVector V7 = V3 + FVector(0, 0, Height);

	FColor Orange(255, 165, 0);

	PDI->DrawLine(V0, V1, Orange, SDPG_Foreground, 2.0f, 0.0f, true);
	PDI->DrawLine(V1, V2, Orange, SDPG_Foreground, 2.0f, 0.0f, true);
	PDI->DrawLine(V2, V3, Orange, SDPG_Foreground, 2.0f, 0.0f, true);
	PDI->DrawLine(V3, V0, Orange, SDPG_Foreground, 2.0f, 0.0f, true);

	PDI->DrawLine(V4, V5, Orange, SDPG_Foreground, 2.0f, 0.0f, true);
	PDI->DrawLine(V5, V6, Orange, SDPG_Foreground, 2.0f, 0.0f, true);
	PDI->DrawLine(V6, V7, Orange, SDPG_Foreground, 2.0f, 0.0f, true);
	PDI->DrawLine(V7, V4, Orange, SDPG_Foreground, 2.0f, 0.0f, true);

	PDI->DrawLine(V0, V4, Orange, SDPG_Foreground, 2.0f, 0.0f, true);
	PDI->DrawLine(V1, V5, Orange, SDPG_Foreground, 2.0f, 0.0f, true);
	PDI->DrawLine(V2, V6, Orange, SDPG_Foreground, 2.0f, 0.0f, true);
	PDI->DrawLine(V3, V7, Orange, SDPG_Foreground, 2.0f, 0.0f, true);

	if (Properties->bShowAnchors)
	{
		DrawAnchorCross(PDI, StartPoint, FColor::Green);
		DrawAnchorCross(PDI, EndPoint, FColor(100, 200, 255));
	}

	if (bHasFirstWall && IsNearStartPoint(EndPoint))
	{
		if (Properties->bShowAnchors)
		{
			DrawAnchorCross(PDI, FirstStartPoint, FColor::Yellow);
		}
	}

	if (Properties->bEnableAngleSnap)
	{
		FVector RefEnd = StartPoint + Direction * (Len + 50.0f);
		PDI->DrawLine(StartPoint, RefEnd, FColor::Cyan, SDPG_Foreground, 1.0f, 0.0f, true);
	}
}

void UWallBuilderWallTool::DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI)
{
	if (!bIsDrawing || !bHasValidStart || !Canvas) return;

	float Len = FVector::Dist2D(StartPoint, EndPoint);
	if (Len < 1.0f) return;

	float Height = Properties->WallHeight;

	FString LenText = FString::Printf(TEXT("Length: %.0f cm"), Len);
	FString HText = FString::Printf(TEXT("Height: %.0f cm"), Height);

	UFont* Fnt = GEngine->GetSmallFont();
	FLinearColor C = FLinearColor::White;

	Canvas->DrawShadowedString(20.0f, 60.0f, *LenText, Fnt, C);
	Canvas->DrawShadowedString(20.0f, 80.0f, *HText, Fnt, C);
}

void UWallBuilderWallTool::OnPropertyModified(UObject* PropertySet, FProperty* Property)
{
	if (PropertySet == Properties)
	{
		// 处理撤回操作
		if (Properties->bUndo)
		{
			Properties->bUndo = false;
			UWallBuilderCommandHistory::Get()->Undo();
			// 刷新锚点显示
			if (Properties->bShowAnchors)
			{
				RefreshAnchorsFromWorld();
			}
			UpdatePreview();
			return;
		}

		// 处理重做操作
		if (Properties->bRedo)
		{
			Properties->bRedo = false;
			UWallBuilderCommandHistory::Get()->Redo();
			// 刷新锚点显示
			if (Properties->bShowAnchors)
			{
				RefreshAnchorsFromWorld();
			}
			UpdatePreview();
			return;
		}

		// 当开启显示锚点时，刷新所有锚点
		if (Properties->bShowAnchors)
		{
			RefreshAnchorsFromWorld();
		}

		if (ParentMode)
		{
			ParentMode->SavedWallHeight = Properties->WallHeight;
			ParentMode->SavedWallThickness = Properties->WallThickness;
			ParentMode->SavedWallMaterial = Properties->WallMaterial;
			ParentMode->SavedCloseThreshold = Properties->CloseThreshold;
			ParentMode->SavedEnableAngleSnap = Properties->bEnableAngleSnap;
			ParentMode->SavedSnapAngle = Properties->SnapAngle;
			ParentMode->SavedEnableLengthSnap = Properties->bEnableLengthSnap;
			ParentMode->SavedSnapLength = Properties->SnapLength;
			ParentMode->SavedShowAnchors = Properties->bShowAnchors;
		}

		UpdatePreview();
	}
}

#undef LOCTEXT_NAMESPACE
