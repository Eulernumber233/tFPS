// Copyright Epic Games, Inc. All Rights Reserved.

#include "WallBuilderEditorModeToolkit.h"
#include "Engine/Selection.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "EditorModeManager.h"

#define LOCTEXT_NAMESPACE "WallBuilderEditorModeToolkit"

FWallBuilderEditorModeToolkit::FWallBuilderEditorModeToolkit()
{
}

void FWallBuilderEditorModeToolkit::Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode)
{
	FModeToolkit::Init(InitToolkitHost, InOwningMode);
}

void FWallBuilderEditorModeToolkit::GetToolPaletteNames(TArray<FName>& PaletteNames) const
{
	PaletteNames.Add(NAME_Default);
}

FName FWallBuilderEditorModeToolkit::GetToolkitFName() const
{
	return FName("WallBuilderEditorMode");
}

FText FWallBuilderEditorModeToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("DisplayName", "WallBuilder");
}

#undef LOCTEXT_NAMESPACE
