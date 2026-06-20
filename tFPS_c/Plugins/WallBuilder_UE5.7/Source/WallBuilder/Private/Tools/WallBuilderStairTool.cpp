// Copyright Epic Games, Inc. All Rights Reserved.

#include "WallBuilderStairTool.h"
#include "WallBuilderEditorMode.h"
#include "WallBuilderWallActor.h"
#include "WallBuilderStairActor.h"
#include "WallBuilderRampActor.h"
#include "InteractiveToolManager.h"
#include "BaseBehaviors/ClickDragBehavior.h"
#include "BaseBehaviors/MouseHoverBehavior.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "Engine/HitResult.h"
#include "CanvasTypes.h"
#include "CanvasItem.h"

#define LOCTEXT_NAMESPACE "WallBuilderStairTool"

UInteractiveTool* UWallBuilderStairToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UWallBuilderStairTool* NewTool = NewObject<UWallBuilderStairTool>(SceneState.ToolManager);
	NewTool->SetWorld(SceneState.World);
	if (ParentMode) NewTool->SetParentMode(ParentMode);
	return NewTool;
}

UWallBuilderStairToolProperties::UWallBuilderStairToolProperties()
{
	StairWidth = 120;
	StairHeight = 150;
	StepCount = 10;
	StairMaterial = nullptr;
	bEnableAngleSnap = false;
	SnapAngle = 15;
	bEnableLengthSnap = false;
	SnapLength = 100;
	bShowAnchors = false;
	bUndo = false;
	bRedo = false;
	CurrentLength = 0.0f;
	StairCount = 0;
	StatusText = TEXT("Click to set start point");
}

void UWallBuilderStairTool::SetWorld(UWorld* World)
{
	check(World);
	TargetWorld = World;
}

void UWallBuilderStairTool::SetParentMode(UWallBuilderEditorMode* InMode)
{
	ParentMode = InMode;
}

void UWallBuilderStairTool::RefreshAnchorsFromWorld()
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

void UWallBuilderStairTool::Setup()
{
	UInteractiveTool::Setup();

	UClickDragInputBehavior* ClickDragBehavior = NewObject<UClickDragInputBehavior>();
	ClickDragBehavior->Initialize(this);
	AddInputBehavior(ClickDragBehavior);

	// 悬停行为 - 用于锚点高亮
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
			bHasHighlightedAnchor = false;
			if (Properties->bShowAnchors)
			{
				for (const FVector& AP : PersistentAnchors)
				{
					if (FVector::Dist2D(HitPos, AP) < 50.0f)
					{
						HighlightedAnchor = AP;
						bHasHighlightedAnchor = true;
						break;
					}
				}
			}
		}
	};
	HoverBehavior->OnUpdateHoverFunc = [this](const FInputDeviceRay& DevicePos) -> bool
	{
		FVector HitPos;
		FInputRayHit Hit = FindRayHit(DevicePos.WorldRay, HitPos);
		if (Hit.bHit)
		{
			bHasHighlightedAnchor = false;
			if (Properties->bShowAnchors)
			{
				for (const FVector& AP : PersistentAnchors)
				{
					if (FVector::Dist2D(HitPos, AP) < 50.0f)
					{
						HighlightedAnchor = AP;
						bHasHighlightedAnchor = true;
						break;
					}
				}
			}
			if (bIsDrawing && bDragActive)
			{
				FVector Pos;
				if (Properties->bShowAnchors)
				{
					bool bSnapped = false;
					Pos = SnapToAnchor(HitPos, bSnapped);
					if (!bSnapped) Pos = SnapEndPoint(HitPos);
				}
				else
				{
					Pos = SnapEndPoint(HitPos);
				}
				EndPoint = Pos;
				Properties->CurrentLength = FVector::Dist2D(StartPoint, EndPoint);
			}
		}
		return true;
	};
	HoverBehavior->OnEndHoverFunc = [this]()
	{
		bHasHighlightedAnchor = false;
	};
	AddInputBehavior(HoverBehavior);

	Properties = NewObject<UWallBuilderStairToolProperties>(this, "Stair Settings");
	AddToolPropertySource(Properties);

	// 从 EditorMode 恢复持久化参数
	if (ParentMode)
	{
		Properties->StairWidth = ParentMode->SavedStairWidth;
		Properties->StairHeight = ParentMode->SavedStairHeight;
		Properties->StepCount = ParentMode->SavedStepCount;
		Properties->StairMaterial = ParentMode->SavedStairMaterial;
		Properties->bEnableAngleSnap = ParentMode->SavedStairEnableAngleSnap;
		Properties->SnapAngle = ParentMode->SavedStairSnapAngle;
		Properties->bEnableLengthSnap = ParentMode->SavedStairEnableLengthSnap;
		Properties->SnapLength = ParentMode->SavedStairSnapLength;
		Properties->bShowAnchors = ParentMode->SavedStairShowAnchors;
	}

	RefreshAnchorsFromWorld();
}

FInputRayHit UWallBuilderStairTool::CanBeginClickDragSequence(const FInputDeviceRay& PressPos)
{
	FVector Temp;
	return FindRayHit(PressPos.WorldRay, Temp);
}

void UWallBuilderStairTool::OnClickPress(const FInputDeviceRay& PressPos)
{
	FVector HitPos;
	FInputRayHit HitResult = FindRayHit(PressPos.WorldRay, HitPos);
	if (!HitResult.bHit) return;

	bool bSnapped = false;
	StartPoint = Properties->bShowAnchors ? SnapToAnchor(HitPos, bSnapped) : HitPos;
	EndPoint = StartPoint;
	bIsDrawing = true;
	bDragActive = true;
	Properties->StatusText = TEXT("Drag to set length");
}

void UWallBuilderStairTool::OnClickDrag(const FInputDeviceRay& DragPos)
{
	if (!bDragActive) return;

	FVector HitPos;
	if (FindRayHit(DragPos.WorldRay, HitPos).bHit)
	{
		FVector Pos;
		if (Properties->bShowAnchors)
		{
			bool bSnapped = false;
			Pos = SnapToAnchor(HitPos, bSnapped);
			if (!bSnapped) Pos = SnapEndPoint(HitPos);
		}
		else
		{
			Pos = SnapEndPoint(HitPos);
		}
		EndPoint = Pos;
		Properties->CurrentLength = FVector::Dist2D(StartPoint, EndPoint);
	}
}

void UWallBuilderStairTool::OnClickRelease(const FInputDeviceRay& ReleasePos)
{
	if (!bDragActive) return;
	bDragActive = false;

	if (Properties->CurrentLength >= 10.0f)
		SpawnStairActor();

	StopDrawing();
}

void UWallBuilderStairTool::OnTerminateDragSequence()
{
	if (bDragActive)
	{
		bDragActive = false;
		StopDrawing();
	}
}

void UWallBuilderStairTool::OnPropertyModified(UObject* PropertySet, FProperty* Property)
{
	if (PropertySet == Properties)
	{
		// 处理撤回操作
		if (Properties->bUndo)
		{
			Properties->bUndo = false;
			UWallBuilderCommandHistory::Get()->Undo();
			if (Properties->bShowAnchors)
				RefreshAnchorsFromWorld();
			return;
		}

		// 处理重做操作
		if (Properties->bRedo)
		{
			Properties->bRedo = false;
			UWallBuilderCommandHistory::Get()->Redo();
			if (Properties->bShowAnchors)
				RefreshAnchorsFromWorld();
			return;
		}

		if (Properties->bShowAnchors)
			RefreshAnchorsFromWorld();

		// 保存参数到 EditorMode
		if (ParentMode)
		{
			ParentMode->SavedStairWidth = Properties->StairWidth;
			ParentMode->SavedStairHeight = Properties->StairHeight;
			ParentMode->SavedStepCount = Properties->StepCount;
			ParentMode->SavedStairMaterial = Properties->StairMaterial;
			ParentMode->SavedStairEnableAngleSnap = Properties->bEnableAngleSnap;
			ParentMode->SavedStairSnapAngle = Properties->SnapAngle;
			ParentMode->SavedStairEnableLengthSnap = Properties->bEnableLengthSnap;
			ParentMode->SavedStairSnapLength = Properties->SnapLength;
			ParentMode->SavedStairShowAnchors = Properties->bShowAnchors;
		}
	}
}

void UWallBuilderStairTool::SpawnStairActor()
{
	if (!TargetWorld) return;

	// 创建命令
	USpawnStairCommand* Command = NewObject<USpawnStairCommand>(this);
	Command->TargetWorld = TargetWorld;
	Command->StairParams.StartPoint = StartPoint;
	Command->StairParams.EndPoint = EndPoint;
	Command->StairParams.Width = (float)Properties->StairWidth;
	Command->StairParams.TotalHeight = (float)Properties->StairHeight;
	Command->StairParams.StepCount = Properties->StepCount;
	Command->StairParams.StairMaterial = Properties->StairMaterial;

	// 执行命令并记录到历史
	UWallBuilderCommandHistory::Get()->ExecuteCommand(Command);

	// 记录生成的楼梯
	if (Command->SpawnedStair.IsValid())
	{
		Command->SpawnedStair->SetActorLabel(FString::Printf(TEXT("Stair_%d"), SpawnedStairs.Num()));
		SpawnedStairs.Add(Command->SpawnedStair.Get());
		Properties->StairCount = SpawnedStairs.Num();

		PersistentAnchors.AddUnique(StartPoint);
		PersistentAnchors.AddUnique(EndPoint);
	}
}

void UWallBuilderStairTool::StopDrawing()
{
	bIsDrawing = false;
	StartPoint = FVector::ZeroVector;
	EndPoint = FVector::ZeroVector;
	Properties->CurrentLength = 0.0f;

	if (SpawnedStairs.Num() > 0)
		Properties->StatusText = FString::Printf(TEXT("Done. %d stairs. Click to draw new."), SpawnedStairs.Num());
	else
		Properties->StatusText = TEXT("Click to set start point");
}

FInputRayHit UWallBuilderStairTool::FindRayHit(const FRay& WorldRay, FVector& HitPos)
{
	FCollisionObjectQueryParams QueryParams(FCollisionObjectQueryParams::AllObjects);
	FHitResult Result;
	if (TargetWorld->LineTraceSingleByObjectType(Result, WorldRay.Origin, WorldRay.PointAt(999999), QueryParams))
	{
		HitPos = Result.ImpactPoint;
		return FInputRayHit(Result.Distance);
	}

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

FVector UWallBuilderStairTool::SnapEndPoint(const FVector& RawEnd) const
{
	FVector Dir2D = RawEnd - StartPoint;
	float Dist = Dir2D.Size2D();
	if (Dist < 1.0f) return RawEnd;

	if (Properties->bShowAnchors)
	{
		bool bSnapped = false;
		FVector AnchorSnap = SnapToAnchor(RawEnd, bSnapped);
		if (bSnapped) return AnchorSnap;
	}

	float AngleDeg = FMath::RadiansToDegrees(FMath::Atan2(Dir2D.Y, Dir2D.X));
	if (Properties->bEnableAngleSnap)
	{
		float SnapStep = (float)Properties->SnapAngle;
		float SnappedAng = FMath::RoundToFloat(AngleDeg / SnapStep) * SnapStep;
		if (FMath::Abs(AngleDeg - SnappedAng) <= SnapStep * 0.5f)
			AngleDeg = SnappedAng;
	}

	float AngleRad = FMath::DegreesToRadians(AngleDeg);
	float FinalDist = Dist;
	if (Properties->bEnableLengthSnap)
	{
		float SnapUnit = (float)Properties->SnapLength;
		FinalDist = FMath::RoundToFloat(Dist / SnapUnit) * SnapUnit;
		if (FinalDist < 1.0f) FinalDist = SnapUnit;
	}

	return StartPoint + FVector(FMath::Cos(AngleRad), FMath::Sin(AngleRad), 0.0f) * FinalDist;
}

FVector UWallBuilderStairTool::SnapToAnchor(const FVector& RawPos, bool& bOutSnapped) const
{
	bOutSnapped = false;
	float BestDist = 50.0f;
	FVector Best = RawPos;
	for (const FVector& AP : PersistentAnchors)
	{
		float D = FVector::Dist2D(RawPos, AP);
		if (D < BestDist) { BestDist = D; Best = AP; bOutSnapped = true; }
	}
	return Best;
}

void UWallBuilderStairTool::DrawAnchorCross(FPrimitiveDrawInterface* PDI, const FVector& Pos, FColor Col) const
{
	const float S = 15.0f;
	PDI->DrawLine(Pos + FVector( S,  S, 0), Pos + FVector(-S, -S, 0), Col, SDPG_Foreground, 2.0f, 0.0f, true);
	PDI->DrawLine(Pos + FVector( S, -S, 0), Pos + FVector(-S,  S, 0), Col, SDPG_Foreground, 2.0f, 0.0f, true);
	PDI->DrawLine(Pos + FVector( S, 0, 0), Pos + FVector(-S, 0, 0), Col, SDPG_Foreground, 2.0f, 0.0f, true);
	PDI->DrawLine(Pos + FVector(0,  S, 0), Pos + FVector(0, -S, 0), Col, SDPG_Foreground, 2.0f, 0.0f, true);
	PDI->DrawPoint(Pos, Col, 6.0f, SDPG_Foreground);
}

void UWallBuilderStairTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	FPrimitiveDrawInterface* PDI = RenderAPI->GetPrimitiveDrawInterface();

	if (Properties->bShowAnchors && PersistentAnchors.Num() > 0)
	{
		for (const FVector& AP : PersistentAnchors)
			DrawAnchorCross(PDI, AP, FColor::Yellow);

		// 绘制高亮锚点
		if (bHasHighlightedAnchor)
			DrawAnchorCross(PDI, HighlightedAnchor, FColor::Red);
	}

	if (!bIsDrawing) return;

	float Len = FVector::Dist2D(StartPoint, EndPoint);
	if (Len < 1.0f) return;

	FVector Dir = (EndPoint - StartPoint).GetSafeNormal2D();
	FVector Right = FVector::CrossProduct(FVector::UpVector, Dir) * (Properties->StairWidth * 0.5f);
	float H = (float)Properties->StairHeight;

	FColor Blue(100, 200, 255);

	// 绘制预览线框
	FVector V0 = StartPoint - Right;
	FVector V1 = StartPoint + Right;
	FVector V2 = EndPoint + Right;
	FVector V3 = EndPoint - Right;
	FVector V4 = V0 + FVector(0, 0, H);
	FVector V5 = V1 + FVector(0, 0, H);
	FVector V6 = V2 + FVector(0, 0, H);
	FVector V7 = V3 + FVector(0, 0, H);

	PDI->DrawLine(V0, V1, Blue, SDPG_Foreground, 2.0f, 0.0f, true);
	PDI->DrawLine(V1, V2, Blue, SDPG_Foreground, 2.0f, 0.0f, true);
	PDI->DrawLine(V2, V3, Blue, SDPG_Foreground, 2.0f, 0.0f, true);
	PDI->DrawLine(V3, V0, Blue, SDPG_Foreground, 2.0f, 0.0f, true);
	PDI->DrawLine(V4, V5, Blue, SDPG_Foreground, 2.0f, 0.0f, true);
	PDI->DrawLine(V5, V6, Blue, SDPG_Foreground, 2.0f, 0.0f, true);
	PDI->DrawLine(V6, V7, Blue, SDPG_Foreground, 2.0f, 0.0f, true);
	PDI->DrawLine(V7, V4, Blue, SDPG_Foreground, 2.0f, 0.0f, true);
	PDI->DrawLine(V0, V4, Blue, SDPG_Foreground, 2.0f, 0.0f, true);
	PDI->DrawLine(V1, V5, Blue, SDPG_Foreground, 2.0f, 0.0f, true);
	PDI->DrawLine(V2, V6, Blue, SDPG_Foreground, 2.0f, 0.0f, true);
	PDI->DrawLine(V3, V7, Blue, SDPG_Foreground, 2.0f, 0.0f, true);
}

void UWallBuilderStairTool::DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI)
{
	if (!bIsDrawing || !Canvas) return;

	float Len = FVector::Dist2D(StartPoint, EndPoint);
	if (Len < 1.0f) return;

	FString LenText = FString::Printf(TEXT("Length: %.0f cm"), Len);
	FString HText = FString::Printf(TEXT("Height: %d cm, Steps: %d"), Properties->StairHeight, Properties->StepCount);

	UFont* Fnt = GEngine->GetSmallFont();
	Canvas->DrawShadowedString(20.0f, 60.0f, *LenText, Fnt, FLinearColor::White);
	Canvas->DrawShadowedString(20.0f, 80.0f, *HText, Fnt, FLinearColor::White);
}

#undef LOCTEXT_NAMESPACE
