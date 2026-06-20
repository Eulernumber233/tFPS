// Copyright Epic Games, Inc. All Rights Reserved.

#include "WallBuilderEditorModeCommands.h"
#include "WallBuilderEditorMode.h"
#include "EditorStyleSet.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateStyle.h"

#define LOCTEXT_NAMESPACE "WallBuilderEditorModeCommands"

// 自定义样式集名称
static FName WallBuilderStyleName("WallBuilderStyle");

FWallBuilderEditorModeCommands::FWallBuilderEditorModeCommands()
	: TCommands<FWallBuilderEditorModeCommands>("WallBuilderEditorMode",
		NSLOCTEXT("WallBuilderEditorMode", "WallBuilderEditorModeCommands", "WallBuilder Editor Mode"),
		NAME_None,
		FAppStyle::GetAppStyleSetName())
{
}

void FWallBuilderEditorModeCommands::RegisterCommands()
{
	TArray <TSharedPtr<FUICommandInfo>>& ToolCommands = Commands.FindOrAdd(NAME_Default);

	// === 核心编辑工具 ===
	// 选择工具（默认激活）
	UI_COMMAND(SelectTool, 
		"Select", 
		"Select and edit walls, floors, stairs, and ramps", 
		EUserInterfaceActionType::ToggleButton, 
		FInputChord());
	ToolCommands.Add(SelectTool);

	// === 绘制工具 ===
	// 绘制墙体
	UI_COMMAND(WallTool, 
		"Wall", 
		"Draw walls by clicking to place points", 
		EUserInterfaceActionType::ToggleButton, 
		FInputChord());
	ToolCommands.Add(WallTool);

	// 绘制楼板
	UI_COMMAND(FloorTool, 
		"Floor", 
		"Draw floor slabs by defining boundary points", 
		EUserInterfaceActionType::ToggleButton, 
		FInputChord());
	ToolCommands.Add(FloorTool);

	// 绘制楼梯
	UI_COMMAND(StairTool, 
		"Stair", 
		"Draw stairs with specified height and steps", 
		EUserInterfaceActionType::ToggleButton, 
		FInputChord());
	ToolCommands.Add(StairTool);

	// 绘制斜坡
	UI_COMMAND(RampTool, 
		"Ramp", 
		"Draw ramps with start and end heights", 
		EUserInterfaceActionType::ToggleButton, 
		FInputChord());
	ToolCommands.Add(RampTool);

	// === 辅助工具 ===
	// 参考图像
	UI_COMMAND(ReferenceTool, 
		"Reference", 
		"Display reference image overlay for tracing", 
		EUserInterfaceActionType::ToggleButton, 
		FInputChord());
	ToolCommands.Add(ReferenceTool);
}

TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> FWallBuilderEditorModeCommands::GetCommands()
{
	return FWallBuilderEditorModeCommands::Get().Commands;
}

#undef LOCTEXT_NAMESPACE
