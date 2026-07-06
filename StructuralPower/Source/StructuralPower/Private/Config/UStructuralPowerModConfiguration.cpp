// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Config/UStructuralPowerModConfiguration.h"

#include "Blueprint/UserWidget.h"
#include "Config/FStructuralPowerModConfig.h"
#include "Configuration/ConfigManager.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "Configuration/Properties/ConfigPropertyBool.h"
#include "Configuration/Properties/ConfigPropertyFloat.h"
#include "Configuration/Properties/ConfigPropertySection.h"
#include "Configuration/Properties/WidgetExtension/CP_Bool.h"
#include "Configuration/Properties/WidgetExtension/CP_Float.h"
#include "Configuration/Properties/WidgetExtension/CP_Section.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet/BlueprintAssetHelperLibrary.h"
#include "StructuralPowerLog.h"
#include "UObject/UObjectIterator.h"

namespace
{
UClass* ResolveSmlConfigPropertyClassRuntime(UClass* NativeBase, const TCHAR* PreferredClassName)
{
	UClass* Preferred = nullptr;
	UClass* SmlFallback = nullptr;

	TArray<UClass*> Candidates;
	GetDerivedClasses(NativeBase, Candidates);
	for (UClass* Candidate : Candidates)
	{
		if (!Candidate || Candidate->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
		{
			continue;
		}

		if (!Candidate->IsChildOf(NativeBase))
		{
			continue;
		}

		if (Candidate->GetName().Equals(PreferredClassName))
		{
			Preferred = Candidate;
			break;
		}

		if (!SmlFallback
			&& Cast<UBlueprintGeneratedClass>(Candidate)
			&& UBlueprintAssetHelperLibrary::FindPluginNameByObjectPath(Candidate->GetPathName()) == TEXT("SML"))
		{
			SmlFallback = Candidate;
		}
	}

	if (Preferred)
	{
		return Preferred;
	}

	if (SmlFallback)
	{
		UE_LOG(LogStructuralPower, Log,
			TEXT("Using SML config property class %s for %s"),
			*SmlFallback->GetName(),
			*NativeBase->GetName());
		return SmlFallback;
	}

	return nullptr;
}

bool IsSmlBlueprintConfigPropertyClass(UClass* Class)
{
	return IsValid(Class)
		&& Cast<UBlueprintGeneratedClass>(Class)
		&& UBlueprintAssetHelperLibrary::FindPluginNameByObjectPath(Class->GetPathName()) == TEXT("SML");
}

void CopyBoolPropertyState(UConfigPropertyBool* Source, UConfigPropertyBool* Dest)
{
	if (!IsValid(Source) || !IsValid(Dest))
	{
		return;
	}

	Dest->DisplayName = Source->DisplayName;
	Dest->Tooltip = Source->Tooltip;
	Dest->DefaultValue = Source->DefaultValue;
	Dest->Value = Source->Value;
	Dest->bHidden = Source->bHidden;
	Dest->bAllowUserReset = Source->bAllowUserReset;
	Dest->bRequiresWorldReload = Source->bRequiresWorldReload;
}

void CopyFloatPropertyState(UConfigPropertyFloat* Source, UConfigPropertyFloat* Dest)
{
	if (!IsValid(Source) || !IsValid(Dest))
	{
		return;
	}

	Dest->DisplayName = Source->DisplayName;
	Dest->Tooltip = Source->Tooltip;
	Dest->DefaultValue = Source->DefaultValue;
	Dest->Value = Source->Value;
	Dest->bHidden = Source->bHidden;
	Dest->bAllowUserReset = Source->bAllowUserReset;
	Dest->bRequiresWorldReload = Source->bRequiresWorldReload;

	if (UCP_Float* DestFloat = Cast<UCP_Float>(Dest))
	{
		ECP_FloatWidgetType WidgetType = ECP_FloatWidgetType::CPF_Spinbox;
		float MinValue = 1.0f;
		float MaxValue = 10.0f;

		if (const UCP_Float* SourceFloat = Cast<UCP_Float>(Source))
		{
			WidgetType = SourceFloat->WidgetType;
			MinValue = SourceFloat->MinValue;
			MaxValue = SourceFloat->MaxValue;
		}
		else if (const UStructuralPowerConfigFloat* NativeFloat = Cast<UStructuralPowerConfigFloat>(Source))
		{
			WidgetType = NativeFloat->WidgetType;
			MinValue = NativeFloat->MinValue;
			MaxValue = NativeFloat->MaxValue;
		}

		DestFloat->WidgetType = WidgetType;
		DestFloat->MinValue = MinValue;
		DestFloat->MaxValue = MaxValue;
	}
}

UConfigPropertyBool* AddBoolProperty(
	UObject* Outer,
	UConfigPropertySection* Section,
	const FString& Key,
	const FText& DisplayName,
	const FText& Tooltip,
	bool bDefault)
{
	UConfigPropertyBool* Property = NewObject<UConfigPropertyBool>(Outer, UConfigPropertyBool::StaticClass(), *Key);
	Property->DisplayName = DisplayName;
	Property->Tooltip = Tooltip;
	Property->DefaultValue = bDefault;
	Property->Value = bDefault;
	Section->SectionProperties.Add(Key, Property);
	return Property;
}

UStructuralPowerConfigFloat* AddFloatProperty(
	UObject* Outer,
	UConfigPropertySection* Section,
	const FString& Key,
	const FText& DisplayName,
	const FText& Tooltip,
	float DefaultValue)
{
	UStructuralPowerConfigFloat* Property = NewObject<UStructuralPowerConfigFloat>(Outer, *Key);
	Property->DisplayName = DisplayName;
	Property->Tooltip = Tooltip;
	Property->DefaultValue = DefaultValue;
	Property->Value = DefaultValue;
	Property->WidgetType = ECP_FloatWidgetType::CPF_Spinbox;
	Property->MinValue = 1.0f;
	Property->MaxValue = 10.0f;
	Section->SectionProperties.Add(Key, Property);
	return Property;
}

UStructuralPowerConfigNestedSection* AddDebugSection(UObject* Outer, UConfigPropertySection* Parent)
{
	UStructuralPowerConfigNestedSection* Section = NewObject<UStructuralPowerConfigNestedSection>(Outer, TEXT("Debug"));
	Section->DisplayName = FText::GetEmpty();
	Section->Tooltip = NSLOCTEXT(
		"StructuralPower",
		"DebugSectionTip",
		"Advanced toggles. Chat: !tracetoggle for trace logging.");
	Section->WidgetType = ECP_SectionWidgetType::CPS_Vertical;
	Section->HasHeader = true;
	Section->HeaderText = NSLOCTEXT("StructuralPower", "DebugSectionHeader", "Debug");
	Section->Collapsed = true;
	Parent->SectionProperties.Add(TEXT("Debug"), Section);
	return Section;
}

void CopySectionWidgetState(UConfigPropertySection* Source, UCP_Section* Dest)
{
	if (!IsValid(Source) || !IsValid(Dest))
	{
		return;
	}

	Dest->DisplayName = Source->DisplayName;
	Dest->Tooltip = Source->Tooltip;

	if (const UStructuralPowerConfigNestedSection* NativeSection = Cast<UStructuralPowerConfigNestedSection>(Source))
	{
		Dest->WidgetType = NativeSection->WidgetType;
		Dest->HasHeader = NativeSection->HasHeader;
		Dest->HeaderText = NativeSection->HeaderText;
		Dest->Collapsed = NativeSection->Collapsed;
		return;
	}

	if (const UCP_Section* SourceSection = Cast<UCP_Section>(Source))
	{
		Dest->WidgetType = SourceSection->WidgetType;
		Dest->HasHeader = SourceSection->HasHeader;
		Dest->HeaderText = SourceSection->HeaderText;
		Dest->Collapsed = SourceSection->Collapsed;
	}
}

void InitializeSchema(UConfigPropertySection* Root)
{
	if (!IsValid(Root))
	{
		return;
	}

	Root->SectionProperties.Empty();

	AddFloatProperty(
		Root,
		Root,
		TEXT("HoverpackStructuralHorizontalMultiplier"),
		NSLOCTEXT("StructuralPower", "HoverpackHorizontalMult", "Hoverpack horizontal multiplier"),
		NSLOCTEXT(
			"StructuralPower",
			"HoverpackHorizontalMultTip",
			"Structural tether horizontal reach = base radius x this value (1.0-10.0). Chat: !HoverH"),
		1.5f);

	AddFloatProperty(
		Root,
		Root,
		TEXT("HoverpackStructuralVerticalMultiplier"),
		NSLOCTEXT("StructuralPower", "HoverpackVerticalMult", "Hoverpack vertical multiplier"),
		NSLOCTEXT(
			"StructuralPower",
			"HoverpackVerticalMultTip",
			"Structural tether vertical reach = base radius x this value (1.0-10.0). Chat: !HoverV"),
		1.5f);

	AddBoolProperty(
		Root,
		Root,
		TEXT("GroupLighting"),
		NSLOCTEXT("StructuralPower", "GroupLighting", "Structural lighting"),
		NSLOCTEXT(
			"StructuralPower",
			"GroupLightingTip",
			"Lights on powered foundations draw without wires (v2.2 M3). Default OFF. Chat: !lighting"),
		false);

	UStructuralPowerConfigNestedSection* Debug = AddDebugSection(Root, Root);

	AddBoolProperty(
		Debug,
		Debug,
		TEXT("Trace"),
		NSLOCTEXT("StructuralPower", "Trace", "PWR trace logging"),
		NSLOCTEXT("StructuralPower", "TraceTip", "Verbose [HALSP] diagnostics in FactoryGame.log. Chat: !tracetoggle"),
		false);

	AddBoolProperty(
		Debug,
		Debug,
		TEXT("EnablePropagation"),
		NSLOCTEXT("StructuralPower", "EnablePropagation", "Enable structural propagation"),
		NSLOCTEXT("StructuralPower", "EnablePropagationTip", "Master toggle for the structural power bus."),
		true);

	AddBoolProperty(
		Debug,
		Debug,
		TEXT("GatePowerSwitches"),
		NSLOCTEXT("StructuralPower", "GatePowerSwitches", "Gate power switches"),
		NSLOCTEXT("StructuralPower", "GatePowerSwitchesTip", "Structural switch gating (v2.1)."),
		true);

	AddBoolProperty(
		Debug,
		Debug,
		TEXT("PowerSwitchManualGroups"),
		NSLOCTEXT("StructuralPower", "PowerSwitchManualGroups", "Switch keyed subnets (Mode B)"),
		NSLOCTEXT(
			"StructuralPower",
			"PowerSwitchManualGroupsTip",
			"ON = keyed subnet mesh; OFF = whole-component gate."),
		true);

	AddBoolProperty(
		Debug,
		Debug,
		TEXT("EnableHoverpackStructural"),
		NSLOCTEXT("StructuralPower", "EnableHoverpackStructural", "Hoverpack structural tether"),
		NSLOCTEXT(
			"StructuralPower",
			"EnableHoverpackStructuralTip",
			"Virtual anchors on powered structure geometry."),
		true);
}

UConfigProperty* UpgradeNestedSectionToSmlBlueprint(
	UConfigPropertySection* Parent,
	const FString& Key,
	UConfigPropertySection* Property)
{
	if (!IsValid(Parent) || !IsValid(Property) || IsSmlBlueprintConfigPropertyClass(Property->GetClass()))
	{
		return Property;
	}

	UClass* SectionClass = ResolveSmlConfigPropertyClassRuntime(
		UCP_Section::StaticClass(),
		TEXT("BP_ConfigPropertySection_C"));
	if (!SectionClass)
	{
		return Property;
	}

	const FName PropertyName = Property->GetFName();
	const TMap<FString, TObjectPtr<UConfigProperty>> ChildProperties = Property->SectionProperties;

	Property->Rename(
		nullptr,
		GetTransientPackage(),
		REN_DontCreateRedirectors | REN_NonTransactional);

	UConfigPropertySection* Upgraded = NewObject<UConfigPropertySection>(Parent, SectionClass, PropertyName);
	Upgraded->SectionProperties = ChildProperties;
	if (UCP_Section* DestSection = Cast<UCP_Section>(Upgraded))
	{
		CopySectionWidgetState(Property, DestSection);
	}
	else
	{
		Upgraded->DisplayName = Property->DisplayName;
		Upgraded->Tooltip = Property->Tooltip;
	}

	Parent->SectionProperties[Key] = Upgraded;
	Property->MarkAsGarbage();
	return Upgraded;
}

UConfigProperty* UpgradePropertyToSmlBlueprint(UConfigPropertySection* Section, const FString& Key, UConfigProperty* Property)
{
	if (!IsValid(Section) || !IsValid(Property) || IsSmlBlueprintConfigPropertyClass(Property->GetClass()))
	{
		return Property;
	}

	UClass* TargetClass = nullptr;
	if (Cast<UConfigPropertyBool>(Property))
	{
		TargetClass = ResolveSmlConfigPropertyClassRuntime(
			UCP_Bool::StaticClass(),
			TEXT("BP_ConfigPropertyBool_C"));
	}
	else if (Cast<UConfigPropertyFloat>(Property))
	{
		TargetClass = ResolveSmlConfigPropertyClassRuntime(
			UCP_Float::StaticClass(),
			TEXT("BP_ConfigPropertyFloat_C"));
	}

	if (!TargetClass)
	{
		return Property;
	}

	const FName PropertyName = Property->GetFName();

	// SML duplicates CDO subobjects by name on RootConfigValue; rename frees the slot for BP class.
	Property->Rename(
		nullptr,
		GetTransientPackage(),
		REN_DontCreateRedirectors | REN_NonTransactional);

	UConfigProperty* Upgraded = nullptr;
	if (UConfigPropertyBool* BoolProperty = Cast<UConfigPropertyBool>(Property))
	{
		UConfigPropertyBool* NewBool = NewObject<UConfigPropertyBool>(Section, TargetClass, PropertyName);
		CopyBoolPropertyState(BoolProperty, NewBool);
		Upgraded = NewBool;
	}
	else if (UConfigPropertyFloat* FloatProperty = Cast<UConfigPropertyFloat>(Property))
	{
		UConfigPropertyFloat* NewFloat = NewObject<UConfigPropertyFloat>(Section, TargetClass, PropertyName);
		CopyFloatPropertyState(FloatProperty, NewFloat);
		Upgraded = NewFloat;
	}

	if (!IsValid(Upgraded))
	{
		return Property;
	}

	Section->SectionProperties[Key] = Upgraded;
	Property->MarkAsGarbage();
	return Upgraded;
}

void UpgradeConfigSectionTree(UConfigPropertySection* Section)
{
	if (!IsValid(Section))
	{
		return;
	}

	TArray<FString> Keys;
	Section->SectionProperties.GetKeys(Keys);
	for (const FString& Key : Keys)
	{
		TObjectPtr<UConfigProperty>* Found = Section->SectionProperties.Find(Key);
		if (!Found || !IsValid(*Found))
		{
			continue;
		}

		if (UConfigPropertySection* Nested = Cast<UConfigPropertySection>(*Found))
		{
			if (!Nested->IsA<UStructuralPowerConfigRootSection>())
			{
				UpgradeConfigSectionTree(Nested);
				UpgradeNestedSectionToSmlBlueprint(Section, Key, Nested);
			}
			continue;
		}

		UpgradePropertyToSmlBlueprint(Section, Key, *Found);
	}
}
}

void StructuralPowerUpgradeConfigPropertiesForSmlWidgets(UConfigPropertySection* Root)
{
	UpgradeConfigSectionTree(Root);
}

UUserWidget* UStructuralPowerConfigRootSection::CreateEditorWidget_Implementation(UUserWidget* ParentWidget) const
{
	UClass* SectionClass = ResolveSmlConfigPropertyClassRuntime(
		UCP_Section::StaticClass(),
		TEXT("BP_ConfigPropertySection_C"));
	if (!SectionClass)
	{
		UE_LOG(LogStructuralPower, Warning,
			TEXT("BP_ConfigPropertySection not loaded — pause-menu widgets unavailable"));
		return nullptr;
	}

	UStructuralPowerConfigRootSection* MutableThis = const_cast<UStructuralPowerConfigRootSection*>(this);
	UObject* Outer = MutableThis->GetOuter();
	UConfigPropertySection* Proxy = NewObject<UConfigPropertySection>(Outer, SectionClass, NAME_None, RF_Transient);
	if (UCP_Section* Section = Cast<UCP_Section>(Proxy))
	{
		Section->WidgetType = ECP_SectionWidgetType::CPS_Vertical;
		Section->HasHeader = false;
	}

	Proxy->SectionProperties = SectionProperties;
	return Proxy->CreateEditorWidget(ParentWidget);
}

UStructuralPowerModConfiguration::UStructuralPowerModConfiguration()
{
	ConfigId.ModReference = FStructuralPowerModConfig::ModReference;
	ConfigId.ConfigCategory = TEXT("");
	DisplayName = NSLOCTEXT("StructuralPower", "ModConfigName", "Structural Power");
	Description = NSLOCTEXT(
		"StructuralPower",
		"ModConfigDescription",
		"Structural power bus, switch gating, and hoverpack structural tether.");

	RootSection = NewObject<UStructuralPowerConfigRootSection>(this, TEXT("RootSection"));
	InitializeSchema(RootSection);
}

void UStructuralPowerConfigLiveSync::HandlePropertyChanged()
{
	if (!IsValid(BoundGameInstance))
	{
		return;
	}

	const FConfigId ConfigId{FStructuralPowerModConfig::ModReference, TEXT("")};
	UConfigManager* ConfigManager = BoundGameInstance->GetSubsystem<UConfigManager>();
	if (!ConfigManager)
	{
		return;
	}

	if (UConfigPropertySection* Root = ConfigManager->GetConfigurationRootSection(ConfigId))
	{
		FStructuralPowerModConfig::ApplyFromConfigRoot(Root);
		ConfigManager->MarkConfigurationDirty(ConfigId);

		if (UWorld* World = BoundGameInstance->GetWorld())
		{
			if (AStructuralPowerGraphSubsystem* Graph = AStructuralPowerGraphSubsystem::Find(World))
			{
				Graph->ReconcileAllLightConsumers();
			}
		}
	}
}
