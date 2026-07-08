// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Config/FStructuralPowerModConfig.h"

#include "Config/UStructuralPowerModConfiguration.h"
#include "Configuration/ConfigManager.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "Configuration/ConfigProperty.h"
#include "Configuration/Properties/ConfigPropertyBool.h"
#include "Configuration/Properties/ConfigPropertyFloat.h"
#include "Configuration/Properties/ConfigPropertySection.h"
#include "Engine/GameInstance.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "StructuralPowerLog.h"

namespace
{
static constexpr float HoverpackMultiplierMin = 1.0f;
static constexpr float HoverpackMultiplierMax = 10.0f;

static TAutoConsoleVariable<int32> CVarStructuralPowerTrace(
	TEXT("StructuralPower.Trace"),
	0,
	TEXT("1 = enable [HALSP] trace logging"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarStructuralPowerGroupLighting(
	TEXT("StructuralPower.GroupLighting"),
	0,
	TEXT("1 = lights draw structural power on powered foundations (v2.2 M3); 0 = wire required"),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarStructuralPowerHoverpackHorizontalMultiplier(
	TEXT("StructuralPower.HoverpackStructuralHorizontalMultiplier"),
	1.5f,
	TEXT("Structural hoverpack horizontal radius multiplier (clamp 1.0-10.0)"),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarStructuralPowerHoverpackVerticalMultiplier(
	TEXT("StructuralPower.HoverpackStructuralVerticalMultiplier"),
	1.5f,
	TEXT("Structural hoverpack vertical radius multiplier (clamp 1.0-10.0)"),
	ECVF_Default);

static float ClampHoverpackMultiplier(float Value)
{
	return FMath::Clamp(Value, HoverpackMultiplierMin, HoverpackMultiplierMax);
}

static bool ParseBoolField(const TSharedPtr<FJsonObject>& Object, const FString& Key, bool DefaultValue)
{
	if (!Object.IsValid() || !Object->HasField(Key))
	{
		return DefaultValue;
	}

	const TSharedPtr<FJsonValue> Value = Object->TryGetField(Key);
	if (!Value.IsValid())
	{
		return DefaultValue;
	}

	if (Value->Type == EJson::Boolean)
	{
		return Value->AsBool();
	}

	if (Value->Type == EJson::Number)
	{
		return Value->AsNumber() != 0.0;
	}

	if (Value->Type == EJson::String)
	{
		const FString Text = Value->AsString().ToLower();
		return Text == TEXT("1") || Text == TEXT("true") || Text == TEXT("on") || Text == TEXT("yes");
	}

	return DefaultValue;
}

static float ParseFloatField(const TSharedPtr<FJsonObject>& Object, const FString& Key, float DefaultValue)
{
	if (!Object.IsValid() || !Object->HasField(Key))
	{
		return DefaultValue;
	}

	double Number = DefaultValue;
	if (Object->TryGetNumberField(Key, Number))
	{
		return static_cast<float>(Number);
	}

	return DefaultValue;
}

static void ApplyCvarsFromLegacyJson(const TSharedPtr<FJsonObject>& Object)
{
	if (!Object.IsValid())
	{
		return;
	}

	CVarStructuralPowerTrace->Set(ParseBoolField(Object, TEXT("Trace"), false) ? 1 : 0);
	CVarStructuralPowerGroupLighting->Set(
		ParseBoolField(Object, TEXT("GroupLighting"), false) ? 1 : 0);

	float Horizontal = ParseFloatField(Object, TEXT("HoverpackStructuralHorizontalMultiplier"), -1.0f);
	float Vertical = ParseFloatField(Object, TEXT("HoverpackStructuralVerticalMultiplier"), -1.0f);
	const float Legacy = ParseFloatField(Object, TEXT("HoverpackStructuralRadiusMultiplier"), 1.5f);
	if (Horizontal < 0.0f)
	{
		Horizontal = Legacy;
	}
	if (Vertical < 0.0f)
	{
		Vertical = Legacy;
	}

	CVarStructuralPowerHoverpackHorizontalMultiplier->Set(ClampHoverpackMultiplier(Horizontal));
	CVarStructuralPowerHoverpackVerticalMultiplier->Set(ClampHoverpackMultiplier(Vertical));
}

static TSharedPtr<FJsonObject> BuildLegacyJsonFromCvars()
{
	TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetBoolField(TEXT("Trace"), CVarStructuralPowerTrace.GetValueOnGameThread() != 0);
	Object->SetBoolField(
		TEXT("GroupLighting"),
		CVarStructuralPowerGroupLighting.GetValueOnGameThread() != 0);
	Object->SetNumberField(
		TEXT("HoverpackStructuralHorizontalMultiplier"),
		ClampHoverpackMultiplier(CVarStructuralPowerHoverpackHorizontalMultiplier.GetValueOnGameThread()));
	Object->SetNumberField(
		TEXT("HoverpackStructuralVerticalMultiplier"),
		ClampHoverpackMultiplier(CVarStructuralPowerHoverpackVerticalMultiplier.GetValueOnGameThread()));
	return Object;
}

static UConfigPropertyBool* FindBoolPropertyInSection(UConfigPropertySection* Section, const FString& Key)
{
	if (!IsValid(Section))
	{
		return nullptr;
	}

	if (TObjectPtr<UConfigProperty>* Found = Section->SectionProperties.Find(Key))
	{
		return Cast<UConfigPropertyBool>(*Found);
	}

	return nullptr;
}

static UConfigPropertyBool* FindBoolProperty(UConfigPropertySection* Root, const FString& Key)
{
	if (UConfigPropertyBool* Prop = FindBoolPropertyInSection(Root, Key))
	{
		return Prop;
	}

	if (TObjectPtr<UConfigProperty>* DebugSection = Root->SectionProperties.Find(TEXT("Debug")))
	{
		if (UConfigPropertySection* Debug = Cast<UConfigPropertySection>(*DebugSection))
		{
			return FindBoolPropertyInSection(Debug, Key);
		}
	}

	return nullptr;
}

static UConfigPropertyFloat* FindFloatProperty(UConfigPropertySection* Root, const FString& Key)
{
	if (!IsValid(Root))
	{
		return nullptr;
	}

	if (TObjectPtr<UConfigProperty>* Found = Root->SectionProperties.Find(Key))
	{
		return Cast<UConfigPropertyFloat>(*Found);
	}

	return nullptr;
}

static void MigrateFlatSmlConfigToNestedDebug(UConfigPropertySection* Root, UGameInstance* GameInstance)
{
	if (!IsValid(Root))
	{
		return;
	}

	TObjectPtr<UConfigProperty>* DebugSection = Root->SectionProperties.Find(TEXT("Debug"));
	if (!DebugSection)
	{
		return;
	}

	UConfigPropertySection* Debug = Cast<UConfigPropertySection>(*DebugSection);
	if (!IsValid(Debug))
	{
		return;
	}

	const FString Path = FStructuralPowerModConfig::GetConfigFilePath();
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *Path))
	{
		return;
	}

	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid())
	{
		return;
	}

	if (!Object->HasField(TEXT("SML_ModVersion_DoNotChange")))
	{
		return;
	}

	bool bMigrated = false;
	const TCHAR* DebugBoolKeys[] = {
		TEXT("Trace"),
	};

	const bool bFlatDebugKeys = !Object->HasField(TEXT("Debug"));
	for (const TCHAR* Key : DebugBoolKeys)
	{
		if (!Object->HasField(Key))
		{
			continue;
		}

		if (!bFlatDebugKeys)
		{
			const TSharedPtr<FJsonObject>* DebugObject = nullptr;
			if (Object->TryGetObjectField(TEXT("Debug"), DebugObject) && DebugObject && DebugObject->IsValid())
			{
				if ((*DebugObject)->HasField(Key))
				{
					continue;
				}
			}
		}

		if (UConfigPropertyBool* Prop = FindBoolPropertyInSection(Debug, Key))
		{
			Prop->Value = ParseBoolField(Object, Key, Prop->DefaultValue);
			bMigrated = true;
		}
	}

	if (!bMigrated || !IsValid(GameInstance))
	{
		return;
	}

	if (UConfigManager* ConfigManager = GameInstance->GetSubsystem<UConfigManager>())
	{
		ConfigManager->MarkConfigurationDirty(FConfigId{FStructuralPowerModConfig::ModReference, TEXT("")});
		UE_LOG(LogStructuralPower, Log, TEXT("Migrated flat SML config keys into Debug section"));
	}
}

static void BindPropertyChangeHandlers(
	UConfigPropertySection* Section,
	UStructuralPowerConfigLiveSync* Sync)
{
	if (!IsValid(Section) || !IsValid(Sync))
	{
		return;
	}

	for (const TPair<FString, TObjectPtr<UConfigProperty>>& Pair : Section->SectionProperties)
	{
		UConfigProperty* Property = Pair.Value;
		if (!IsValid(Property))
		{
			continue;
		}

		if (UConfigPropertySection* Nested = Cast<UConfigPropertySection>(Property))
		{
			BindPropertyChangeHandlers(Nested, Sync);
			continue;
		}

		Property->OnPropertyValueChanged.AddDynamic(Sync, &UStructuralPowerConfigLiveSync::HandlePropertyChanged);
	}
}

static UConfigPropertySection* GetLiveConfigRoot(UGameInstance* GameInstance)
{
	if (!IsValid(GameInstance))
	{
		return nullptr;
	}

	UConfigManager* ConfigManager = GameInstance->GetSubsystem<UConfigManager>();
	if (!ConfigManager)
	{
		return nullptr;
	}

	const FConfigId ConfigId{FStructuralPowerModConfig::ModReference, TEXT("")};
	return ConfigManager->GetConfigurationRootSection(ConfigId);
}
}

void FStructuralPowerModConfig::RegisterConsoleVariables()
{
	CVarStructuralPowerTrace.AsVariable();
	CVarStructuralPowerGroupLighting.AsVariable();
	CVarStructuralPowerHoverpackHorizontalMultiplier.AsVariable();
	CVarStructuralPowerHoverpackVerticalMultiplier.AsVariable();
}

FString FStructuralPowerModConfig::GetConfigFilePath()
{
	return UConfigManager::GetConfigurationFolderPath()
		+ FString::Printf(TEXT("%s.cfg"), ModReference);
}

void FStructuralPowerModConfig::LoadLegacyFromDisk()
{
	const FString Path = GetConfigFilePath();
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *Path))
	{
		return;
	}

	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid())
	{
		UE_LOG(LogStructuralPower, Warning, TEXT("Failed to parse legacy config %s"), *Path);
		return;
	}

	ApplyCvarsFromLegacyJson(Object);
	UE_LOG(LogStructuralPower, Log, TEXT("Loaded legacy CVars from %s (pre-SML migrate)"), *Path);
}

void FStructuralPowerModConfig::SaveLegacyToDisk()
{
	const FString Path = GetConfigFilePath();
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);

	const TSharedPtr<FJsonObject> Object = BuildLegacyJsonFromCvars();
	FString JsonText;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
	if (!FJsonSerializer::Serialize(Object.ToSharedRef(), Writer))
	{
		return;
	}

	FFileHelper::SaveStringToFile(JsonText, *Path);
}

void FStructuralPowerModConfig::ApplyFromConfigRoot(UConfigPropertySection* Root)
{
	if (!IsValid(Root))
	{
		return;
	}

	if (UConfigPropertyBool* Prop = FindBoolProperty(Root, TEXT("Trace")))
	{
		CVarStructuralPowerTrace->Set(Prop->Value ? 1 : 0);
	}
	if (UConfigPropertyBool* Prop = FindBoolProperty(Root, TEXT("GroupLighting")))
	{
		CVarStructuralPowerGroupLighting->Set(Prop->Value ? 1 : 0);
	}
	if (UConfigPropertyFloat* Prop = FindFloatProperty(Root, TEXT("HoverpackStructuralHorizontalMultiplier")))
	{
		CVarStructuralPowerHoverpackHorizontalMultiplier->Set(ClampHoverpackMultiplier(Prop->Value));
	}
	if (UConfigPropertyFloat* Prop = FindFloatProperty(Root, TEXT("HoverpackStructuralVerticalMultiplier")))
	{
		CVarStructuralPowerHoverpackVerticalMultiplier->Set(ClampHoverpackMultiplier(Prop->Value));
	}
}

static void NotifyConfigPropertyChanged(UConfigProperty* Property)
{
	if (!IsValid(Property))
	{
		return;
	}

	Property->MarkDirty();
}

void FStructuralPowerModConfig::PushToConfigRoot(UConfigPropertySection* Root)
{
	if (!IsValid(Root))
	{
		return;
	}

	if (UConfigPropertyBool* Prop = FindBoolProperty(Root, TEXT("Trace")))
	{
		Prop->Value = CVarStructuralPowerTrace.GetValueOnGameThread() != 0;
		NotifyConfigPropertyChanged(Prop);
	}
	if (UConfigPropertyBool* Prop = FindBoolProperty(Root, TEXT("GroupLighting")))
	{
		Prop->Value = CVarStructuralPowerGroupLighting.GetValueOnGameThread() != 0;
		NotifyConfigPropertyChanged(Prop);
	}
	if (UConfigPropertyFloat* Prop = FindFloatProperty(Root, TEXT("HoverpackStructuralHorizontalMultiplier")))
	{
		Prop->Value = ClampHoverpackMultiplier(
			CVarStructuralPowerHoverpackHorizontalMultiplier.GetValueOnGameThread());
		NotifyConfigPropertyChanged(Prop);
	}
	if (UConfigPropertyFloat* Prop = FindFloatProperty(Root, TEXT("HoverpackStructuralVerticalMultiplier")))
	{
		Prop->Value = ClampHoverpackMultiplier(
			CVarStructuralPowerHoverpackVerticalMultiplier.GetValueOnGameThread());
		NotifyConfigPropertyChanged(Prop);
	}
}

void FStructuralPowerModConfig::SyncRuntimeFromConfigManager(UGameInstance* GameInstance)
{
	UConfigPropertySection* Root = GetLiveConfigRoot(GameInstance);
	if (!IsValid(Root))
	{
		LoadLegacyFromDisk();
		return;
	}

	const FString Path = GetConfigFilePath();
	FString JsonText;
	if (FFileHelper::LoadFileToString(JsonText, *Path))
	{
		TSharedPtr<FJsonObject> Object;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
		if (FJsonSerializer::Deserialize(Reader, Object) && Object.IsValid()
			&& !Object->HasField(TEXT("SML_ModVersion_DoNotChange"))
			&& (Object->HasField(TEXT("GroupLighting")) || Object->HasField(TEXT("Trace"))))
		{
			ApplyCvarsFromLegacyJson(Object);
			PushToConfigRoot(Root);
			if (UConfigManager* ConfigManager = GameInstance->GetSubsystem<UConfigManager>())
			{
				ConfigManager->MarkConfigurationDirty(FConfigId{ModReference, TEXT("")});
			}
			UE_LOG(LogStructuralPower, Log, TEXT("Migrated legacy config into SML schema at %s"), *Path);
		}
	}

	StructuralPowerUpgradeConfigPropertiesForSmlWidgets(Root);
	MigrateFlatSmlConfigToNestedDebug(Root, GameInstance);
	ApplyFromConfigRoot(Root);
	BindLiveConfigHandlers(GameInstance);
	UE_LOG(LogStructuralPower, Log, TEXT("Synced runtime CVars from SML mod configuration"));
}

void FStructuralPowerModConfig::BindLiveConfigHandlers(UGameInstance* GameInstance)
{
	if (!IsValid(GameInstance))
	{
		return;
	}

	static TSet<TWeakObjectPtr<UGameInstance>> BoundInstances;
	if (BoundInstances.Contains(GameInstance))
	{
		return;
	}

	UConfigPropertySection* Root = GetLiveConfigRoot(GameInstance);
	if (!IsValid(Root))
	{
		return;
	}

	BoundInstances.Add(GameInstance);

	UStructuralPowerConfigLiveSync* Sync = NewObject<UStructuralPowerConfigLiveSync>(GameInstance);
	Sync->BoundGameInstance = GameInstance;
	BindPropertyChangeHandlers(Root, Sync);
}

bool FStructuralPowerModConfig::TryApplySetCommand(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 2)
	{
		UE_LOG(LogStructuralPower, Warning, TEXT("Usage: StructuralPower.Set <key> <value>"));
		return false;
	}

	if (IsValid(World) && World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogStructuralPower, Warning, TEXT("StructuralPower.Set ignored on client — authority only"));
		return false;
	}

	const FString Key = Args[0];
	const FString ValueText = Args[1];

	auto SetBool = [&](TAutoConsoleVariable<int32>& CVar, const FString& Name) -> bool
	{
		if (!Key.Equals(Name, ESearchCase::IgnoreCase))
		{
			return false;
		}

		const bool bOn = ValueText.Equals(TEXT("1"), ESearchCase::IgnoreCase)
			|| ValueText.Equals(TEXT("true"), ESearchCase::IgnoreCase)
			|| ValueText.Equals(TEXT("on"), ESearchCase::IgnoreCase);
		CVar->Set(bOn ? 1 : 0);
		return true;
	};

	bool bChanged = false;
	if (SetBool(CVarStructuralPowerTrace, TEXT("Trace"))
		|| SetBool(CVarStructuralPowerGroupLighting, TEXT("GroupLighting")))
	{
		bChanged = true;
	}
	else if (Key.Equals(TEXT("HoverpackStructuralRadiusMultiplier"), ESearchCase::IgnoreCase))
	{
		const float Value = ClampHoverpackMultiplier(FCString::Atof(*ValueText));
		CVarStructuralPowerHoverpackHorizontalMultiplier->Set(Value);
		CVarStructuralPowerHoverpackVerticalMultiplier->Set(Value);
		bChanged = true;
	}
	else if (Key.Equals(TEXT("HoverpackStructuralHorizontalMultiplier"), ESearchCase::IgnoreCase))
	{
		CVarStructuralPowerHoverpackHorizontalMultiplier->Set(
			ClampHoverpackMultiplier(FCString::Atof(*ValueText)));
		bChanged = true;
	}
	else if (Key.Equals(TEXT("HoverpackStructuralVerticalMultiplier"), ESearchCase::IgnoreCase))
	{
		CVarStructuralPowerHoverpackVerticalMultiplier->Set(
			ClampHoverpackMultiplier(FCString::Atof(*ValueText)));
		bChanged = true;
	}
	else
	{
		UE_LOG(LogStructuralPower, Warning, TEXT("Unknown config key: %s"), *Key);
		return false;
	}

	if (UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr)
	{
		if (UConfigPropertySection* Root = GetLiveConfigRoot(GameInstance))
		{
			PushToConfigRoot(Root);
			if (UConfigManager* ConfigManager = GameInstance->GetSubsystem<UConfigManager>())
			{
				ConfigManager->MarkConfigurationDirty(FConfigId{ModReference, TEXT("")});
			}
		}
		else
		{
			SaveLegacyToDisk();
		}
	}
	else
	{
		SaveLegacyToDisk();
	}

	if (Key.Equals(TEXT("GroupLighting"), ESearchCase::IgnoreCase)
		&& IsValid(World))
	{
		if (AStructuralPowerGraphSubsystem* Graph = AStructuralPowerGraphSubsystem::Find(World))
		{
			Graph->ReconcileAllLightConsumers();
		}
	}

	return bChanged;
}

bool FStructuralPowerModConfig::IsGroupLightingEnabled()
{
	return CVarStructuralPowerGroupLighting.GetValueOnGameThread() != 0;
}

float FStructuralPowerModConfig::GetHoverpackHorizontalMultiplier()
{
	return ClampHoverpackMultiplier(CVarStructuralPowerHoverpackHorizontalMultiplier.GetValueOnGameThread());
}

float FStructuralPowerModConfig::GetHoverpackVerticalMultiplier()
{
	return ClampHoverpackMultiplier(CVarStructuralPowerHoverpackVerticalMultiplier.GetValueOnGameThread());
}

bool FStructuralPowerModConfig::IsTraceEnabled()
{
	return CVarStructuralPowerTrace.GetValueOnGameThread() != 0;
}
