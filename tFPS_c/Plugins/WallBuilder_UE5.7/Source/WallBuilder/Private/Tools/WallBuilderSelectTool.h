// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InteractiveToolBuilder.h"
#include "BaseTools/ClickDragTool.h"
#include "BaseBehaviors/BehaviorTargetInterfaces.h"
#include "WallBuilderWallActor.h"
#include "WallBuilderStairActor.h"
#include "WallBuilderRampActor.h"
#include "WallBuilderFloorActor.h"
#include "WallBuilderCommandHistory.h"
#include "WallBuilderSelectTool.generated.h"

class UWallBuilderEditorMode;
class UCombinedTransformGizmo;
class UTransformProxy;

/**
 * 选择工具属性集
 */
UCLASS(Transient)
class WALLBUILDER_API UWallBuilderSelectToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	UWallBuilderSelectToolProperties();

	// === 显示锚点 ===
	UPROPERTY(EditAnywhere, Category = "Actions")
	bool bShowAnchors = true;

	// === 墙体参数 ===
	UPROPERTY(EditAnywhere, Category = "Wall", meta = (UIMin = "10", UIMax = "1000"))
	int32 WallHeight = 300;

	UPROPERTY(EditAnywhere, Category = "Wall", meta = (UIMin = "1", UIMax = "500"))
	int32 WallThickness = 20;

	UPROPERTY(EditAnywhere, Category = "Wall")
	UMaterialInterface* WallMaterial = nullptr;

	UPROPERTY(EditAnywhere, Category = "Wall", meta = (UIMin = "10", UIMax = "200"))
	int32 CloseThreshold = 50;

	// === 吸附 ===
	UPROPERTY(EditAnywhere, Category = "Snap")
	bool bEnableLengthSnap = false;

	UPROPERTY(EditAnywhere, Category = "Snap", meta = (UIMin = "10", UIMax = "1000"))
	int32 SnapLength = 100;

	// === 选中楼梯参数 ===
	UPROPERTY(EditAnywhere, Category = "Selected Stair", meta = (UIMin = "10", UIMax = "1000"))
	int32 StairWidth = 120;

	UPROPERTY(EditAnywhere, Category = "Selected Stair", meta = (UIMin = "10", UIMax = "500"))
	int32 StairHeight = 150;

	UPROPERTY(EditAnywhere, Category = "Selected Stair", meta = (UIMin = "1", UIMax = "50"))
	int32 StepCount = 10;

	UPROPERTY(EditAnywhere, Category = "Selected Stair")
	UMaterialInterface* StairMaterial = nullptr;

	// === 选中斜坡参数 ===
	UPROPERTY(EditAnywhere, Category = "Selected Ramp", meta = (UIMin = "10", UIMax = "1000"))
	int32 RampWidth = 100;

	UPROPERTY(EditAnywhere, Category = "Selected Ramp", meta = (UIMin = "-500", UIMax = "500"))
	int32 RampStartHeight = 0;

	UPROPERTY(EditAnywhere, Category = "Selected Ramp", meta = (UIMin = "-500", UIMax = "500"))
	int32 RampEndHeight = 100;

	UPROPERTY(EditAnywhere, Category = "Selected Ramp")
	UMaterialInterface* RampMaterial = nullptr;

	// === 操作 ===
	UPROPERTY(EditAnywhere, Category = "Actions")
	bool bDeleteWall = false;

	// === 门洞模式 ===
	UPROPERTY(EditAnywhere, Category = "Door Mode")
	bool bDoorMode = false;

	UPROPERTY(EditAnywhere, Category = "Door Mode")
	int32 SelectedOpeningIndex = -1;

	UPROPERTY(EditAnywhere, Category = "Door Mode", meta = (UIMin = "10", UIMax = "300"))
	int32 OpeningWidth = 90;

	UPROPERTY(EditAnywhere, Category = "Door Mode", meta = (UIMin = "10", UIMax = "300"))
	int32 OpeningHeight = 210;

	UPROPERTY(EditAnywhere, Category = "Door Mode", meta = (UIMin = "0", UIMax = "200"))
	int32 OpeningZOffset = 0;

	// === 删除门洞 ===
	UPROPERTY(EditAnywhere, Category = "Door Mode")
	bool bDeleteSelectedOpening = false;

	// === 撤回重做 ===
	UPROPERTY(EditAnywhere, Category = "Actions", meta = (DisplayName = "Undo"))
	bool bUndo = false;

	UPROPERTY(EditAnywhere, Category = "Actions", meta = (DisplayName = "Redo"))
	bool bRedo = false;

	// === 信息 ===
	UPROPERTY(VisibleAnywhere, Category = "Info")
	float WallLength = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Info")
	FString SelectedBuildName = TEXT("None");

	UPROPERTY(VisibleAnywhere, Category = "Info")
	FString StatusText = TEXT("Click a build element to select");
};

/**
 * 选择工具构建器
 */
UCLASS()
class WALLBUILDER_API UWallBuilderSelectToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	void SetParentMode(UWallBuilderEditorMode* InMode) { ParentMode = InMode; }
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;

	UWallBuilderEditorMode* ParentMode = nullptr;
};

/**
 * 墙体/楼梯/斜坡选择工具 — 支持选中/拖拽锚点/操作轴移动
 */
UCLASS()
class WALLBUILDER_API UWallBuilderSelectTool : public UInteractiveTool, public IClickDragBehaviorTarget
{
	GENERATED_BODY()

public:
	virtual void SetWorld(UWorld* World);
	void SetParentMode(UWallBuilderEditorMode* InMode);

	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;

	virtual FInputRayHit CanBeginClickDragSequence(const FInputDeviceRay& PressPos) override;
	virtual void OnClickPress(const FInputDeviceRay& PressPos) override;
	virtual void OnClickDrag(const FInputDeviceRay& DragPos) override;
	virtual void OnClickRelease(const FInputDeviceRay& ReleasePos) override;
	virtual void OnTerminateDragSequence() override {}

protected:
	FInputRayHit FindWallHit(const FRay& WorldRay, AWallBuilderWallActor*& OutWall);
	FInputRayHit FindRayHit(const FRay& WorldRay, FVector& HitPos);
	bool SelectWall(AWallBuilderWallActor* Wall);
	void SelectStair(AWallBuilderStairActor* Stair);
	void SelectRamp(AWallBuilderRampActor* Ramp);
	void DeselectAll();
	void RefreshAnchors();
	void DrawAnchorCross(FPrimitiveDrawInterface* PDI, const FVector& Pos, FColor Col) const;

	/** 同步选中Actor的参数回Properties */
	void SyncParamsFromSelectedActor();

	/** 创建/更新 Transform Gizmo */
	void UpdateTransformGizmo();
	void DestroyTransformGizmo();
	
	/** Gizmo 变换回调 */
	UFUNCTION()
	void OnGizmoTransformChanged(UTransformProxy* Proxy, FTransform Transform);

protected:
	UPROPERTY()
	TObjectPtr<UWallBuilderSelectToolProperties> Properties;

	UWorld* TargetWorld = nullptr;
	UWallBuilderEditorMode* ParentMode = nullptr;

	// 选中的构建元素
	EBuildActorType SelectedType = EBuildActorType::None;

	UPROPERTY()
	AWallBuilderWallActor* SelectedWall = nullptr;

	UPROPERTY()
	AWallBuilderStairActor* SelectedStair = nullptr;

	UPROPERTY()
	AWallBuilderRampActor* SelectedRamp = nullptr;

	// 锚点
	TArray<FVector> PersistentAnchors;

	// 锚点高亮
	bool bHasHighlightedAnchor = false;
	FVector HighlightedAnchor = FVector::ZeroVector;

	// 锚点拖拽
	bool bIsDraggingAnchor = false;
	FVector DraggedAnchorOriginalPos = FVector::ZeroVector;
	FVector DraggedAnchorCurrentPos = FVector::ZeroVector;

	// 拖拽开始时保存的原始参数（用于撤回）
	UPROPERTY()
	TArray<FWallParameters> SavedWallParams;

	UPROPERTY()
	TArray<FStairParameters> SavedStairParams;

	UPROPERTY()
	TArray<FRampParameters> SavedRampParams;

	TArray<FFloorParameters> SavedFloorParams;

	TArray<TArray<FVector>> SavedFloorVertices;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> SavedAffectedActors;

	// 门洞预览
	bool bHasPreviewOpening = false;
	float PreviewOpeningPos = 0.0f;
	AWallBuilderWallActor* PreviewOpeningWall = nullptr;

	// Transform Gizmo
	UPROPERTY()
	UCombinedTransformGizmo* TransformGizmo = nullptr;

	UPROPERTY()
	UTransformProxy* TransformProxy = nullptr;

	// 当前选中的 Actor（用于 Gizmo）
	AActor* SelectedActor = nullptr;
};
