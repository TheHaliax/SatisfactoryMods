// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Appearance/FPCAppearanceSpec.h"
#include "Appearance/IAppearanceCatalog.h"
#include "Resources/FGItemDescriptor.h"
#include "Templates/SubclassOf.h"

enum class EPCPaintFinishKind : uint8
{
	Default,
	Matte,
	MetallicColor,
};

struct FPCFluidCatalogEntry
{
	FName FluidStem;
	FLinearColor Primary;
	EPCPaintFinishKind Finish = EPCPaintFinishKind::Default;
	TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> SwatchClass;
};

class FPCFluidAppearanceCatalog final : public IAppearanceCatalog
{
public:
	static FPCFluidAppearanceCatalog& Get();

	void EnsureLoaded();

	virtual bool Resolve(
		TSubclassOf<UFGItemDescriptor> FluidDescriptor,
		bool bEmpty,
		FPCAppearanceSpec& OutSpec) const override;

	const FPCAppearanceSpec& GetNeutral() const { return NeutralSpec; }

	bool ResolveByKey(FName CatalogKey, FPCAppearanceSpec& OutSpec) const;

	TSubclassOf<UFGFactoryCustomizationDescriptor_PaintFinish> GetFinishClass(
		EPCPaintFinishKind Kind) const;

private:
	FPCFluidAppearanceCatalog() = default;

	void BuildEntries();
	static FLinearColor HexRgb(uint8 R, uint8 G, uint8 B);
	static TSubclassOf<UFGItemDescriptor> LoadFluidDesc(const TCHAR* SoftPath);
	static TSubclassOf<UFGFactoryCustomizationDescriptor_PaintFinish> LoadFinish(
		const TCHAR* SoftPath,
		const TCHAR* Label);

	bool bBuilt = false;
	FPCAppearanceSpec NeutralSpec;
	TMap<TSubclassOf<UFGItemDescriptor>, FPCFluidCatalogEntry> ByDescriptor;
	TSubclassOf<UFGFactoryCustomizationDescriptor_PaintFinish> FinishDefault;
	TSubclassOf<UFGFactoryCustomizationDescriptor_PaintFinish> FinishMatte;
	TSubclassOf<UFGFactoryCustomizationDescriptor_PaintFinish> FinishMetallicColor;
};
