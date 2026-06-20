// Copyright Epic Games, Inc. All Rights Reserved.

#include "WallBuilderModule.h"
#include "WallBuilderEditorModeCommands.h"

#define LOCTEXT_NAMESPACE "WallBuilderModule"

void FWallBuilderModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	FWallBuilderEditorModeCommands::Register();
}

void FWallBuilderModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	FWallBuilderEditorModeCommands::Unregister();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FWallBuilderModule, WallBuilder)