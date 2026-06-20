// Copyright Epic Games, Inc. All Rights Reserved.

#include "WallBuilderReferenceTool.h"
#include "WallBuilderEditorMode.h"
#include "InteractiveToolManager.h"
#include "Engine/World.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Framework/Application/SlateApplication.h"

// ===== Properties =====

UWallBuilderReferenceToolProperties::UWallBuilderReferenceToolProperties()
{
	bShowReferenceImage = false;
	bLoadReferenceImage = false;
	bClearReferenceImage = false;
	ReferenceImageScale = 500;
	ReferenceImageOpacity = 100;
	ReferenceImageOffsetX = 0;
	ReferenceImageOffsetY = 0;
	ReferenceImageHeight = 0;
}

// ===== Builder =====

UInteractiveTool* UWallBuilderReferenceToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UWallBuilderReferenceTool* NewTool = NewObject<UWallBuilderReferenceTool>(SceneState.ToolManager);
	NewTool->SetWorld(SceneState.World);
	NewTool->SetParentMode(ParentMode);
	return NewTool;
}

// ===== Tool =====

void UWallBuilderReferenceTool::SetWorld(UWorld* World)
{
	TargetWorld = World;
}

void UWallBuilderReferenceTool::SetParentMode(UWallBuilderEditorMode* InMode)
{
	ParentMode = InMode;
}

void UWallBuilderReferenceTool::Setup()
{
	USingleClickTool::Setup();

	Properties = NewObject<UWallBuilderReferenceToolProperties>(this);
	AddToolPropertySource(Properties);

	// 先查找世界中的已有参考图 Actor（跨工具持久化）
	ReferenceImageActor = FindExistingActor();

	// 从持久化恢复设置
	if (ParentMode)
	{
		Properties->bShowReferenceImage = ParentMode->SavedShowReferenceImage;
		Properties->ReferenceImageScale = ParentMode->SavedReferenceImageScale;
		Properties->ReferenceImageOpacity = ParentMode->SavedReferenceImageOpacity;
		Properties->ReferenceImageOffsetX = ParentMode->SavedReferenceImageOffsetX;
		Properties->ReferenceImageOffsetY = ParentMode->SavedReferenceImageOffsetY;
		Properties->ReferenceImageHeight = ParentMode->SavedReferenceImageHeight;

		// 如果应该显示但没有 actor，且已有路径，则重新创建
		if (Properties->bShowReferenceImage && !ReferenceImageActor && !ParentMode->SavedReferenceImagePath.IsEmpty())
		{
			LoadReferenceImageFromPath(ParentMode->SavedReferenceImagePath);
		}
	}
}

void UWallBuilderReferenceTool::Shutdown(EToolShutdownType ShutdownType)
{
	// 不销毁参考图 Actor - 保存状态，下次进入恢复
	ApplyPersistentSettings();
	USingleClickTool::Shutdown(ShutdownType);
}

void UWallBuilderReferenceTool::OnPropertyModified(UObject* PropertySet, FProperty* Property)
{
	if (!Property) return;

	ApplyPersistentSettings();

	FName PropName = Property->GetFName();

	if (PropName == GET_MEMBER_NAME_CHECKED(UWallBuilderReferenceToolProperties, bLoadReferenceImage))
	{
		Properties->bLoadReferenceImage = false;
		if (Properties->bShowReferenceImage)
			LoadReferenceImage();
	}
	else if (PropName == GET_MEMBER_NAME_CHECKED(UWallBuilderReferenceToolProperties, bClearReferenceImage))
	{
		Properties->bClearReferenceImage = false;
		ClearReferenceImage();
	}
	else if (PropName == GET_MEMBER_NAME_CHECKED(UWallBuilderReferenceToolProperties, bShowReferenceImage))
	{
		if (Properties->bShowReferenceImage)
		{
			if (!ReferenceImageActor && ParentMode && !ParentMode->SavedReferenceImagePath.IsEmpty())
			{
				LoadReferenceImageFromPath(ParentMode->SavedReferenceImagePath);
			}
		}
		else
		{
			DestroyReferenceImage();
		}
	}
	else if (PropName == GET_MEMBER_NAME_CHECKED(UWallBuilderReferenceToolProperties, ReferenceImageScale) ||
		PropName == GET_MEMBER_NAME_CHECKED(UWallBuilderReferenceToolProperties, ReferenceImageOffsetX) ||
		PropName == GET_MEMBER_NAME_CHECKED(UWallBuilderReferenceToolProperties, ReferenceImageOffsetY) ||
		PropName == GET_MEMBER_NAME_CHECKED(UWallBuilderReferenceToolProperties, ReferenceImageHeight))
	{
		UpdateReferenceImage();
	}
	else if (PropName == GET_MEMBER_NAME_CHECKED(UWallBuilderReferenceToolProperties, ReferenceImageOpacity))
	{
		if (ReferenceImageActor)
			ReferenceImageActor->SetOpacity((float)Properties->ReferenceImageOpacity / 100.0f);
	}
}

void UWallBuilderReferenceTool::ApplyPersistentSettings()
{
	if (ParentMode)
	{
		ParentMode->SavedShowReferenceImage = Properties->bShowReferenceImage;
		ParentMode->SavedReferenceImageScale = Properties->ReferenceImageScale;
		ParentMode->SavedReferenceImageOpacity = Properties->ReferenceImageOpacity;
		ParentMode->SavedReferenceImageOffsetX = Properties->ReferenceImageOffsetX;
		ParentMode->SavedReferenceImageOffsetY = Properties->ReferenceImageOffsetY;
		ParentMode->SavedReferenceImageHeight = Properties->ReferenceImageHeight;
	}
}

void UWallBuilderReferenceTool::LoadReferenceImage()
{
	if (!TargetWorld) return;

	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform) return;

	TArray<FString> OutFiles;
	bool bOpened = DesktopPlatform->OpenFileDialog(
		FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
		TEXT("Select Reference Image"),
		TEXT(""),
		TEXT(""),
		TEXT("Image Files (*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.exr)|*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.exr|All Files (*.*)|*.*"),
		EFileDialogFlags::None,
		OutFiles
	);

	if (!bOpened || OutFiles.Num() == 0) return;

	LoadReferenceImageFromPath(OutFiles[0]);
}

void UWallBuilderReferenceTool::LoadReferenceImageFromPath(const FString& FilePath)
{
	if (!TargetWorld || FilePath.IsEmpty()) return;

	DestroyReferenceImage();

	FActorSpawnParameters SpawnParams;
	SpawnParams.bAllowDuringConstructionScript = true;
	ReferenceImageActor = TargetWorld->SpawnActor<AWallBuilderFloorPlanActor>(
		AWallBuilderFloorPlanActor::StaticClass(), FTransform::Identity, SpawnParams);

	if (!ReferenceImageActor) return;

	ReferenceImageActor->SetActorLabel(TEXT("FloorPlanRef"));

	if (!ReferenceImageActor->LoadImage(FilePath))
	{
		ReferenceImageActor->Destroy();
		ReferenceImageActor = nullptr;
		return;
	}

	// 保存路径
	if (ParentMode)
	{
		ParentMode->SavedReferenceImagePath = FilePath;
	}

	Properties->bShowReferenceImage = true;
	UpdateReferenceImage();
	ReferenceImageActor->SetOpacity((float)Properties->ReferenceImageOpacity / 100.0f);
}

void UWallBuilderReferenceTool::ClearReferenceImage()
{
	DestroyReferenceImage();
	Properties->bShowReferenceImage = false;
	if (ParentMode)
	{
		ParentMode->SavedReferenceImagePath.Empty();
	}
	ApplyPersistentSettings();
}

void UWallBuilderReferenceTool::UpdateReferenceImage()
{
	if (!ReferenceImageActor) return;

	ReferenceImageActor->SetHeight((float)Properties->ReferenceImageHeight);
	ReferenceImageActor->SetImageScale((float)Properties->ReferenceImageScale);
	ReferenceImageActor->SetImagePosition(FVector2D(
		(float)Properties->ReferenceImageOffsetX,
		(float)Properties->ReferenceImageOffsetY));
}

void UWallBuilderReferenceTool::DestroyReferenceImage()
{
	if (ReferenceImageActor)
	{
		ReferenceImageActor->Destroy();
		ReferenceImageActor = nullptr;
	}
}

AWallBuilderFloorPlanActor* UWallBuilderReferenceTool::FindExistingActor() const
{
	if (!TargetWorld || !TargetWorld->PersistentLevel) return nullptr;
	for (AActor* Actor : TargetWorld->PersistentLevel->Actors)
	{
		if (AWallBuilderFloorPlanActor* RefActor = Cast<AWallBuilderFloorPlanActor>(Actor))
		{
			if (RefActor->GetTexture())
				return RefActor;
		}
	}
	return nullptr;
}
