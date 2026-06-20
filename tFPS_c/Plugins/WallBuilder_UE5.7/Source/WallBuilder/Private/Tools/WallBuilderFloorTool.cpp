// Copyright Epic Games, Inc. All Rights Reserved.

#include "WallBuilderFloorTool.h"
#include "WallBuilderEditorMode.h"
#include "WallBuilderWallActor.h"
#include "InteractiveToolManager.h"
#include "BaseBehaviors/ClickDragBehavior.h"
#include "BaseBehaviors/MouseHoverBehavior.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/Canvas.h"
#include "CanvasItem.h"
#include "CollisionQueryParams.h"

// ===== Properties =====

UWallBuilderFloorToolProperties::UWallBuilderFloorToolProperties()
{
	FloorThickness = 20;
	FloorHeight = 0;
	FloorMaterial = nullptr;
	bEnableGridSnap = true;
	GridSize = 10;
	bSnapToWallTops = true;
	SnapAnchorThreshold = 30;
	bFinishDrawing = false;
	bClosePolygon = false;
	VertexCount = 0;
	StatusText = TEXT("Click to place first vertex");
}

// ===== Tool Builder =====

UInteractiveTool* UWallBuilderFloorToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UWallBuilderFloorTool* NewTool = NewObject<UWallBuilderFloorTool>(SceneState.ToolManager);
	NewTool->SetWorld(SceneState.World);
	NewTool->SetParentMode(ParentMode);
	return NewTool;
}

// ===== Tool =====

void UWallBuilderFloorTool::SetWorld(UWorld* World)
{
	TargetWorld = World;
}

void UWallBuilderFloorTool::SetParentMode(UWallBuilderEditorMode* InMode)
{
	ParentMode = InMode;
}

void UWallBuilderFloorTool::Setup()
{
	UInteractiveTool::Setup();

	Properties = NewObject<UWallBuilderFloorToolProperties>(this);
	AddToolPropertySource(Properties);

	// 从持久化参数恢复
	if (ParentMode)
	{
		Properties->FloorThickness = ParentMode->SavedFloorThickness;
		Properties->FloorHeight = ParentMode->SavedFloorHeight;
		Properties->FloorMaterial = ParentMode->SavedFloorMaterial;
		Properties->bEnableGridSnap = ParentMode->SavedFloorGridSnap;
		Properties->GridSize = ParentMode->SavedFloorGridSize;
		Properties->bSnapToWallTops = ParentMode->SavedFloorSnapToWallTops;
		Properties->SnapAnchorThreshold = ParentMode->SavedFloorSnapThreshold;
	}

	// 注册点击拖拽行为
	UClickDragInputBehavior* ClickDragBehavior = NewObject<UClickDragInputBehavior>();
	ClickDragBehavior->Initialize(this);
	AddInputBehavior(ClickDragBehavior);

	// 注册悬停行为用于预览和锚点高亮
	ULocalMouseHoverBehavior* HoverBehavior = NewObject<ULocalMouseHoverBehavior>();
	HoverBehavior->Initialize();
	HoverBehavior->BeginHitTestFunc = [this](const FInputDeviceRay& PressPos) -> FInputRayHit
	{
		FVector HitPos;
		return FindRayHit(PressPos.WorldRay, HitPos);
	};
	HoverBehavior->OnUpdateHoverFunc = [this](const FInputDeviceRay& DevicePos) -> bool
	{
		FVector HitPos;
		FInputRayHit Hit = FindRayHit(DevicePos.WorldRay, HitPos);
		if (Hit.bHit)
		{
			FVector Snapped = SnapToGrid(HitPos);
			Snapped.Z = (float)Properties->FloorHeight;

			// 检查锚点吸附
			bHasHighlightedAnchor = false;
			if (Properties->bSnapToWallTops)
			{
				bool bSnapped = false;
				FVector AnchorPos = SnapToAnchor(Snapped, bSnapped);
				if (bSnapped)
				{
					HoverPoint = AnchorPos;
					bHasHighlightedAnchor = true;
					HighlightedAnchor = AnchorPos;
				}
				else
				{
					HoverPoint = Snapped;
				}
			}
			else
			{
				HoverPoint = Snapped;
			}
			bHasHoverPoint = true;
			UpdatePreview();
		}
		return true;
	};
	HoverBehavior->OnEndHoverFunc = [this]()
	{
		bHasHoverPoint = false;
		bHasHighlightedAnchor = false;
	};
	AddInputBehavior(HoverBehavior);
}

void UWallBuilderFloorTool::Shutdown(EToolShutdownType ShutdownType)
{
	if (PreviewFloorActor)
	{
		PreviewFloorActor->Destroy();
		PreviewFloorActor = nullptr;
	}
	UInteractiveTool::Shutdown(ShutdownType);
}

FInputRayHit UWallBuilderFloorTool::FindRayHit(const FRay& WorldRay, FVector& HitPos)
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FloorToolTrace), true);
	FHitResult HitResult;
	bool bHit = TargetWorld->LineTraceSingleByChannel(HitResult, WorldRay.Origin, WorldRay.Origin + WorldRay.Direction * 10000.0f, ECC_Visibility, Params);
	if (bHit)
	{
		HitPos = HitResult.ImpactPoint;
		return FInputRayHit(HitResult.Distance);
	}
	return FInputRayHit();
}

FVector UWallBuilderFloorTool::SnapToGrid(const FVector& Pos) const
{
	if (!Properties->bEnableGridSnap || Properties->GridSize <= 0)
		return Pos;

	float Grid = (float)Properties->GridSize;
	return FVector(
		FMath::RoundToFloat(Pos.X / Grid) * Grid,
		FMath::RoundToFloat(Pos.Y / Grid) * Grid,
		Pos.Z
	);
}

void UWallBuilderFloorTool::RefreshAnchorsFromWorld()
{
	WallTopAnchors.Empty();
	if (!TargetWorld || !TargetWorld->PersistentLevel) return;

	for (AActor* Actor : TargetWorld->PersistentLevel->Actors)
	{
		if (!Actor || !IsValid(Actor)) continue;
		if (AWallBuilderWallActor* W = Cast<AWallBuilderWallActor>(Actor))
		{
			const FWallParameters& P = W->GetWallParameters();
			// 只收集墙顶端点（用于楼板吸附）
			FVector TopStart(P.StartPoint.X, P.StartPoint.Y, P.StartPoint.Z + P.Height);
			FVector TopEnd(P.EndPoint.X, P.EndPoint.Y, P.EndPoint.Z + P.Height);
			WallTopAnchors.AddUnique(TopStart);
			WallTopAnchors.AddUnique(TopEnd);
		}
	}
}

FVector UWallBuilderFloorTool::SnapToAnchor(const FVector& RawPos, bool& bOutSnapped) const
{
	bOutSnapped = false;
	float BestDist = (float)Properties->SnapAnchorThreshold;
	FVector Best = RawPos;

	for (const FVector& AP : WallTopAnchors)
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

// IClickDragBehaviorTarget
FInputRayHit UWallBuilderFloorTool::CanBeginClickDragSequence(const FInputDeviceRay& PressPos)
{
	FVector HitPos;
	return FindRayHit(PressPos.WorldRay, HitPos);
}

void UWallBuilderFloorTool::OnClickPress(const FInputDeviceRay& PressPos)
{
	FVector HitPos;
	FInputRayHit Hit = FindRayHit(PressPos.WorldRay, HitPos);
	if (!Hit.bHit) return;

	FVector SnappedPos = SnapToGrid(HitPos);
	SnappedPos.Z = (float)Properties->FloorHeight;

	// 锚点吸附优先
	if (Properties->bSnapToWallTops)
	{
		RefreshAnchorsFromWorld();
		bool bSnapped = false;
		FVector AnchorPos = SnapToAnchor(SnappedPos, bSnapped);
		if (bSnapped)
		{
			SnappedPos = AnchorPos;
			// 吸附到锚点时，自动更新楼板高度为锚点高度
			Properties->FloorHeight = FMath::RoundToFloat(AnchorPos.Z);
		}
	}

	if (CurrentVertices.Num() >= 3 && IsNearFirstVertex(SnappedPos))
	{
		ClosePolygon();
		return;
	}

	CurrentVertices.Add(SnappedPos);
	bIsDrawing = true;
	UpdatePreview();
	UpdateStatusText();
}

void UWallBuilderFloorTool::OnClickDrag(const FInputDeviceRay& DragPos)
{
	// 悬停行为已处理预览更新
}

void UWallBuilderFloorTool::OnClickRelease(const FInputDeviceRay& ReleasePos)
{
	bHasHoverPoint = false;
	bHasHighlightedAnchor = false;
}

void UWallBuilderFloorTool::UpdatePreview()
{
	if (CurrentVertices.Num() < 2)
	{
		if (PreviewFloorActor)
		{
			PreviewFloorActor->Destroy();
			PreviewFloorActor = nullptr;
		}
		return;
	}

	if (!PreviewFloorActor)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.bAllowDuringConstructionScript = true;
		PreviewFloorActor = TargetWorld->SpawnActor<AWallBuilderFloorActor>(AWallBuilderFloorActor::StaticClass(), FTransform::Identity, SpawnParams);
		PreviewFloorActor->SetActorLabel(TEXT("PreviewFloor"));
	}

	FFloorParameters FloorParams;
	FloorParams.Thickness = (float)Properties->FloorThickness;
	FloorParams.Height = (float)Properties->FloorHeight;
	FloorParams.FloorMaterial = Properties->FloorMaterial;
	PreviewFloorActor->SetFloorParameters(FloorParams);

	TArray<FVector> PreviewVerts = CurrentVertices;
	if (bHasHoverPoint && CurrentVertices.Num() >= 2)
		PreviewVerts.Add(HoverPoint);
	PreviewFloorActor->SetVertices(PreviewVerts);
}

void UWallBuilderFloorTool::UpdateStatusText()
{
	Properties->VertexCount = CurrentVertices.Num();
	if (CurrentVertices.Num() == 0)
		Properties->StatusText = TEXT("Click to place first vertex");
	else if (CurrentVertices.Num() == 1)
		Properties->StatusText = TEXT("Click to place second vertex");
	else if (CurrentVertices.Num() == 2)
		Properties->StatusText = TEXT("Click more vertices, or click near first to close");
	else
		Properties->StatusText = FString::Printf(TEXT("Vertices: %d - Click near first vertex to close"), CurrentVertices.Num());
}

bool UWallBuilderFloorTool::IsNearFirstVertex(const FVector& TestPoint) const
{
	if (CurrentVertices.Num() < 3) return false;
	return FVector::Dist2D(TestPoint, CurrentVertices[0]) < 20.0f;
}

void UWallBuilderFloorTool::ClosePolygon()
{
	if (CurrentVertices.Num() < 3) return;
	SpawnFloorActor();

	CurrentVertices.Empty();
	bIsDrawing = false;
	if (PreviewFloorActor)
	{
		PreviewFloorActor->Destroy();
		PreviewFloorActor = nullptr;
	}
	UpdateStatusText();
}

void UWallBuilderFloorTool::SpawnFloorActor()
{
	if (CurrentVertices.Num() < 3) return;

	FActorSpawnParameters SpawnParams;
	SpawnParams.bAllowDuringConstructionScript = true;
	AWallBuilderFloorActor* FloorActor = TargetWorld->SpawnActor<AWallBuilderFloorActor>(AWallBuilderFloorActor::StaticClass(), FTransform::Identity, SpawnParams);

	if (FloorActor)
	{
		FFloorParameters FloorParams;
		FloorParams.Thickness = (float)Properties->FloorThickness;
		FloorParams.Height = (float)Properties->FloorHeight;
		FloorParams.FloorMaterial = Properties->FloorMaterial;
		FloorActor->SetFloorParameters(FloorParams);
		FloorActor->SetVertices(CurrentVertices);
		SpawnedFloors.Add(FloorActor);
	}
}

void UWallBuilderFloorTool::FinishDrawing()
{
	if (CurrentVertices.Num() >= 3)
		ClosePolygon();
	else
	{
		CurrentVertices.Empty();
		bIsDrawing = false;
		if (PreviewFloorActor)
		{
			PreviewFloorActor->Destroy();
			PreviewFloorActor = nullptr;
		}
		UpdateStatusText();
	}
}

void UWallBuilderFloorTool::OnPropertyModified(UObject* PropertySet, FProperty* Property)
{
	// 将参数保存到持久化
	if (ParentMode)
	{
		ParentMode->SavedFloorThickness = Properties->FloorThickness;
		ParentMode->SavedFloorHeight = Properties->FloorHeight;
		ParentMode->SavedFloorMaterial = Properties->FloorMaterial;
		ParentMode->SavedFloorGridSnap = Properties->bEnableGridSnap;
		ParentMode->SavedFloorGridSize = Properties->GridSize;
		ParentMode->SavedFloorSnapToWallTops = Properties->bSnapToWallTops;
		ParentMode->SavedFloorSnapThreshold = Properties->SnapAnchorThreshold;
	}

	if (!Property) return;
	FName PropName = Property->GetFName();

	if (PropName == GET_MEMBER_NAME_CHECKED(UWallBuilderFloorToolProperties, bFinishDrawing))
	{
		Properties->bFinishDrawing = false;
		FinishDrawing();
	}
	else if (PropName == GET_MEMBER_NAME_CHECKED(UWallBuilderFloorToolProperties, bClosePolygon))
	{
		Properties->bClosePolygon = false;
		if (CurrentVertices.Num() >= 3)
			ClosePolygon();
	}
	else
	{
		UpdatePreview();
	}
}

// ===== Render =====

void UWallBuilderFloorTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	FPrimitiveDrawInterface* PDI = RenderAPI->GetPrimitiveDrawInterface();

	// 绘制墙顶锚点
	if (Properties->bSnapToWallTops)
	{
		RefreshAnchorsFromWorld();
		for (const FVector& AP : WallTopAnchors)
		{
			PDI->DrawPoint(AP, FColor::Cyan, 8.0f, SDPG_Foreground);
		}
	}

	// 高亮吸附的锚点
	if (bHasHighlightedAnchor)
	{
		PDI->DrawPoint(HighlightedAnchor, FColor::Magenta, 15.0f, SDPG_Foreground);
	}

	// 绘制已放置的顶点连线
	for (int32 i = 0; i < CurrentVertices.Num() - 1; i++)
		DrawLine(PDI, CurrentVertices[i], CurrentVertices[i + 1], FColor::Yellow);

	// 鼠标到最后顶点的预览线
	if (bHasHoverPoint && CurrentVertices.Num() > 0)
		DrawLine(PDI, CurrentVertices.Last(), HoverPoint, FColor::Green);

	// 闭合提示线
	if (CurrentVertices.Num() >= 3)
	{
		DrawLine(PDI, CurrentVertices.Last(), CurrentVertices[0], FColor(255, 128, 0));
		PDI->DrawPoint(CurrentVertices[0], FColor::Red, 15.0f, SDPG_Foreground);
	}
}

void UWallBuilderFloorTool::DrawLine(FPrimitiveDrawInterface* PDI, const FVector& Start, const FVector& End, FColor Color) const
{
	PDI->DrawLine(Start, End, Color, SDPG_Foreground, 2.0f, 0.0f, true);
}

void UWallBuilderFloorTool::DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI)
{
	if (Canvas && CurrentVertices.Num() > 0)
	{
		FString Text = FString::Printf(TEXT("Vertices: %d"), CurrentVertices.Num());
		if (CurrentVertices.Num() >= 3)
			Text += TEXT(" (Click near red dot to close)");

		FCanvasTextItem TextItem(FVector2D(10, 100), FText::FromString(Text), GEngine->GetLargeFont(), FColor::White);
		TextItem.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(TextItem);
	}
}
