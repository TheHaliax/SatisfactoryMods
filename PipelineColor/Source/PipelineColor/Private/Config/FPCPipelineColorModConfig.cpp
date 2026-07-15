// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Config/FPCPipelineColorModConfig.h"

#include "Configuration/ConfigManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PipelineColorLog.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
static TAutoConsoleVariable<int32> CVarDefaultGasMetallic(
	TEXT("PipelineColor.DefaultGasMetallic"),
	1,
	TEXT("1 = gases metallic by default when no per-key override"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarDefaultLiquidMetallic(
	TEXT("PipelineColor.DefaultLiquidMetallic"),
	0,
	TEXT("1 = liquids metallic by default when no per-key override"),
	ECVF_Default);

static TMap<FName, bool> GMetallicOverrides;
static FPCPipelineColorConfigChanged GConfigChanged;

static bool ParseBoolText(const FString& ValueText)
{
	return ValueText.Equals(TEXT("1"), ESearchCase::IgnoreCase)
		|| ValueText.Equals(TEXT("true"), ESearchCase::IgnoreCase)
		|| ValueText.Equals(TEXT("on"), ESearchCase::IgnoreCase);
}

static bool ParseBoolField(
	const TSharedPtr<FJsonObject>& Object,
	const FString& Key,
	bool DefaultValue)
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
		return ParseBoolText(Value->AsString());
	}
	return DefaultValue;
}

static void BroadcastConfigChanged()
{
	GConfigChanged.Broadcast();
}

static void ApplyCvarsFromJson(const TSharedPtr<FJsonObject>& Object)
{
	CVarDefaultGasMetallic->Set(
		ParseBoolField(Object, TEXT("DefaultGasMetallic"), true) ? 1 : 0);
	CVarDefaultLiquidMetallic->Set(
		ParseBoolField(Object, TEXT("DefaultLiquidMetallic"), false) ? 1 : 0);

	GMetallicOverrides.Reset();
	const TSharedPtr<FJsonObject>* Overrides = nullptr;
	if (Object->TryGetObjectField(TEXT("MetallicOverrides"), Overrides) && Overrides)
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Overrides)->Values)
		{
			if (Pair.Value.IsValid())
			{
				GMetallicOverrides.Add(FName(*Pair.Key), Pair.Value->AsBool());
			}
		}
	}
}

static TSharedRef<FJsonObject> BuildJsonFromCvars()
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetBoolField(
		TEXT("DefaultGasMetallic"),
		CVarDefaultGasMetallic.GetValueOnGameThread() != 0);
	Root->SetBoolField(
		TEXT("DefaultLiquidMetallic"),
		CVarDefaultLiquidMetallic.GetValueOnGameThread() != 0);

	TSharedRef<FJsonObject> Overrides = MakeShared<FJsonObject>();
	for (const TPair<FName, bool>& Pair : GMetallicOverrides)
	{
		Overrides->SetBoolField(Pair.Key.ToString(), Pair.Value);
	}
	Root->SetObjectField(TEXT("MetallicOverrides"), Overrides);
	return Root;
}
} // namespace

void FPCPipelineColorModConfig::RegisterConsoleVariables()
{
	CVarDefaultGasMetallic.AsVariable();
	CVarDefaultLiquidMetallic.AsVariable();
}

FString FPCPipelineColorModConfig::GetConfigFilePath()
{
	return UConfigManager::GetConfigurationFolderPath()
		+ FString::Printf(TEXT("%s.cfg"), ModReference);
}

void FPCPipelineColorModConfig::ApplyDefaultFlags(bool bGasMetallic, bool bLiquidMetallic)
{
	const int32 GasWant = bGasMetallic ? 1 : 0;
	const int32 LiqWant = bLiquidMetallic ? 1 : 0;
	const bool bChanged =
		CVarDefaultGasMetallic.GetValueOnGameThread() != GasWant
		|| CVarDefaultLiquidMetallic.GetValueOnGameThread() != LiqWant;
	CVarDefaultGasMetallic->Set(GasWant);
	CVarDefaultLiquidMetallic->Set(LiqWant);
	if (bChanged)
	{
		SaveToDisk();
		BroadcastConfigChanged();
	}
}

void FPCPipelineColorModConfig::LoadRuntimeConfig()
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
		UE_LOG(LogPipelineColor, Warning, TEXT("%s bad JSON %s"),
			PIPELINECOLOR_LOG_PREFIX, *Path);
		return;
	}

	ApplyCvarsFromJson(Object);
	UE_LOG(LogPipelineColor, Log, TEXT("%s loaded config from %s (%d overrides)"),
		PIPELINECOLOR_LOG_PREFIX, *Path, GMetallicOverrides.Num());
}

void FPCPipelineColorModConfig::SaveToDisk()
{
	if (IsEngineExitRequested())
	{
		return;
	}

	const FString Path = GetConfigFilePath();
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);

	FString JsonText;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
	if (!FJsonSerializer::Serialize(BuildJsonFromCvars(), Writer))
	{
		return;
	}

	FFileHelper::SaveStringToFile(JsonText, *Path);
}

bool FPCPipelineColorModConfig::CanMutateLiveConfig(UWorld* World)
{
	if (IsEngineExitRequested() || !IsValid(World) || World->bIsTearingDown)
	{
		return false;
	}
	return World->GetNetMode() != NM_Client;
}

bool FPCPipelineColorModConfig::IsGasCatalogKey(FName CatalogKey)
{
	return CatalogKey == FName(TEXT("DarkMatterResidue"))
		|| CatalogKey == FName(TEXT("ExcitedPhotonicMatter"))
		|| CatalogKey == FName(TEXT("IonizedFuel"))
		|| CatalogKey == FName(TEXT("RocketFuel"))
		|| CatalogKey == FName(TEXT("NitrogenGas"));
}

bool FPCPipelineColorModConfig::IsDefaultGasMetallic()
{
	return CVarDefaultGasMetallic.GetValueOnGameThread() != 0;
}

bool FPCPipelineColorModConfig::IsDefaultLiquidMetallic()
{
	return CVarDefaultLiquidMetallic.GetValueOnGameThread() != 0;
}

bool FPCPipelineColorModConfig::IsMetallicForKey(FName CatalogKey)
{
	if (const bool* Override = GMetallicOverrides.Find(CatalogKey))
	{
		return *Override;
	}
	if (IsGasCatalogKey(CatalogKey))
	{
		return IsDefaultGasMetallic();
	}
	return IsDefaultLiquidMetallic();
}

bool FPCPipelineColorModConfig::TrySetMetallicOverride(FName CatalogKey, bool bOn, UWorld* World)
{
	if (CatalogKey.IsNone() || !CanMutateLiveConfig(World))
	{
		return false;
	}

	GMetallicOverrides.Add(CatalogKey, bOn);
	SaveToDisk();
	BroadcastConfigChanged();
	return true;
}

bool FPCPipelineColorModConfig::TryApplySetCommand(const TArray<FString>& Args, UWorld* World)
{
	if (Args.Num() < 2)
	{
		UE_LOG(LogPipelineColor, Warning,
			TEXT("%s Usage: PipelineColor.Set <key> <value>"), PIPELINECOLOR_LOG_PREFIX);
		return false;
	}

	if (IsValid(World) && World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogPipelineColor, Warning,
			TEXT("%s PipelineColor.Set ignored on client"), PIPELINECOLOR_LOG_PREFIX);
		return false;
	}

	if (IsEngineExitRequested())
	{
		return false;
	}

	const FString Key = Args[0];
	const FString ValueText = Args[1];
	bool bChanged = false;

	if (Key.Equals(TEXT("DefaultGasMetallic"), ESearchCase::IgnoreCase))
	{
		CVarDefaultGasMetallic->Set(ParseBoolText(ValueText) ? 1 : 0);
		bChanged = true;
	}
	else if (Key.Equals(TEXT("DefaultLiquidMetallic"), ESearchCase::IgnoreCase))
	{
		CVarDefaultLiquidMetallic->Set(ParseBoolText(ValueText) ? 1 : 0);
		bChanged = true;
	}
	else if (Key.StartsWith(TEXT("Metallic."), ESearchCase::IgnoreCase))
	{
		const FName FluidKey(*Key.Mid(9));
		if (!FluidKey.IsNone())
		{
			GMetallicOverrides.Add(FluidKey, ParseBoolText(ValueText));
			bChanged = true;
		}
	}
	else
	{
		UE_LOG(LogPipelineColor, Warning, TEXT("%s unknown config key: %s"),
			PIPELINECOLOR_LOG_PREFIX, *Key);
		return false;
	}

	if (bChanged)
	{
		SaveToDisk();
		BroadcastConfigChanged();
	}
	return bChanged;
}

FPCPipelineColorConfigChanged& FPCPipelineColorModConfig::OnConfigChanged()
{
	return GConfigChanged;
}
