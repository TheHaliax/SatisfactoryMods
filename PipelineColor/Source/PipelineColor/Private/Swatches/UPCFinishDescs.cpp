// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Swatches/UPCFinishDescs.h"

#include "PipelineColorLog.h"
#include "UObject/SoftObjectPath.h"

UPCFinish_MetallicColor::UPCFinish_MetallicColor()
{
	mUseDisplayNameAndDescription = true;
	mDisplayName = NSLOCTEXT("PipelineColor", "FinishMetallicColor", "PC Metallic Color");
	mDescription = NSLOCTEXT(
		"PipelineColor",
		"FinishMetallicColorDesc",
		"Metallic 1 — Roughness from Primary V");
	ID = INDEX_CUSTOM_COLOR_SLOT;
	RoughnessValue = 0.4f;
	MetallicValue = 1.0f;
	mHasForcedColor = false;
	mForcedColor = FLinearColor::White;
	mValidBuildables.Reset();
}

void UPCFinish_MetallicColor::EnsureIconLoaded()
{
	UFGFactoryCustomizationDescriptor_PaintFinish* Self =
		GetMutableDefault<UPCFinish_MetallicColor>();
	if (!Self || Self->mIcon.IsValid())
	{
		return;
	}

	const FSoftClassPath ChromePath(TEXT(
		"/Game/FactoryGame/Buildable/-Shared/Customization/PaintFinishes/Metals/"
		"PaintFinishDesc_Chrome.PaintFinishDesc_Chrome_C"));
	UClass* ChromeClass =
		ChromePath.TryLoadClass<UFGFactoryCustomizationDescriptor_PaintFinish>();
	if (!ChromeClass)
	{
		UE_LOG(LogPipelineColor, Warning,
			TEXT("%s EnsureIconLoaded: Chrome finish missing"), PIPELINECOLOR_LOG_PREFIX);
		return;
	}

	const UFGFactoryCustomizationDescriptor_PaintFinish* Chrome =
		ChromeClass->GetDefaultObject<UFGFactoryCustomizationDescriptor_PaintFinish>();
	if (!Chrome)
	{
		UE_LOG(LogPipelineColor, Warning,
			TEXT("%s EnsureIconLoaded: Chrome CDO missing"), PIPELINECOLOR_LOG_PREFIX);
		return;
	}

	Self->mIcon = Chrome->mIcon;
}
