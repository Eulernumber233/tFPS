// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InteractiveToolBuilder.h"
#include "BaseTools/ClickDragTool.h"
#include "WallBuilderFloorActor.h"
#include "WallBuilderFloorTool.generated.h"

class UWallBuilderEditorMode;

/**
 * 楼板绘制工具属性集
 */
UCLASS(Transient)
class WALLBUILDER_API UWallBuilderFloorToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	UWallBuilderFloorToolProperties();

	UPROPERTY(EditAnywhere, Category = "Floor", meta = (UIMin = "1", UIMax = "100"))
	int32 FloorThickness = 20;

	UPROPERTY(EditAnywhere, Category = "Floor", meta = (UIMin = "0", UIMax = "1000"))
	int32 FloorHeight = 0;

	UPROPERTY(EditAnywhere, Category = "Floor")
	UMaterialInterface* FloorMaterial = nullptr;

	UPROPERTY(EditAnywhere, Category = "Snap")
	bool bEnableGridSnap = true;

	UPROPERTY(EditAnywhere, Category = "Snap", meta = (UIMin = "1", UIMax = "50"))
	int32 GridSize = 10;

	UPROPERTY(EditAnywhere, Category = "Snap")
	bool bSnapToWallTops = true;

	UPROPERTY(EditAnywhere, Category = "Snap", meta = (UIMin = "5", UIMax = "100"))
	int32 SnapAnchorThreshold = 30;

	UPROPERTY(EditAnywhere, Category = "Actions")
	bool bFinishDrawing = false;

	UPROPERTY(EditAnywhere, Category = "Actions")
	bool bClosePolygon = false;

	UPROPERTY(VisibleAnywhere, Category = "Info")
	int32 VertexCount = 0;

	UPROPERTY(VisibleAnywhere, Category = "Info")
	FString StatusText = TEXT("Click to place first vertex");
};

/**
 * 楼板绘制工具构建器
 */
UCLASS()
class WALLBUILDER_API UWallBuilderFloorToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	void SetParentMode(UWallBuilderEditorMode* InMode) { ParentMode = InMode; }
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;

	UWallBuilderEditorMode* ParentMode = nullptr;
};

/**
 * 楼板绘制工具
 */
UCLASS()
class WALLBUILDER_API UWallBuilderFloorTool : public UInteractiveTool, public IClickDragBehaviorTarget
{
	GENERATED_BODY()

public:
	virtual void SetWorld(UWorld* World);
	void SetParentMode(UWallBuilderEditorMode* InMode);

	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI) override;
	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;

	// IClickDragBehaviorTarget
	virtual FInputRayHit CanBeginClickDragSequence(const FInputDeviceRay& PressPos) override;
	virtual void OnClickPress(const FInputDeviceRay& PressPos) override;
	virtual void OnClickDrag(const FInputDeviceRay& DragPos) override;
	virtual void OnClickRelease(const FInputDeviceRay& ReleasePos) override;
	virtual void OnTerminateDragSequence() override {}

protected:
	FInputRayHit FindRayHit(const FRay& WorldRay, FVector& HitPos);
	FVector SnapToGrid(const FVector& Pos) const;
	FVector SnapToAnchor(const FVector& RawPos, bool& bOutSnapped) const;
	void RefreshAnchorsFromWorld();
	void UpdatePreview();
	void SpawnFloorActor();
	void FinishDrawing();
	void ClosePolygon();
	bool IsNearFirstVertex(const FVector& TestPoint) const;
	void UpdateStatusText();
	void DrawLine(FPrimitiveDrawInterface* PDI, const FVector& Start, const FVector& End, FColor Color) const;

protected:
	UPROPERTY()
	TObjectPtr<UWallBuilderFloorToolProperties> Properties;

	UWorld* TargetWorld = nullptr;
	UWallBuilderEditorMode* ParentMode = nullptr;

	TArray<FVector> CurrentVertices;
	FVector HoverPoint = FVector::ZeroVector;
	bool bHasHoverPoint = false;
	bool bIsDrawing = false;

	UPROPERTY()
	AWallBuilderFloorActor* PreviewFloorActor = nullptr;

	UPROPERTY()
	TArray<AWallBuilderFloorActor*> SpawnedFloors;

	// 墙顶锚点
	TArray<FVector> WallTopAnchors;
	bool bHasHighlightedAnchor = false;
	FVector HighlightedAnchor = FVector::ZeroVector;
};
