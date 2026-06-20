// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InteractiveToolBuilder.h"
#include "BaseTools/ClickDragTool.h"
#include "WallBuilderStairActor.h"
#include "WallBuilderCommandHistory.h"
#include "WallBuilderStairTool.generated.h"

class UWallBuilderEditorMode;

UCLASS(Transient)
class WALLBUILDER_API UWallBuilderStairToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	UWallBuilderStairToolProperties();

	UPROPERTY(EditAnywhere, Category = "Stair", meta = (UIMin = "10", UIMax = "1000"))
	int32 StairWidth = 120;

	UPROPERTY(EditAnywhere, Category = "Stair", meta = (UIMin = "10", UIMax = "500"))
	int32 StairHeight = 150;

	UPROPERTY(EditAnywhere, Category = "Stair", meta = (UIMin = "1", UIMax = "50"))
	int32 StepCount = 10;

	UPROPERTY(EditAnywhere, Category = "Stair")
	UMaterialInterface* StairMaterial = nullptr;

	UPROPERTY(EditAnywhere, Category = "Snap")
	bool bEnableAngleSnap = false;

	UPROPERTY(EditAnywhere, Category = "Snap", meta = (UIMin = "5", UIMax = "90"))
	int32 SnapAngle = 15;

	UPROPERTY(EditAnywhere, Category = "Snap")
	bool bEnableLengthSnap = false;

	UPROPERTY(EditAnywhere, Category = "Snap", meta = (UIMin = "10", UIMax = "1000"))
	int32 SnapLength = 100;

	UPROPERTY(EditAnywhere, Category = "Actions")
	bool bShowAnchors = false;

	// === 撤回重做 ===
	UPROPERTY(EditAnywhere, Category = "Actions", meta = (DisplayName = "Undo"))
	bool bUndo = false;

	UPROPERTY(EditAnywhere, Category = "Actions", meta = (DisplayName = "Redo"))
	bool bRedo = false;

	UPROPERTY(VisibleAnywhere, Category = "Info")
	float CurrentLength = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Info")
	int32 StairCount = 0;

	UPROPERTY(VisibleAnywhere, Category = "Info")
	FString StatusText = TEXT("Click to set start point");
};

UCLASS()
class WALLBUILDER_API UWallBuilderStairToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	void SetParentMode(UWallBuilderEditorMode* InMode) { ParentMode = InMode; }
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;

	UWallBuilderEditorMode* ParentMode = nullptr;
};

UCLASS()
class WALLBUILDER_API UWallBuilderStairTool : public UInteractiveTool, public IClickDragBehaviorTarget
{
	GENERATED_BODY()

public:
	virtual void SetWorld(UWorld* World);
	void SetParentMode(UWallBuilderEditorMode* InMode);

	virtual void Setup() override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI) override;
	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;

	virtual FInputRayHit CanBeginClickDragSequence(const FInputDeviceRay& PressPos) override;
	virtual void OnClickPress(const FInputDeviceRay& PressPos) override;
	virtual void OnClickDrag(const FInputDeviceRay& DragPos) override;
	virtual void OnClickRelease(const FInputDeviceRay& ReleasePos) override;
	virtual void OnTerminateDragSequence() override;

protected:
	FInputRayHit FindRayHit(const FRay& WorldRay, FVector& HitPos);
	void SpawnStairActor();
	void StopDrawing();
	FVector SnapEndPoint(const FVector& RawEnd) const;
	FVector SnapToAnchor(const FVector& RawPos, bool& bOutSnapped) const;
	void DrawAnchorCross(FPrimitiveDrawInterface* PDI, const FVector& Pos, FColor Col) const;
	void RefreshAnchorsFromWorld();

protected:
	UPROPERTY()
	TObjectPtr<UWallBuilderStairToolProperties> Properties;

	UWorld* TargetWorld = nullptr;
	UWallBuilderEditorMode* ParentMode = nullptr;

	FVector StartPoint = FVector::ZeroVector;
	FVector EndPoint = FVector::ZeroVector;
	bool bIsDrawing = false;
	bool bDragActive = false;

	UPROPERTY()
	TArray<AWallBuilderStairActor*> SpawnedStairs;

	TArray<FVector> PersistentAnchors;

	// 锚点高亮
	bool bHasHighlightedAnchor = false;
	FVector HighlightedAnchor = FVector::ZeroVector;
};
