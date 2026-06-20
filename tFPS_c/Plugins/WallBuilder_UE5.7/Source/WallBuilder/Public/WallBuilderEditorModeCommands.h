// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"

/**
 * WallBuilder 编辑器模式命令
 * 
 * 工具栏顺序（从左到右）：
 * 1. SelectTool - 选择编辑工具（默认）
 * 2. WallTool - 绘制墙体
 * 3. FloorTool - 绘制楼板
 * 4. StairTool - 绘制楼梯
 * 5. RampTool - 绘制斜坡
 * 6. ReferenceTool - 参考图像
 */
class FWallBuilderEditorModeCommands : public TCommands<FWallBuilderEditorModeCommands>
{
public:
	FWallBuilderEditorModeCommands();

	virtual void RegisterCommands() override;
	static TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> GetCommands();

	// === 核心编辑工具 ===
	TSharedPtr<FUICommandInfo> SelectTool;      // 选择编辑工具

	// === 绘制工具 ===
	TSharedPtr<FUICommandInfo> WallTool;        // 绘制墙体
	TSharedPtr<FUICommandInfo> FloorTool;       // 绘制楼板
	TSharedPtr<FUICommandInfo> StairTool;       // 绘制楼梯
	TSharedPtr<FUICommandInfo> RampTool;        // 绘制斜坡

	// === 辅助工具 ===
	TSharedPtr<FUICommandInfo> ReferenceTool;   // 参考图像

protected:
	TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> Commands;
};
