// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InteractiveToolBuilder.h"
#include "BaseTools/ClickDragTool.h"
#include "WallBuilderWallActor.h"
#include "WallBuilderCommandHistory.h"
#include "WallBuilderWallTool.generated.h"

class UWallBuilderEditorMode;

/**
 * 墙体绘制工具属性集
 */
UCLASS(Transient)
class WALLBUILDER_API UWallBuilderWallToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	UWallBuilderWallToolProperties();

	// === 墙体参数 ===
	UPROPERTY(EditAnywhere, Category = "Wall", meta = (UIMin = "10", UIMax = "1000"))
	int32 WallHeight = 300;

	UPROPERTY(EditAnywhere, Category = "Wall", meta = (UIMin = "1", UIMax = "500"))
	int32 WallThickness = 20;

	UPROPERTY(EditAnywhere, Category = "Wall")
	UMaterialInterface* WallMaterial = nullptr;

	UPROPERTY(EditAnywhere, Category = "Wall", meta = (UIMin = "10", UIMax = "200"))
	int32 CloseThreshold = 50;

	// === 角度吸附 ===
	UPROPERTY(EditAnywhere, Category = "Snap")
	bool bEnableAngleSnap = false;

	UPROPERTY(EditAnywhere, Category = "Snap", meta = (UIMin = "5", UIMax = "90"))
	int32 SnapAngle = 15;

	// === 长度吸附 ===
	UPROPERTY(EditAnywhere, Category = "Snap")
	bool bEnableLengthSnap = false;

	UPROPERTY(EditAnywhere, Category = "Snap", meta = (UIMin = "10", UIMax = "1000"))
	int32 SnapLength = 100;

	// === 操作 ===
	UPROPERTY(EditAnywhere, Category = "Actions")
	bool bShowAnchors = false;

	// === 撤回重做 ===
	UPROPERTY(EditAnywhere, Category = "Actions", meta = (DisplayName = "Undo"))
	bool bUndo = false;

	UPROPERTY(EditAnywhere, Category = "Actions", meta = (DisplayName = "Redo"))
	bool bRedo = false;

	// === 信息 ===
	UPROPERTY(VisibleAnywhere, Category = "Info")
	float CurrentLength = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Info")
	int32 WallCount = 0;

	UPROPERTY(VisibleAnywhere, Category = "Info")
	FString StatusText = TEXT("Click to set start point");
};

/**
 * 墙体绘制工具构建器
 */
UCLASS()
class WALLBUILDER_API UWallBuilderWallToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	void SetParentMode(UWallBuilderEditorMode* InMode) { ParentMode = InMode; }
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;

	UWallBuilderEditorMode* ParentMode = nullptr;
};

/**
 * 墙体绘制工具
 */
UCLASS()
class WALLBUILDER_API UWallBuilderWallTool : public UInteractiveTool, public IClickDragBehaviorTarget
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
	virtual void OnTerminateDragSequence() override {}

protected:
	FInputRayHit FindRayHit(const FRay& WorldRay, FVector& HitPos);
	void UpdatePreview();
	void SpawnWallActor();
	void StopDrawing();
	bool IsNearStartPoint(const FVector& TestPoint) const;
	FVector SnapEndPoint(const FVector& RawEnd) const;
	FVector SnapEndPointWithLength(const FVector& RawEnd) const;
	FVector SnapToAnchor(const FVector& RawPos, bool& bOutSnapped) const;
	void DrawAnchorCross(FPrimitiveDrawInterface* PDI, const FVector& Pos, FColor Col) const;
	void RefreshAnchorsFromWorld();

protected:
	UPROPERTY()
	TObjectPtr<UWallBuilderWallToolProperties> Properties;

	UWorld* TargetWorld = nullptr;
	UWallBuilderEditorMode* ParentMode = nullptr;

	FVector StartPoint = FVector::ZeroVector;
	FVector EndPoint = FVector::ZeroVector;
	FVector FirstStartPoint = FVector::ZeroVector;

	bool bIsDrawing = false;
	bool bHasValidStart = false;
	bool bHasFirstWall = false;

	// Hover 高亮锚点
	bool bHasHighlightedAnchor = false;
	FVector HighlightedAnchor = FVector::ZeroVector;

	UPROPERTY()
	AWallBuilderWallActor* PreviewWallActor = nullptr;

	UPROPERTY()
	TArray<AWallBuilderWallActor*> SpawnedWalls;

	// 锚点（直接存储，不依赖读回）
	TArray<FVector> PersistentAnchors;
};
