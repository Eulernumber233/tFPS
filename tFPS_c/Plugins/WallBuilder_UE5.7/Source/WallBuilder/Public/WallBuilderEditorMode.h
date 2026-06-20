// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/UEdMode.h"
#include "WallBuilderEditorMode.generated.h"

UCLASS()
class UWallBuilderEditorMode : public UEdMode
{
	GENERATED_BODY()

public:
	const static FEditorModeID EM_WallBuilderEditorModeId;

	// 工具名称常量（按工具栏顺序）
	static FString SelectToolName;      // 选择编辑工具
	static FString WallToolName;        // 绘制墙体
	static FString FloorToolName;       // 绘制楼板
	static FString StairToolName;       // 绘制楼梯
	static FString RampToolName;        // 绘制斜坡
	static FString ReferenceToolName;   // 参考图像

	UWallBuilderEditorMode();
	virtual ~UWallBuilderEditorMode();

	virtual void Enter() override;
	virtual void ActorSelectionChangeNotify() override;
	virtual void CreateToolkit() override;
	virtual TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> GetModeCommands() const override;

	// 持久化的墙体参数
	int32 SavedWallHeight = 300;
	int32 SavedWallThickness = 20;
	UMaterialInterface* SavedWallMaterial = nullptr;
	int32 SavedCloseThreshold = 50;
	bool SavedEnableAngleSnap = false;
	int32 SavedSnapAngle = 15;
	bool SavedEnableLengthSnap = false;
	int32 SavedSnapLength = 100;
	bool SavedShowAnchors = false;

	// 持久化的选择工具参数
	bool SavedSelectEnableLengthSnap = false;
	int32 SavedSelectSnapLength = 100;
	bool SavedSelectShowAnchors = true;
	bool SavedSelectDoorMode = false;
	int32 SavedSelectOpeningWidth = 90;
	int32 SavedSelectOpeningHeight = 210;
	int32 SavedSelectOpeningZOffset = 0;

	// 持久化的楼板工具参数
	int32 SavedFloorThickness = 20;
	int32 SavedFloorHeight = 0;
	UMaterialInterface* SavedFloorMaterial = nullptr;
	bool SavedFloorGridSnap = true;
	int32 SavedFloorGridSize = 10;
	bool SavedFloorSnapToWallTops = true;
	int32 SavedFloorSnapThreshold = 30;

	// 持久化的参考图参数
	FString SavedReferenceImagePath;
	bool SavedShowReferenceImage = false;
	int32 SavedReferenceImageScale = 500;
	int32 SavedReferenceImageOpacity = 100;
	int32 SavedReferenceImageOffsetX = 0;
	int32 SavedReferenceImageOffsetY = 0;
	int32 SavedReferenceImageHeight = 0;

	// 持久化的楼梯工具参数
	int32 SavedStairWidth = 120;
	int32 SavedStairHeight = 150;
	int32 SavedStepCount = 10;
	UMaterialInterface* SavedStairMaterial = nullptr;
	bool SavedStairEnableAngleSnap = false;
	int32 SavedStairSnapAngle = 15;
	bool SavedStairEnableLengthSnap = false;
	int32 SavedStairSnapLength = 100;
	bool SavedStairShowAnchors = false;

	// 持久化的斜坡工具参数
	int32 SavedRampWidth = 100;
	int32 SavedRampStartHeight = 0;
	int32 SavedRampEndHeight = 100;
	UMaterialInterface* SavedRampMaterial = nullptr;
	bool SavedRampEnableAngleSnap = false;
	int32 SavedRampSnapAngle = 15;
	bool SavedRampEnableLengthSnap = false;
	int32 SavedRampSnapLength = 100;
	bool SavedRampShowAnchors = false;
};
