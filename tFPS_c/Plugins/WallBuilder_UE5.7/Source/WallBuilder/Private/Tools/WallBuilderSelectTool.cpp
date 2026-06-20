// Copyright Epic Games, Inc. All Rights Reserved.

#include "WallBuilderSelectTool.h"
#include "WallBuilderEditorMode.h"
#include "WallBuilderWallActor.h"
#include "WallBuilderStairActor.h"
#include "WallBuilderRampActor.h"
#include "WallBuilderFloorActor.h"
#include "InteractiveToolManager.h"
#include "ToolBuilderUtil.h"
#include "BaseBehaviors/ClickDragBehavior.h"
#include "BaseBehaviors/MouseHoverBehavior.h"
#include "BaseGizmos/CombinedTransformGizmo.h"
#include "BaseGizmos/TransformProxy.h"
#include "BaseGizmos/TransformGizmoUtil.h"

#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "Engine/HitResult.h"
#include "Kismet/GameplayStatics.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "SceneManagement.h"

#define LOCTEXT_NAMESPACE "WallBuilderSelectTool"

//////////////////////////////////////////////////////////////////////////
// ToolBuilder
//////////////////////////////////////////////////////////////////////////

UInteractiveTool* UWallBuilderSelectToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UWallBuilderSelectTool* NewTool = NewObject<UWallBuilderSelectTool>(SceneState.ToolManager);
	NewTool->SetWorld(SceneState.World);
	if (ParentMode) NewTool->SetParentMode(ParentMode);
	return NewTool;
}

//////////////////////////////////////////////////////////////////////////
// ToolProperties
//////////////////////////////////////////////////////////////////////////

UWallBuilderSelectToolProperties::UWallBuilderSelectToolProperties()
{
	WallHeight = 300; WallThickness = 20; WallMaterial = nullptr;
	bDeleteWall = false;
	bShowAnchors = true; CloseThreshold = 50;
	bEnableLengthSnap = false; SnapLength = 100;
	bDoorMode = false; SelectedOpeningIndex = -1;
	OpeningWidth = 90; OpeningHeight = 210; OpeningZOffset = 0;
	bDeleteSelectedOpening = false;
	bUndo = false; bRedo = false;
	WallLength = 0.0f; SelectedBuildName = TEXT("None");
	StatusText = TEXT("Click a build element to select");
}

//////////////////////////////////////////////////////////////////////////
// Tool
//////////////////////////////////////////////////////////////////////////

void UWallBuilderSelectTool::SetWorld(UWorld* World)
{
	check(World);
	TargetWorld = World;
}

void UWallBuilderSelectTool::SetParentMode(UWallBuilderEditorMode* InMode) { ParentMode = InMode; }

void UWallBuilderSelectTool::Setup()
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
		bHasHighlightedAnchor = false; bHasPreviewOpening = false; PreviewOpeningWall = nullptr;
		if (Hit.bHit)
		{
			if (Properties->bShowAnchors)
			{
				for (const FVector& AP : PersistentAnchors)
				{
					if (FVector::Dist(HitPos, AP) < Properties->CloseThreshold)
					{
						HighlightedAnchor = AP; bHasHighlightedAnchor = true; break;
					}
				}
			}
			if (Properties->bDoorMode && !bHasHighlightedAnchor)
			{
				AWallBuilderWallActor* HitWall = nullptr;
				FInputRayHit WallHit = FindWallHit(DevicePos.WorldRay, HitWall);
				if (WallHit.bHit && HitWall)
				{
					FVector WallDir = HitWall->GetWallDirection();
					float WallLen = HitWall->GetWallLength();
					float HalfLen = WallLen * 0.5f;
					FVector LocalHit = HitPos - HitWall->GetActorLocation();
					float ProjDist = FVector::DotProduct(LocalHit, WallDir);
					float StartDist = ProjDist + HalfLen;
					StartDist = FMath::Clamp(StartDist, (float)Properties->OpeningWidth * 0.5f, WallLen - (float)Properties->OpeningWidth * 0.5f);
					PreviewOpeningPos = StartDist; PreviewOpeningWall = HitWall; bHasPreviewOpening = true;
				}
			}
		}
	};
	HoverBehavior->OnUpdateHoverFunc = [this](const FInputDeviceRay& DevicePos) -> bool
	{
		FVector HitPos;
		FInputRayHit Hit = FindRayHit(DevicePos.WorldRay, HitPos);
		bHasHighlightedAnchor = false; bHasPreviewOpening = false; PreviewOpeningWall = nullptr;
		if (Hit.bHit)
		{
			if (Properties->bShowAnchors)
			{
				for (const FVector& AP : PersistentAnchors)
				{
					if (FVector::Dist(HitPos, AP) < Properties->CloseThreshold)
					{
						HighlightedAnchor = AP; bHasHighlightedAnchor = true; break;
					}
				}
			}
			if (Properties->bDoorMode && !bHasHighlightedAnchor)
			{
				AWallBuilderWallActor* HitWall = nullptr;
				FInputRayHit WallHit = FindWallHit(DevicePos.WorldRay, HitWall);
				if (WallHit.bHit && HitWall)
				{
					FVector WallDir = HitWall->GetWallDirection();
					float WallLen = HitWall->GetWallLength();
					float HalfLen = WallLen * 0.5f;
					FVector LocalHit = HitPos - HitWall->GetActorLocation();
					float ProjDist = FVector::DotProduct(LocalHit, WallDir);
					float StartDist = ProjDist + HalfLen;
					StartDist = FMath::Clamp(StartDist, (float)Properties->OpeningWidth * 0.5f, WallLen - (float)Properties->OpeningWidth * 0.5f);
					PreviewOpeningPos = StartDist; PreviewOpeningWall = HitWall; bHasPreviewOpening = true;
				}
			}
		}
		return true;
	};
	HoverBehavior->OnEndHoverFunc = [this]() { bHasHighlightedAnchor = false; bHasPreviewOpening = false; PreviewOpeningWall = nullptr; };
	AddInputBehavior(HoverBehavior);

	Properties = NewObject<UWallBuilderSelectToolProperties>(this, "Select Settings");
	AddToolPropertySource(Properties);

	if (ParentMode)
	{
		Properties->bShowAnchors = ParentMode->SavedSelectShowAnchors;
		Properties->bEnableLengthSnap = ParentMode->SavedSelectEnableLengthSnap;
		Properties->SnapLength = ParentMode->SavedSelectSnapLength;
		Properties->bDoorMode = ParentMode->SavedSelectDoorMode;
		Properties->OpeningWidth = ParentMode->SavedSelectOpeningWidth;
		Properties->OpeningHeight = ParentMode->SavedSelectOpeningHeight;
		Properties->OpeningZOffset = ParentMode->SavedSelectOpeningZOffset;
	}

	SelectedWall = nullptr; SelectedStair = nullptr; SelectedRamp = nullptr;
	SelectedActor = nullptr;
	SelectedType = EBuildActorType::None;
	bIsDraggingAnchor = false;
	RefreshAnchors();
}

void UWallBuilderSelectTool::Shutdown(EToolShutdownType ShutdownType)
{
	DestroyTransformGizmo();
}

void UWallBuilderSelectTool::UpdateTransformGizmo()
{
	DestroyTransformGizmo();

	if (!SelectedActor) return;

	// 创建 Transform Proxy
	TransformProxy = NewObject<UTransformProxy>(this);
	TransformProxy->SetTransform(SelectedActor->GetActorTransform());
	
	// 绑定变换回调
	TransformProxy->OnTransformChanged.AddUObject(this, &UWallBuilderSelectTool::OnGizmoTransformChanged);

	// 创建 Combined Transform Gizmo
	TransformGizmo = UE::TransformGizmoUtil::CreateCustomTransformGizmo(
		GetToolManager(),
		ETransformGizmoSubElements::TranslateRotateUniformScale,
		this,
		TEXT("WallBuilderSelectToolGizmo")
	);

	if (TransformGizmo)
	{
		TransformGizmo->SetActiveTarget(TransformProxy, GetToolManager());
	}
}

void UWallBuilderSelectTool::OnGizmoTransformChanged(UTransformProxy* Proxy, FTransform Transform)
{
	if (SelectedActor)
	{
		SelectedActor->SetActorTransform(Transform);
	}
}

void UWallBuilderSelectTool::DestroyTransformGizmo()
{
	if (TransformGizmo)
	{
		GetToolManager()->GetPairedGizmoManager()->DestroyGizmo(TransformGizmo);
		TransformGizmo = nullptr;
	}
	if (TransformProxy)
	{
		TransformProxy->OnTransformChanged.RemoveAll(this);
		TransformProxy = nullptr;
	}
}

void UWallBuilderSelectTool::RefreshAnchors()
{
	PersistentAnchors.Empty();
	if (TargetWorld && TargetWorld->PersistentLevel)
	{
		for (AActor* Actor : TargetWorld->PersistentLevel->Actors)
		{
			if (!Actor || !IsValid(Actor)) continue;
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
			else if (AWallBuilderFloorActor* F = Cast<AWallBuilderFloorActor>(Actor))
			{
				const TArray<FVector>& FloorVerts = F->GetVertices();
				for (const FVector& V : FloorVerts)
					PersistentAnchors.AddUnique(V);
			}
		}
	}
}

void UWallBuilderSelectTool::SyncParamsFromSelectedActor()
{
	switch (SelectedType)
	{
	case EBuildActorType::Wall:
		if (SelectedWall)
		{
			const FWallParameters& P = SelectedWall->GetWallParameters();
			Properties->WallHeight = FMath::RoundToFloat(P.Height);
			Properties->WallThickness = FMath::RoundToFloat(P.Thickness);
			Properties->WallMaterial = P.WallMaterial;
			Properties->WallLength = FVector::Dist2D(P.StartPoint, P.EndPoint);
			Properties->SelectedBuildName = SelectedWall->GetActorLabel();
		}
		break;
	case EBuildActorType::Stair:
		if (SelectedStair)
		{
			const FStairParameters& P = SelectedStair->GetStairParameters();
			Properties->StairWidth = FMath::RoundToFloat(P.Width);
			Properties->StairHeight = FMath::RoundToFloat(P.TotalHeight);
			Properties->StepCount = P.StepCount;
			Properties->StairMaterial = P.StairMaterial;
			Properties->SelectedBuildName = SelectedStair->GetActorLabel();
		}
		break;
	case EBuildActorType::Ramp:
		if (SelectedRamp)
		{
			const FRampParameters& P = SelectedRamp->GetRampParameters();
			Properties->RampWidth = FMath::RoundToFloat(P.Width);
			Properties->RampStartHeight = FMath::RoundToFloat(P.StartHeight);
			Properties->RampEndHeight = FMath::RoundToFloat(P.EndHeight);
			Properties->RampMaterial = P.RampMaterial;
			Properties->SelectedBuildName = SelectedRamp->GetActorLabel();
		}
		break;
	default: break;
	}
}

FInputRayHit UWallBuilderSelectTool::CanBeginClickDragSequence(const FInputDeviceRay& PressPos)
{
	FVector Temp;
	return FindRayHit(PressPos.WorldRay, Temp);
}

void UWallBuilderSelectTool::OnClickPress(const FInputDeviceRay& PressPos)
{
	FVector HitPos;
	FInputRayHit HitResult = FindRayHit(PressPos.WorldRay, HitPos);
	if (!HitResult.bHit) return;

	// 锚点拖拽
	if (Properties->bShowAnchors)
	{
		RefreshAnchors();
		for (const FVector& AP : PersistentAnchors)
		{
			if (FVector::Dist(HitPos, AP) < Properties->CloseThreshold)
			{
				bIsDraggingAnchor = true;
				DraggedAnchorOriginalPos = AP;
				DraggedAnchorCurrentPos = AP;
				HighlightedAnchor = AP;
				bHasHighlightedAnchor = true;
				Properties->StatusText = TEXT("Drag anchor to move");

				// 保存所有受影响Actor的原始参数
				SavedWallParams.Empty();
				SavedStairParams.Empty();
				SavedRampParams.Empty();
				SavedFloorParams.Empty();
				SavedAffectedActors.Empty();

				for (AActor* Actor : TargetWorld->PersistentLevel->Actors)
				{
					if (!Actor || !IsValid(Actor)) continue;

					if (AWallBuilderWallActor* Wall = Cast<AWallBuilderWallActor>(Actor))
					{
						const FWallParameters& Params = Wall->GetWallParameters();
						if (Params.StartPoint.Equals(AP) || Params.EndPoint.Equals(AP))
						{
							SavedAffectedActors.Add(Wall);
							SavedWallParams.Add(Params);
						}
					}
					else if (AWallBuilderStairActor* Stair = Cast<AWallBuilderStairActor>(Actor))
					{
						const FStairParameters& Params = Stair->GetStairParameters();
						if (Params.StartPoint.Equals(AP) || Params.EndPoint.Equals(AP))
						{
							SavedAffectedActors.Add(Stair);
							SavedStairParams.Add(Params);
						}
					}
					else if (AWallBuilderRampActor* Ramp = Cast<AWallBuilderRampActor>(Actor))
					{
						const FRampParameters& Params = Ramp->GetRampParameters();
						if (Params.StartPoint.Equals(AP) || Params.EndPoint.Equals(AP))
						{
							SavedAffectedActors.Add(Ramp);
							SavedRampParams.Add(Params);
						}
					}
					else if (AWallBuilderFloorActor* Floor = Cast<AWallBuilderFloorActor>(Actor))
					{
						const TArray<FVector>& FloorVerts = Floor->GetVertices();
						for (const FVector& V : FloorVerts)
						{
							if (V.Equals(AP))
							{
								SavedAffectedActors.Add(Floor);
								SavedFloorParams.Add(Floor->GetFloorParameters());
								SavedFloorVertices.Add(FloorVerts);
								break;
							}
						}
					}
				}

				return;
			}
		}
	}

	// 门洞模式
	if (Properties->bDoorMode)
	{
		AWallBuilderWallActor* HitWall = nullptr;
		FInputRayHit WallHit = FindWallHit(PressPos.WorldRay, HitWall);
		if (WallHit.bHit && HitWall)
		{
			FVector WallDir = HitWall->GetWallDirection();
			float WallLen = HitWall->GetWallLength();
			float HalfLen = WallLen * 0.5f;
			FVector LocalHit = HitPos - HitWall->GetActorLocation();
			float ProjDist = FVector::DotProduct(LocalHit, WallDir);
			float HitPosAlongWall = ProjDist + HalfLen;

			const TArray<FWallOpening>& Openings = HitWall->GetOpenings();
			for (int32 i = 0; i < Openings.Num(); i++)
			{
				const FWallOpening& Op = Openings[i];
				if (FMath::Abs(HitPosAlongWall - Op.Position) < Op.Width * 0.5f + 10.0f)
				{
					Properties->SelectedOpeningIndex = i;
					Properties->OpeningWidth = FMath::RoundToFloat(Op.Width);
					Properties->OpeningHeight = FMath::RoundToFloat(Op.Height);
					Properties->OpeningZOffset = FMath::RoundToFloat(Op.ZOffset);
					Properties->StatusText = FString::Printf(TEXT("Selected opening #%d on %s"), i, *HitWall->GetActorLabel());
					SelectWall(HitWall);
					return;
				}
			}

			if (!SelectWall(HitWall))
			{
				DeselectAll();
				SelectWall(HitWall);
			}

			float StartDist = FMath::Clamp(HitPosAlongWall, (float)Properties->OpeningWidth * 0.5f, WallLen - (float)Properties->OpeningWidth * 0.5f);
			FWallOpening NewOpening;
			NewOpening.Position = StartDist;
			NewOpening.Width = (float)Properties->OpeningWidth;
			NewOpening.Height = (float)Properties->OpeningHeight;
			NewOpening.ZOffset = (float)Properties->OpeningZOffset;
			HitWall->AddOpening(NewOpening);
			Properties->SelectedOpeningIndex = HitWall->GetOpenings().Num() - 1;
			Properties->StatusText = FString::Printf(TEXT("Opening created at %.0f cm"), StartDist);
			return;
		}
	}

	// 尝试选中墙
	AWallBuilderWallActor* HitWall2 = nullptr;
	FInputRayHit WallHit2 = FindWallHit(PressPos.WorldRay, HitWall2);
	if (WallHit2.bHit && HitWall2)
	{
		SelectWall(HitWall2);
		return;
	}

	// 尝试选中楼梯/斜坡
	FCollisionObjectQueryParams QueryParams2(FCollisionObjectQueryParams::AllObjects);
	FHitResult StairRampHit;
	if (TargetWorld->LineTraceSingleByObjectType(StairRampHit, PressPos.WorldRay.Origin, PressPos.WorldRay.PointAt(999999), QueryParams2))
	{
		AActor* HitActor = StairRampHit.GetActor();
		if (AWallBuilderStairActor* HitStair = Cast<AWallBuilderStairActor>(HitActor))
		{
			SelectStair(HitStair);
			return;
		}
		if (AWallBuilderRampActor* HitRamp = Cast<AWallBuilderRampActor>(HitActor))
		{
			SelectRamp(HitRamp);
			return;
		}
	}

	DeselectAll();
}

void UWallBuilderSelectTool::OnClickDrag(const FInputDeviceRay& DragPos)
{
	if (Properties->bDoorMode && SelectedWall && Properties->SelectedOpeningIndex >= 0 && !bIsDraggingAnchor)
	{
		FVector HitPos;
		FInputRayHit HitResult = FindRayHit(DragPos.WorldRay, HitPos);
		if (!HitResult.bHit) return;

		FVector WallDir = SelectedWall->GetWallDirection();
		float WallLen = SelectedWall->GetWallLength();
		float HalfLen = WallLen * 0.5f;
		FVector LocalHit = HitPos - SelectedWall->GetActorLocation();
		float ProjDist = FVector::DotProduct(LocalHit, WallDir);
		float PosAlongWall = ProjDist + HalfLen;

		const TArray<FWallOpening>& OpeningList = SelectedWall->GetOpenings();
		if (!OpeningList.IsValidIndex(Properties->SelectedOpeningIndex)) return;
		const FWallOpening& Current = OpeningList[Properties->SelectedOpeningIndex];
		float MinPos = Current.Width * 0.5f;
		float MaxPos = WallLen - Current.Width * 0.5f;
		PosAlongWall = FMath::Clamp(PosAlongWall, MinPos, MaxPos);

		if (Properties->bEnableLengthSnap)
		{
			float SnapUnit = (float)Properties->SnapLength;
			PosAlongWall = FMath::RoundToFloat(PosAlongWall / SnapUnit) * SnapUnit;
			PosAlongWall = FMath::Clamp(PosAlongWall, MinPos, MaxPos);
		}

		FWallOpening Updated = Current;
		Updated.Position = PosAlongWall;
		SelectedWall->UpdateOpening(Properties->SelectedOpeningIndex, Updated);
		Properties->StatusText = FString::Printf(TEXT("Moving opening to %.0f cm"), PosAlongWall);
		return;
	}

	if (bIsDraggingAnchor)
	{
		FVector HitPos;
		FInputRayHit HitResult = FindRayHit(DragPos.WorldRay, HitPos);
		if (!HitResult.bHit) return;
		HitPos.Z = 0.0f;

		if (Properties->bEnableLengthSnap)
		{
			float SnapUnit = (float)Properties->SnapLength;
			FVector Delta = HitPos - DraggedAnchorCurrentPos;
			Delta.X = FMath::RoundToFloat(Delta.X / SnapUnit) * SnapUnit;
			Delta.Y = FMath::RoundToFloat(Delta.Y / SnapUnit) * SnapUnit;
			HitPos = DraggedAnchorCurrentPos + Delta;
		}

		for (AActor* Actor : TargetWorld->PersistentLevel->Actors)
		{
			if (!Actor || !IsValid(Actor)) continue;

			if (AWallBuilderWallActor* Wall = Cast<AWallBuilderWallActor>(Actor))
			{
				FWallParameters P = Wall->GetWallParameters();
				bool bChanged = false;
				if (P.StartPoint.Equals(DraggedAnchorCurrentPos)) { P.StartPoint = HitPos; bChanged = true; }
				if (P.EndPoint.Equals(DraggedAnchorCurrentPos)) { P.EndPoint = HitPos; bChanged = true; }
				if (bChanged)
				{
					FVector Center = (P.StartPoint + P.EndPoint) * 0.5f;
					Wall->SetActorLocation(Center);
					Wall->SetWallParameters(P);
					if (Wall == SelectedWall) Properties->WallLength = FVector::Dist2D(P.StartPoint, P.EndPoint);
					// 实时更新墙角斜切
					AWallBuilderWallActor::UpdateWallAndNeighbors(Wall, TargetWorld);
				}
			}
			else if (AWallBuilderStairActor* Stair = Cast<AWallBuilderStairActor>(Actor))
			{
				FStairParameters P = Stair->GetStairParameters();
				bool bChanged = false;
				if (P.StartPoint.Equals(DraggedAnchorCurrentPos)) { P.StartPoint = HitPos; bChanged = true; }
				if (P.EndPoint.Equals(DraggedAnchorCurrentPos)) { P.EndPoint = HitPos; bChanged = true; }
				if (bChanged)
				{
					FVector Center = (P.StartPoint + P.EndPoint) * 0.5f;
					Stair->SetActorLocation(Center);
					Stair->SetStairParameters(P);
				}
			}
			else if (AWallBuilderRampActor* Ramp = Cast<AWallBuilderRampActor>(Actor))
			{
				FRampParameters P = Ramp->GetRampParameters();
				bool bChanged = false;
				if (P.StartPoint.Equals(DraggedAnchorCurrentPos)) { P.StartPoint = HitPos; bChanged = true; }
				if (P.EndPoint.Equals(DraggedAnchorCurrentPos)) { P.EndPoint = HitPos; bChanged = true; }
				if (bChanged)
				{
					FVector Center = (P.StartPoint + P.EndPoint) * 0.5f;
					Ramp->SetActorLocation(Center);
					Ramp->SetRampParameters(P);
				}
			}
			else if (AWallBuilderFloorActor* Floor = Cast<AWallBuilderFloorActor>(Actor))
			{
				TArray<FVector> FloorVerts = Floor->GetVertices();
				bool bChanged = false;
				for (int32 i = 0; i < FloorVerts.Num(); i++)
				{
					if (FloorVerts[i].Equals(DraggedAnchorCurrentPos))
					{
						FloorVerts[i] = HitPos;
						bChanged = true;
						break;
					}
				}
				if (bChanged)
				{
					Floor->SetVertices(FloorVerts);
					Floor->RebuildMesh();
				}
			}
		}

		DraggedAnchorCurrentPos = HitPos;
		RefreshAnchors();
		HighlightedAnchor = HitPos;
		bHasHighlightedAnchor = true;
		Properties->StatusText = TEXT("Dragging anchor...");
	}
}

void UWallBuilderSelectTool::OnClickRelease(const FInputDeviceRay& ReleasePos)
{
	if (bIsDraggingAnchor)
	{
		bIsDraggingAnchor = false;
		bHasHighlightedAnchor = false;

		// 如果锚点位置确实改变了，创建撤回命令
		if (!DraggedAnchorOriginalPos.Equals(DraggedAnchorCurrentPos))
		{
			UMoveAnchorCommand* Command = NewObject<UMoveAnchorCommand>(this);
			// 直接设置已保存的参数，不重新收集
			Command->OriginalPosition = DraggedAnchorOriginalPos;
			Command->NewPosition = DraggedAnchorCurrentPos;
			Command->AffectedActors = SavedAffectedActors;
			Command->OriginalWallParams = SavedWallParams;
			Command->OriginalStairParams = SavedStairParams;
			Command->OriginalRampParams = SavedRampParams;
			Command->CommandDescription = TEXT("Move Anchor");

			// 执行命令（记录到历史，不需要再执行因为Actor已经移动了）
			UWallBuilderCommandHistory::Get()->ExecuteCommand(Command);
			Properties->StatusText = TEXT("Anchor moved. Click to select");

			// 更新墙角斜切（所有受影响的墙体）
			for (const TWeakObjectPtr<AActor>& ActorPtr : SavedAffectedActors)
			{
				if (AActor* Actor = ActorPtr.Get())
				{
					if (AWallBuilderWallActor* WallActor = Cast<AWallBuilderWallActor>(Actor))
					{
						AWallBuilderWallActor::UpdateWallAndNeighbors(WallActor, TargetWorld);
					}
				}
			}
		}
	}
}

FInputRayHit UWallBuilderSelectTool::FindRayHit(const FRay& WorldRay, FVector& HitPos)
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
		if (T > 0.0f) { HitPos = WorldRay.Origin + WorldRay.Direction * T; return FInputRayHit(T); }
	}
	return FInputRayHit();
}

FInputRayHit UWallBuilderSelectTool::FindWallHit(const FRay& WorldRay, AWallBuilderWallActor*& OutWall)
{
	FCollisionObjectQueryParams QueryParams(FCollisionObjectQueryParams::AllObjects);
	FHitResult Result;
	if (TargetWorld->LineTraceSingleByObjectType(Result, WorldRay.Origin, WorldRay.PointAt(999999), QueryParams))
	{
		if (AWallBuilderWallActor* WallActor = Cast<AWallBuilderWallActor>(Result.GetActor()))
		{
			OutWall = WallActor;
			return FInputRayHit(Result.Distance);
		}
	}
	OutWall = nullptr;
	return FInputRayHit();
}

bool UWallBuilderSelectTool::SelectWall(AWallBuilderWallActor* Wall)
{
	if (SelectedWall == Wall) return false;
	DeselectAll();
	SelectedWall = Wall;
	SelectedActor = Wall;
	SelectedType = EBuildActorType::Wall;
	SyncParamsFromSelectedActor();
	UpdateTransformGizmo();
	if (!Properties->bDoorMode)
		Properties->StatusText = FString::Printf(TEXT("Selected Wall: %s"), *Wall->GetActorLabel());
	return true;
}

void UWallBuilderSelectTool::SelectStair(AWallBuilderStairActor* Stair)
{
	DeselectAll();
	SelectedStair = Stair;
	SelectedActor = Stair;
	SelectedType = EBuildActorType::Stair;
	SyncParamsFromSelectedActor();
	UpdateTransformGizmo();
	Properties->StatusText = FString::Printf(TEXT("Selected Stair: %s"), *Stair->GetActorLabel());
	RefreshAnchors();
}

void UWallBuilderSelectTool::SelectRamp(AWallBuilderRampActor* Ramp)
{
	DeselectAll();
	SelectedRamp = Ramp;
	SelectedActor = Ramp;
	SelectedType = EBuildActorType::Ramp;
	SyncParamsFromSelectedActor();
	UpdateTransformGizmo();
	Properties->StatusText = FString::Printf(TEXT("Selected Ramp: %s"), *Ramp->GetActorLabel());
	RefreshAnchors();
}

void UWallBuilderSelectTool::DeselectAll()
{
	SelectedWall = nullptr;
	SelectedStair = nullptr;
	SelectedRamp = nullptr;
	SelectedActor = nullptr;
	SelectedType = EBuildActorType::None;
	DestroyTransformGizmo();
	Properties->WallHeight = 300; Properties->WallThickness = 20; Properties->WallMaterial = nullptr;
	Properties->WallLength = 0.0f;
	Properties->StairWidth = 120; Properties->StairHeight = 150; Properties->StepCount = 10; Properties->StairMaterial = nullptr;
	Properties->RampWidth = 100; Properties->RampStartHeight = 0; Properties->RampEndHeight = 100; Properties->RampMaterial = nullptr;
	Properties->SelectedBuildName = TEXT("None");
	Properties->SelectedOpeningIndex = -1;
	if (!Properties->bDoorMode) Properties->StatusText = TEXT("Click a build element to select");
}

static void DrawHighlightBox(FPrimitiveDrawInterface* PDI, const FVector& SP, const FVector& EP,
	float Width, float HeightMin, float HeightMax, FColor Color)
{
	FVector Dir2D = (EP - SP).GetSafeNormal2D();
	FVector Right = FVector::CrossProduct(FVector::UpVector, Dir2D).GetSafeNormal() * (Width * 0.5f + 2.0f);

	FVector V0 = SP - Right; V0.Z += HeightMin - 1.0f;
	FVector V1 = SP + Right; V1.Z += HeightMin - 1.0f;
	FVector V2 = EP + Right; V2.Z += HeightMin - 1.0f;
	FVector V3 = EP - Right; V3.Z += HeightMin - 1.0f;
	FVector V4 = SP - Right; V4.Z += HeightMax + 1.0f;
	FVector V5 = SP + Right; V5.Z += HeightMax + 1.0f;
	FVector V6 = EP + Right; V6.Z += HeightMax + 1.0f;
	FVector V7 = EP - Right; V7.Z += HeightMax + 1.0f;

	float Thickness = 3.0f;
	PDI->DrawLine(V0, V1, Color, SDPG_Foreground, Thickness, 0.0f, true);
	PDI->DrawLine(V1, V2, Color, SDPG_Foreground, Thickness, 0.0f, true);
	PDI->DrawLine(V2, V3, Color, SDPG_Foreground, Thickness, 0.0f, true);
	PDI->DrawLine(V3, V0, Color, SDPG_Foreground, Thickness, 0.0f, true);
	PDI->DrawLine(V4, V5, Color, SDPG_Foreground, Thickness, 0.0f, true);
	PDI->DrawLine(V5, V6, Color, SDPG_Foreground, Thickness, 0.0f, true);
	PDI->DrawLine(V6, V7, Color, SDPG_Foreground, Thickness, 0.0f, true);
	PDI->DrawLine(V7, V4, Color, SDPG_Foreground, Thickness, 0.0f, true);
	PDI->DrawLine(V0, V4, Color, SDPG_Foreground, Thickness, 0.0f, true);
	PDI->DrawLine(V1, V5, Color, SDPG_Foreground, Thickness, 0.0f, true);
	PDI->DrawLine(V2, V6, Color, SDPG_Foreground, Thickness, 0.0f, true);
	PDI->DrawLine(V3, V7, Color, SDPG_Foreground, Thickness, 0.0f, true);
}

void UWallBuilderSelectTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	FPrimitiveDrawInterface* PDI = RenderAPI->GetPrimitiveDrawInterface();

	// 锚点
	if (Properties->bShowAnchors && PersistentAnchors.Num() > 0)
	{
		for (const FVector& AP : PersistentAnchors)
		{
			if (bHasHighlightedAnchor && AP.Equals(HighlightedAnchor))
			{
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

	// 门洞预览
	if (bHasPreviewOpening && PreviewOpeningWall && Properties->bDoorMode)
	{
		const TArray<FWallOpening>& OpeningList = PreviewOpeningWall->GetOpenings();
		bool bOverlaps = false;
		for (const FWallOpening& Existing : OpeningList)
		{
			if (FMath::Abs(PreviewOpeningPos - Existing.Position) < ((float)Properties->OpeningWidth + Existing.Width) * 0.4f)
			{
				bOverlaps = true; break;
			}
		}
		if (!bOverlaps)
		{
			FWallOpening PreviewOp;
			PreviewOp.Position = PreviewOpeningPos;
			PreviewOp.Width = (float)Properties->OpeningWidth;
			PreviewOp.Height = (float)Properties->OpeningHeight;
			PreviewOp.ZOffset = (float)Properties->OpeningZOffset;
			FVector WallDir = PreviewOpeningWall->GetWallDirection();
			float WallLen = PreviewOpeningWall->GetWallLength();
			float HalfT = PreviewOpeningWall->GetWallParameters().Thickness * 0.5f;
			// reuse legacy DrawOpeningRect via lambda
			auto DrawOpening = [&](const FWallOpening& Op, FColor C, float Tk)
			{
				float HalfLen = WallLen * 0.5f;
				float Cx = Op.Position - HalfLen;
				FVector R = FVector::CrossProduct(FVector::UpVector, WallDir).GetSafeNormal();
				float HL = Op.Width * 0.5f;
				float H2 = Op.Height * 0.5f;
				float WallH2 = PreviewOpeningWall->GetWallParameters().Height * 0.5f;
				float CenterZ = Op.ZOffset + H2 - WallH2;
				FVector O = PreviewOpeningWall->GetActorLocation();
				FVector BL = O + WallDir * (Cx - HL) + R * HalfT + FVector(0, 0, CenterZ - H2);
				FVector BR = O + WallDir * (Cx + HL) + R * HalfT + FVector(0, 0, CenterZ - H2);
				FVector TL = O + WallDir * (Cx - HL) + R * HalfT + FVector(0, 0, CenterZ + H2);
				FVector TR = O + WallDir * (Cx + HL) + R * HalfT + FVector(0, 0, CenterZ + H2);
				FVector bBL = O + WallDir * (Cx - HL) - R * HalfT + FVector(0, 0, CenterZ - H2);
				FVector bBR = O + WallDir * (Cx + HL) - R * HalfT + FVector(0, 0, CenterZ - H2);
				FVector bTL = O + WallDir * (Cx - HL) - R * HalfT + FVector(0, 0, CenterZ + H2);
				FVector bTR = O + WallDir * (Cx + HL) - R * HalfT + FVector(0, 0, CenterZ + H2);
				PDI->DrawLine(BL, BR, C, SDPG_Foreground, Tk, 0, true);
				PDI->DrawLine(BR, TR, C, SDPG_Foreground, Tk, 0, true);
				PDI->DrawLine(TR, TL, C, SDPG_Foreground, Tk, 0, true);
				PDI->DrawLine(TL, BL, C, SDPG_Foreground, Tk, 0, true);
				PDI->DrawLine(bBL, bBR, C, SDPG_Foreground, Tk, 0, true);
				PDI->DrawLine(bBR, bTR, C, SDPG_Foreground, Tk, 0, true);
				PDI->DrawLine(bTR, bTL, C, SDPG_Foreground, Tk, 0, true);
				PDI->DrawLine(bTL, bBL, C, SDPG_Foreground, Tk, 0, true);
				PDI->DrawLine(BL, bBL, C, SDPG_Foreground, Tk, 0, true);
				PDI->DrawLine(BR, bBR, C, SDPG_Foreground, Tk, 0, true);
				PDI->DrawLine(TL, bTL, C, SDPG_Foreground, Tk, 0, true);
				PDI->DrawLine(TR, bTR, C, SDPG_Foreground, Tk, 0, true);
			};
			DrawOpening(PreviewOp, FColor(0, 255, 0, 100), 3.0f);
		}
	}

	// 门洞模式 — 显示所有开口
	if (Properties->bDoorMode && TargetWorld)
	{
		for (AActor* Actor : TargetWorld->PersistentLevel->Actors)
		{
			if (!Actor || !IsValid(Actor)) continue;
			AWallBuilderWallActor* Wall = Cast<AWallBuilderWallActor>(Actor);
			if (!Wall) continue;
			const TArray<FWallOpening>& OpeningList = Wall->GetOpenings();
			if (OpeningList.Num() == 0) continue;
			FVector WallDir = Wall->GetWallDirection();
			float WallLen = Wall->GetWallLength();
			float HalfT = Wall->GetWallParameters().Thickness * 0.5f;
			for (int32 i = 0; i < OpeningList.Num(); i++)
			{
				bool bSelected = (Wall == SelectedWall && Properties->SelectedOpeningIndex == i);
				FColor Col = bSelected ? FColor(0, 255, 255) : FColor(100, 100, 255);
				// inline draw
				float HalfLen = WallLen * 0.5f;
				float Cx = OpeningList[i].Position - HalfLen;
				FVector R = FVector::CrossProduct(FVector::UpVector, WallDir).GetSafeNormal();
				float HL = OpeningList[i].Width * 0.5f;
				float H2 = OpeningList[i].Height * 0.5f;
				float WallH2 = Wall->GetWallParameters().Height * 0.5f;
				float CenterZ = OpeningList[i].ZOffset + H2 - WallH2;
				FVector O = Wall->GetActorLocation();
				float Tk = bSelected ? 3.0f : 1.5f;
				FVector BL = O + WallDir * (Cx - HL) + R * HalfT + FVector(0, 0, CenterZ - H2);
				FVector BR = O + WallDir * (Cx + HL) + R * HalfT + FVector(0, 0, CenterZ - H2);
				FVector TL = O + WallDir * (Cx - HL) + R * HalfT + FVector(0, 0, CenterZ + H2);
				FVector TR = O + WallDir * (Cx + HL) + R * HalfT + FVector(0, 0, CenterZ + H2);
				FVector bBL = O + WallDir * (Cx - HL) - R * HalfT + FVector(0, 0, CenterZ - H2);
				FVector bBR = O + WallDir * (Cx + HL) - R * HalfT + FVector(0, 0, CenterZ - H2);
				FVector bTL = O + WallDir * (Cx - HL) - R * HalfT + FVector(0, 0, CenterZ + H2);
				FVector bTR = O + WallDir * (Cx + HL) - R * HalfT + FVector(0, 0, CenterZ + H2);
				PDI->DrawLine(BL, BR, Col, SDPG_Foreground, Tk, 0, true);
				PDI->DrawLine(BR, TR, Col, SDPG_Foreground, Tk, 0, true);
				PDI->DrawLine(TR, TL, Col, SDPG_Foreground, Tk, 0, true);
				PDI->DrawLine(TL, BL, Col, SDPG_Foreground, Tk, 0, true);
				PDI->DrawLine(bBL, bBR, Col, SDPG_Foreground, Tk, 0, true);
				PDI->DrawLine(bBR, bTR, Col, SDPG_Foreground, Tk, 0, true);
				PDI->DrawLine(bTR, bTL, Col, SDPG_Foreground, Tk, 0, true);
				PDI->DrawLine(bTL, bBL, Col, SDPG_Foreground, Tk, 0, true);
				PDI->DrawLine(BL, bBL, Col, SDPG_Foreground, Tk, 0, true);
				PDI->DrawLine(BR, bBR, Col, SDPG_Foreground, Tk, 0, true);
				PDI->DrawLine(TL, bTL, Col, SDPG_Foreground, Tk, 0, true);
				PDI->DrawLine(TR, bTR, Col, SDPG_Foreground, Tk, 0, true);
			}
		}
	}

	// 选中高亮
	if (SelectedType == EBuildActorType::Wall && SelectedWall)
	{
		const FWallParameters& P = SelectedWall->GetWallParameters();
		DrawHighlightBox(PDI, P.StartPoint, P.EndPoint, P.Thickness, 0, P.Height, FColor(0, 255, 255));
	}
	else if (SelectedType == EBuildActorType::Stair && SelectedStair)
	{
		const FStairParameters& P = SelectedStair->GetStairParameters();
		DrawHighlightBox(PDI, P.StartPoint, P.EndPoint, P.Width, 0, P.TotalHeight, FColor(0, 255, 255));
	}
	else if (SelectedType == EBuildActorType::Ramp && SelectedRamp)
	{
		const FRampParameters& P = SelectedRamp->GetRampParameters();
		float ZMin = FMath::Min(P.StartHeight, P.EndHeight);
		float ZMax = FMath::Max(P.StartHeight, P.EndHeight);
		DrawHighlightBox(PDI, P.StartPoint, P.EndPoint, P.Width, ZMin, ZMax, FColor(0, 255, 255));
	}
}

void UWallBuilderSelectTool::DrawAnchorCross(FPrimitiveDrawInterface* PDI, const FVector& Pos, FColor Col) const
{
	const float S = 15.0f;
	PDI->DrawLine(Pos + FVector( S,  S, 0), Pos + FVector(-S, -S, 0), Col, SDPG_Foreground, 2.0f, 0.0f, true);
	PDI->DrawLine(Pos + FVector( S, -S, 0), Pos + FVector(-S,  S, 0), Col, SDPG_Foreground, 2.0f, 0.0f, true);
	PDI->DrawLine(Pos + FVector( S, 0, 0), Pos + FVector(-S, 0, 0), Col, SDPG_Foreground, 2.0f, 0.0f, true);
	PDI->DrawLine(Pos + FVector(0,  S, 0), Pos + FVector(0, -S, 0), Col, SDPG_Foreground, 2.0f, 0.0f, true);
	PDI->DrawPoint(Pos, Col, 6.0f, SDPG_Foreground);
}

void UWallBuilderSelectTool::OnPropertyModified(UObject* PropertySet, FProperty* Property)
{
	if (PropertySet != Properties) return;

	// 处理撤回操作
	if (Properties->bUndo)
	{
		Properties->bUndo = false;
		UWallBuilderCommandHistory::Get()->Undo();
		if (Properties->bShowAnchors)
			RefreshAnchors();
		return;
	}

	// 处理重做操作
	if (Properties->bRedo)
	{
		Properties->bRedo = false;
		UWallBuilderCommandHistory::Get()->Redo();
		if (Properties->bShowAnchors)
			RefreshAnchors();
		return;
	}

	// 删除
	if (Properties->bDeleteWall)
	{
		Properties->bDeleteWall = false;
		UDeleteActorCommand* Command = NewObject<UDeleteActorCommand>(this);
		Command->TargetWorld = TargetWorld;

		if (SelectedType == EBuildActorType::Wall && SelectedWall)
		{
			Command->ActorType = EBuildActorType::Wall;
			Command->DeletedWallParams = SelectedWall->GetWallParameters();
			FString Name = SelectedWall->GetActorLabel();

			// 删除前保存相邻墙，删除后恢复斜切
			FVector DeletedStart = SelectedWall->GetWallParameters().StartPoint;
			FVector DeletedEnd = SelectedWall->GetWallParameters().EndPoint;
			float Threshold = SelectedWall->GetWallParameters().Thickness * 1.5f;

			SelectedWall->Destroy();
			DeselectAll();

			// 更新相邻墙的斜切（恢复为无斜切）
			TArray<AActor*> FoundActors;
			UGameplayStatics::GetAllActorsOfClass(TargetWorld, AWallBuilderWallActor::StaticClass(), FoundActors);
			for (AActor* FoundActor : FoundActors)
			{
				AWallBuilderWallActor* Wall = Cast<AWallBuilderWallActor>(FoundActor);
				if (!Wall) continue;
				const FWallParameters& Params = Wall->GetWallParameters();
				FVector WStart = Params.StartPoint; WStart.Z = 0;
				FVector WEnd = Params.EndPoint; WEnd.Z = 0;
				FVector DS = DeletedStart; DS.Z = 0;
				FVector DE = DeletedEnd; DE.Z = 0;
				float Dist = FMath::Min(
					FMath::Min(FVector::Dist(WStart, DS), FVector::Dist(WStart, DE)),
					FMath::Min(FVector::Dist(WEnd, DS), FVector::Dist(WEnd, DE))
				);
				if (Dist < Threshold)
				{
					// 直接恢复为无斜切
					FWallParameters NewParams = Params;
					NewParams.StartMiterAngle = 0.0f;
					NewParams.EndMiterAngle = 0.0f;
					Wall->SetWallParameters(NewParams);
				}
			}

			Properties->StatusText = FString::Printf(TEXT("Deleted: %s"), *Name);
		}
		else if (SelectedType == EBuildActorType::Stair && SelectedStair)
		{
			Command->ActorType = EBuildActorType::Stair;
			Command->DeletedStairParams = SelectedStair->GetStairParameters();
			FString Name = SelectedStair->GetActorLabel();
			SelectedStair->Destroy();
			DeselectAll();
			Properties->StatusText = FString::Printf(TEXT("Deleted: %s"), *Name);
		}
		else if (SelectedType == EBuildActorType::Ramp && SelectedRamp)
		{
			Command->ActorType = EBuildActorType::Ramp;
			Command->DeletedRampParams = SelectedRamp->GetRampParameters();
			FString Name = SelectedRamp->GetActorLabel();
			SelectedRamp->Destroy();
			DeselectAll();
			Properties->StatusText = FString::Printf(TEXT("Deleted: %s"), *Name);
		}

		// 记录删除命令
		if (Command->ActorType != EBuildActorType::None)
		{
			UWallBuilderCommandHistory::Get()->ExecuteCommand(Command);
		}

		RefreshAnchors();
		return;
	}

	// 删除门洞
	if (Properties->bDeleteSelectedOpening && SelectedWall && Properties->SelectedOpeningIndex >= 0)
	{
		Properties->bDeleteSelectedOpening = false;
		int32 DeletedIndex = Properties->SelectedOpeningIndex;
		SelectedWall->RemoveOpening(DeletedIndex);
		Properties->SelectedOpeningIndex = -1;
		Properties->StatusText = FString::Printf(TEXT("Deleted opening #%d"), DeletedIndex);
		return;
	}

	// 锚点
	if (Properties->bShowAnchors) RefreshAnchors();
	else PersistentAnchors.Empty();

	// 门洞切换
	if (Property && Property->GetFName() == GET_MEMBER_NAME_CHECKED(UWallBuilderSelectToolProperties, bDoorMode))
	{
		if (!Properties->bDoorMode) Properties->SelectedOpeningIndex = -1;
		Properties->StatusText = Properties->bDoorMode ? TEXT("Door Mode: click on a wall to create opening") : TEXT("Click a build element to select");
	}

	// 门洞参数修改
	if (Properties->bDoorMode && SelectedWall && Properties->SelectedOpeningIndex >= 0)
	{
		const TArray<FWallOpening>& OpeningList = SelectedWall->GetOpenings();
		if (OpeningList.IsValidIndex(Properties->SelectedOpeningIndex))
		{
			FWallOpening Updated = OpeningList[Properties->SelectedOpeningIndex];
			Updated.Width = (float)Properties->OpeningWidth;
			Updated.Height = (float)Properties->OpeningHeight;
			Updated.ZOffset = (float)Properties->OpeningZOffset;
			SelectedWall->UpdateOpening(Properties->SelectedOpeningIndex, Updated);
		}
	}

	// 保存到 EditorMode
	if (ParentMode)
	{
		ParentMode->SavedSelectShowAnchors = Properties->bShowAnchors;
		ParentMode->SavedSelectEnableLengthSnap = Properties->bEnableLengthSnap;
		ParentMode->SavedSelectSnapLength = Properties->SnapLength;
		ParentMode->SavedSelectDoorMode = Properties->bDoorMode;
		ParentMode->SavedSelectOpeningWidth = Properties->OpeningWidth;
		ParentMode->SavedSelectOpeningHeight = Properties->OpeningHeight;
		ParentMode->SavedSelectOpeningZOffset = Properties->OpeningZOffset;
		ParentMode->SavedWallHeight = Properties->WallHeight;
		ParentMode->SavedWallThickness = Properties->WallThickness;
		ParentMode->SavedWallMaterial = Properties->WallMaterial;
		ParentMode->SavedCloseThreshold = Properties->CloseThreshold;
	}

	// 非门洞模式的参数修改
	if (Properties->bDoorMode) return;

	switch (SelectedType)
	{
	case EBuildActorType::Wall:
		if (SelectedWall)
		{
			FWallParameters NewParams = SelectedWall->GetWallParameters();
			NewParams.Height = (float)Properties->WallHeight;
			NewParams.Thickness = (float)Properties->WallThickness;
			NewParams.WallMaterial = Properties->WallMaterial;
			SelectedWall->SetWallParameters(NewParams);
			Properties->WallLength = FVector::Dist2D(NewParams.StartPoint, NewParams.EndPoint);
		}
		break;
	case EBuildActorType::Stair:
		if (SelectedStair)
		{
			FStairParameters NewParams = SelectedStair->GetStairParameters();
			NewParams.Width = (float)Properties->StairWidth;
			NewParams.TotalHeight = (float)Properties->StairHeight;
			NewParams.StepCount = Properties->StepCount;
			NewParams.StairMaterial = Properties->StairMaterial;
			SelectedStair->SetStairParameters(NewParams);
			SyncParamsFromSelectedActor();
		}
		break;
	case EBuildActorType::Ramp:
		if (SelectedRamp)
		{
			FRampParameters NewParams = SelectedRamp->GetRampParameters();
			NewParams.Width = (float)Properties->RampWidth;
			NewParams.StartHeight = (float)Properties->RampStartHeight;
			NewParams.EndHeight = (float)Properties->RampEndHeight;
			NewParams.RampMaterial = Properties->RampMaterial;
			SelectedRamp->SetRampParameters(NewParams);
			SyncParamsFromSelectedActor();
		}
		break;
	default: break;
	}
}

#undef LOCTEXT_NAMESPACE
