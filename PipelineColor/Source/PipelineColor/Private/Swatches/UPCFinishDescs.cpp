// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Swatches/UPCFinishDescs.h"

#include "UObject/SoftObjectPath.h"

UPCFinish_MetallicColor::UPCFinish_MetallicColor()
{
	mUseDisplayNameAndDescription = true;
	mDisplayName = FText::FromString(TEXT("PC Metallic Color"));
	mDescription = FText::FromString(TEXT("Metallic 1 — Roughness from Primary V"));
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
	if (UClass* ChromeClass =
		ChromePath.TryLoadClass<UFGFactoryCustomizationDescriptor_PaintFinish>())
	{
		if (const UFGFactoryCustomizationDescriptor_PaintFinish* Chrome =
			ChromeClass->GetDefaultObject<UFGFactoryCustomizationDescriptor_PaintFinish>())
		{
			Self->mIcon = Chrome->mIcon;
		}
	}
}
