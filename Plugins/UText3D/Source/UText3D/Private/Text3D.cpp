// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Text3D.h"

#define LOCTEXT_NAMESPACE "FText3DModule"

void FText3DModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FText3DModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FText3DModule, Text3D)

DEFINE_LOG_CATEGORY(Text3D)