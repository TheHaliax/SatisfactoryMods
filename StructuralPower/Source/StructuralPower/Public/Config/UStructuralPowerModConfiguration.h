// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "Configuration/ModConfiguration.h"
#include "Configuration/Properties/ConfigPropertyBool.h"
#include "Configuration/Properties/ConfigPropertyFloat.h"
#include "Configuration/Properties/ConfigPropertySection.h"
#include "Configuration/Properties/WidgetExtension/CP_Float.h"
#include "Configuration/Properties/WidgetExtension/CP_Section.h"
#include "UStructuralPowerModConfiguration.generated.h"

class UGameInstance;
class UConfigPropertySection;
class UUserWidget;

UCLASS()
class STRUCTURALPOWER_API UStructuralPowerConfigFloat : public UConfigPropertyFloat
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Default", BlueprintReadOnly, meta = (DisplayAfter = "DefaultValue"))
	ECP_FloatWidgetType WidgetType = ECP_FloatWidgetType::CPF_Spinbox;

	UPROPERTY(EditAnywhere, Category = "Default", BlueprintReadOnly, meta = (DisplayAfter = "WidgetType"))
	float MinValue = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Default", BlueprintReadOnly, meta = (DisplayAfter = "MinValue"))
	float MaxValue = 10.0f;
};

UCLASS()
class STRUCTURALPOWER_API UStructuralPowerConfigNestedSection : public UConfigPropertySection
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Default", BlueprintReadOnly, meta = (DisplayAfter = "DefaultValue"))
	ECP_SectionWidgetType WidgetType = ECP_SectionWidgetType::CPS_Vertical;

	UPROPERTY(EditAnywhere, Category = "Default", BlueprintReadOnly, meta = (DisplayAfter = "DefaultValue"))
	bool HasHeader = true;

	UPROPERTY(EditAnywhere, Category = "Default", BlueprintReadOnly, meta = (DisplayAfter = "HasHeader"))
	FText HeaderText;

	UPROPERTY(EditAnywhere, Category = "Default", BlueprintReadOnly, meta = (DisplayAfter = "HeaderText"))
	bool Collapsed = true;
};

UCLASS()
class STRUCTURALPOWER_API UStructuralPowerConfigRootSection : public UConfigPropertySection
{
	GENERATED_BODY()

public:
	virtual UUserWidget* CreateEditorWidget_Implementation(UUserWidget* ParentWidget) const override;
};

UCLASS()
class UStructuralPowerConfigLiveSync : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TObjectPtr<UGameInstance> BoundGameInstance;

	UFUNCTION()
	void HandlePropertyChanged();
};

STRUCTURALPOWER_API void StructuralPowerUpgradeConfigPropertiesForSmlWidgets(UConfigPropertySection* Root);

UCLASS()
class STRUCTURALPOWER_API UStructuralPowerModConfiguration : public UModConfiguration
{
	GENERATED_BODY()

public:
	UStructuralPowerModConfiguration();
};
