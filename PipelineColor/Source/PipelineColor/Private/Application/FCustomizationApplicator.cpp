// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Application/FCustomizationApplicator.h"

#include "Buildables/FGBuildable.h"
#include "FGFactoryColoringTypes.h"
#include "PipelineColorLog.h"
#include "Swatches/UPCFinishDescs.h"
#include "UObject/SoftObjectPath.h"

namespace
{
TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> LoadCustomSwatch()
{
	static TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Cached;
	if (!Cached)
	{
		const FSoftClassPath Path(TEXT(
			"/Game/FactoryGame/Buildable/-Shared/Customization/Swatches/"
			"SwatchDesc_Custom.SwatchDesc_Custom_C"));
		Cached = Path.TryLoadClass<UFGFactoryCustomizationDescriptor_Swatch>();
	}
	return Cached;
}

void ApplyRoughnessToMetallicCDO(float Roughness)
{
	if (UPCFinish_MetallicColor* CDO = GetMutableDefault<UPCFinish_MetallicColor>())
	{
		CDO->RoughnessValue = Roughness;
		CDO->MetallicValue = 1.f;
		CDO->mHasForcedColor = false;
	}
}
} // namespace

bool FCustomizationApplicator::ApplyIfChanged(AFGBuildable* Buildable, const FPCAppearanceSpec& Spec)
{
	if (!IsValid(Buildable) || !Buildable->HasAuthority())
	{
		return false;
	}

	// SwatchDesc alone does not drive spline Primary/Finish — Custom + Override.
	TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> PaintSwatch = LoadCustomSwatch();
	if (!PaintSwatch)
	{
		PaintSwatch = Spec.SwatchDesc;
	}

	FFactoryCustomizationData Data = Buildable->GetCustomizationData_Native();
	const bool bSameColors =
		Data.SwatchDesc == PaintSwatch
		&& Data.ColorSlot == INDEX_CUSTOM_COLOR_SLOT
		&& Data.OverrideColorData.PaintFinish == Spec.PaintFinish
		&& Data.OverrideColorData.PrimaryColor.Equals(Spec.PrimaryColor, 0.001f)
		&& Data.OverrideColorData.SecondaryColor.Equals(Spec.SecondaryColor, 0.001f);

	if (bSameColors && !Spec.bOverrideRoughness)
	{
		return false;
	}

	if (Spec.bOverrideRoughness
		&& Spec.PaintFinish == UPCFinish_MetallicColor::StaticClass())
	{
		ApplyRoughnessToMetallicCDO(Spec.RoughnessValue);
	}

	FFactoryCustomizationData Next;
	Next.InlineCombine(Data);
	Next.SwatchDesc = PaintSwatch;
	Next.ColorSlot = INDEX_CUSTOM_COLOR_SLOT;
	Next.OverrideColorData.PrimaryColor = Spec.PrimaryColor;
	Next.OverrideColorData.SecondaryColor = Spec.SecondaryColor;
	Next.OverrideColorData.PaintFinish = Spec.PaintFinish;

	Buildable->SetCustomizationData_Native(Next, /*skipCombine=*/true);

	UE_LOG(LogPipelineColor, Log, TEXT("%s apply %s key=%s primary=(%.2f,%.2f,%.2f)"),
		PIPELINECOLOR_LOG_PREFIX,
		*GetNameSafe(Buildable),
		*Spec.CatalogKey.ToString(),
		Spec.PrimaryColor.R,
		Spec.PrimaryColor.G,
		Spec.PrimaryColor.B);

	return true;
}
