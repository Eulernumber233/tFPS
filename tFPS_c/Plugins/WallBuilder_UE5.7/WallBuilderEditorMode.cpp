// Copyright Epic Games, Inc. All Rights Reserved.

#include "WallBuilderEditorMode.h"
#include "WallBuilderEditorModeToolkit.h"

// UE 5.7+ uses new header path
#include "Tools/EditorInteractiveToolsContext.h"

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

FString UWallBuilderEditorMode::SelectToolName = TEXT("WallBuilder_SelectTool");
FString UWallBuilderEditorMode::WallToolName = TEXT("WallBuilder_WallTool");
FString UWallBuilderEditorMode::FloorToolName = TEXT("WallBuilder_FloorTool");
FString UWallBuilderEditorMode::StairToolName = TEXT("WallBuilder_StairTool");
FString UWallBuilderEditorMode::RampToolName = TEXT("WallBuilder_RampTool");
FString UWallBuilderEditorMode::ReferenceToolName = TEXT("WallBuilder_ReferenceTool");
