// Copyright Epic Games, Inc. All Rights Reserved.

#include "WallBuilderEditorMode.h"
#include "WallBuilderEditorModeToolkit.h"

// UE 5.7+ moved this header to Tools/ subfolder
#include "EdModeInteractiveToolsContext.h"

#include "InteractiveToolManager.h"
#include "WallBuilderEditorModeCommands.h"
#include "Modules/ModuleManager.h"

#include "Tools/WallBuilderReferenceTool.h"
#include "Tools/WallBuilderSelectTool.h"
#include "Tools/WallBuilderWallTool.h"
#include "Tools/WallBuilderFloorTool.h"
#include "Tools/WallBuilderStairTool.h"
#include "Tools/WallBuilderRampTool.h"

#define LOCTEXT_NAMESPACE "WallBuilderEditorMode"

const FEditorModeID UWallBuilderEditorMode::EM_WallBuilderEditorModeId = TEXT("EM_WallBuilderEditorMode");

// 工具名称常量
FString UWallBuilderEditorMode::SelectToolName = TEXT("WallBuilder_SelectTool");
FString UWallBuilderEditorMode::WallToolName = TEXT("WallBuilder_WallTool");
FString UWallBuilderEditorMode::FloorToolName = TEXT("WallBuilder_FloorTool");
FString UWallBuilderEditorMode::StairToolName = TEXT("WallBuilder_StairTool");
FString UWallBuilderEditorMode::RampToolName = TEXT("WallBuilder_RampTool");
FString UWallBuilderEditorMode::ReferenceToolName = TEXT("WallBuilder_ReferenceTool");

UWallBuilderEditorMode::UWallBuilderEditorMode()
{
	FModuleManager::Get().LoadModule("EditorStyle");

	Info = FEditorModeInfo(UWallBuilderEditorMode::EM_WallBuilderEditorModeId,
		LOCTEXT("ModeName", "WallBuilder"),
		FSlateIcon(),
		true);
}

UWallBuilderEditorMode::~UWallBuilderEditorMode()
{
}

void UWallBuilderEditorMode::ActorSelectionChangeNotify()
{
}

void UWallBuilderEditorMode::Enter()
{
	UEdMode::Enter();

	const FWallBuilderEditorModeCommands& Commands = FWallBuilderEditorModeCommands::Get();

	// === 核心编辑工具 ===
	// 选择工具（默认激活）
	UWallBuilderSelectToolBuilder* SelectToolBuilder = NewObject<UWallBuilderSelectToolBuilder>(this);
	SelectToolBuilder->SetParentMode(this);
	RegisterTool(Commands.SelectTool, SelectToolName, SelectToolBuilder);

	// === 绘制工具 ===
	// 绘制墙体
	UWallBuilderWallToolBuilder* WallToolBuilder = NewObject<UWallBuilderWallToolBuilder>(this);
	WallToolBuilder->SetParentMode(this);
	RegisterTool(Commands.WallTool, WallToolName, WallToolBuilder);

	// 绘制楼板
	UWallBuilderFloorToolBuilder* FloorToolBuilder = NewObject<UWallBuilderFloorToolBuilder>(this);
	FloorToolBuilder->SetParentMode(this);
	RegisterTool(Commands.FloorTool, FloorToolName, FloorToolBuilder);

	// 绘制楼梯
	UWallBuilderStairToolBuilder* StairToolBuilder = NewObject<UWallBuilderStairToolBuilder>(this);
	StairToolBuilder->SetParentMode(this);
	RegisterTool(Commands.StairTool, StairToolName, StairToolBuilder);

	// 绘制斜坡
	UWallBuilderRampToolBuilder* RampToolBuilder = NewObject<UWallBuilderRampToolBuilder>(this);
	RampToolBuilder->SetParentMode(this);
	RegisterTool(Commands.RampTool, RampToolName, RampToolBuilder);

	// === 辅助工具 ===
	// 参考图像
	UWallBuilderReferenceToolBuilder* RefToolBuilder = NewObject<UWallBuilderReferenceToolBuilder>(this);
	RefToolBuilder->SetParentMode(this);
	RegisterTool(Commands.ReferenceTool, ReferenceToolName, RefToolBuilder);

	// 默认激活选择工具
	GetToolManager()->SelectActiveToolType(EToolSide::Left, SelectToolName);
}

void UWallBuilderEditorMode::CreateToolkit()
{
	Toolkit = MakeShareable(new FWallBuilderEditorModeToolkit);
}

TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> UWallBuilderEditorMode::GetModeCommands() const
{
	return FWallBuilderEditorModeCommands::Get().GetCommands();
}

#undef LOCTEXT_NAMESPACE
