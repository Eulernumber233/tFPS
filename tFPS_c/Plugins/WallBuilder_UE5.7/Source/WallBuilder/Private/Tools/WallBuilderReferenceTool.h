// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InteractiveToolBuilder.h"
#include "BaseTools/SingleClickTool.h"
#include "WallBuilderFloorPlanActor.h"
#include "WallBuilderReferenceTool.generated.h"

class UWallBuilderEditorMode;

/**
 * 参考图工具属性集
 */
UCLASS(Transient)
class WALLBUILDER_API UWallBuilderReferenceToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	UWallBuilderReferenceToolProperties();

	UPROPERTY(EditAnywhere, Category = "Reference Image")
	bool bShowReferenceImage = false;

	UPROPERTY(EditAnywhere, Category = "Reference Image", meta = (EditCondition = "bShowReferenceImage"))
	bool bLoadReferenceImage = false;

	UPROPERTY(EditAnywhere, Category = "Reference Image", meta = (EditCondition = "bShowReferenceImage"))
	bool bClearReferenceImage = false;

	UPROPERTY(EditAnywhere, Category = "Reference Image", meta = (EditCondition = "bShowReferenceImage", UIMin = "10", UIMax = "10000"))
	int32 ReferenceImageScale = 500;

	UPROPERTY(EditAnywhere, Category = "Reference Image", meta = (EditCondition = "bShowReferenceImage", UIMin = "0", UIMax = "100"))
	int32 ReferenceImageOpacity = 100;

	UPROPERTY(EditAnywhere, Category = "Reference Image", meta = (EditCondition = "bShowReferenceImage"))
	int32 ReferenceImageOffsetX = 0;

	UPROPERTY(EditAnywhere, Category = "Reference Image", meta = (EditCondition = "bShowReferenceImage"))
	int32 ReferenceImageOffsetY = 0;

	UPROPERTY(EditAnywhere, Category = "Reference Image", meta = (EditCondition = "bShowReferenceImage"))
	int32 ReferenceImageHeight = 0;
};

/**
 * 参考图工具构建器
 */
UCLASS()
class WALLBUILDER_API UWallBuilderReferenceToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	void SetParentMode(UWallBuilderEditorMode* InMode) { ParentMode = InMode; }
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;

	UWallBuilderEditorMode* ParentMode = nullptr;
};

/**
 * 参考图管理工具 - 提供导入平面图作为参照的UI控制
 */
UCLASS()
class WALLBUILDER_API UWallBuilderReferenceTool : public USingleClickTool
{
	GENERATED_BODY()

public:
	virtual void SetWorld(UWorld* World);
	void SetParentMode(UWallBuilderEditorMode* InMode);

	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;

	void LoadReferenceImage();
	void LoadReferenceImageFromPath(const FString& FilePath);
	void ClearReferenceImage();
	void UpdateReferenceImage();
	void DestroyReferenceImage();
	void ApplyPersistentSettings();
	AWallBuilderFloorPlanActor* FindExistingActor() const;

protected:
	UPROPERTY()
	TObjectPtr<UWallBuilderReferenceToolProperties> Properties;

	UWorld* TargetWorld = nullptr;
	UWallBuilderEditorMode* ParentMode = nullptr;

	UPROPERTY()
	AWallBuilderFloorPlanActor* ReferenceImageActor = nullptr;
};
