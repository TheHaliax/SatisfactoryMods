// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Config/FStructuralPowerModConfig.h"

#include "Configuration/ConfigManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "StructuralPowerLog.h"

namespace {
static constexpr float HoverpackMultiplierMin = 1.0f;
static constexpr float HoverpackMultiplierMax = 10.0f;

static TAutoConsoleVariable<int32> CVarStructuralPowerTrace(
    TEXT("StructuralPower.Trace"), 0, TEXT("1 = enable [HALSP] trace logging (debug)"),
    ECVF_Default);

static TAutoConsoleVariable<int32> CVarStructuralPowerExtendedDebug(
    TEXT("StructuralPower.ExtendedDebug"), 0,
    TEXT("1 = vanilla power/circuit hooks + fat [HALSP] logs (debug)"), ECVF_Default);

static TAutoConsoleVariable<int32> CVarStructuralPowerGroupLighting(
    TEXT("StructuralPower.GroupLighting"), 0,
    TEXT("1 = lights draw structural power on powered foundations; 0 = wire required"),
    ECVF_Default);

static TAutoConsoleVariable<float> CVarStructuralPowerHoverpackHorizontalMultiplier(
    TEXT("StructuralPower.HoverpackStructuralHorizontalMultiplier"), 1.2f,
    TEXT("Structural hoverpack horizontal radius multiplier (clamp 1.0-10.0)"), ECVF_Default);

static TAutoConsoleVariable<float> CVarStructuralPowerHoverpackVerticalMultiplier(
    TEXT("StructuralPower.HoverpackStructuralVerticalMultiplier"), 1.2f,
    TEXT("Structural hoverpack vertical radius multiplier (clamp 1.0-10.0)"), ECVF_Default);

static bool IsGroupEnabled(TAutoConsoleVariable<int32>& CVar) {
  return CVar.GetValueOnGameThread() != 0;
}

static TAutoConsoleVariable<int32> CVarStructuralPowerGroupGeneration(
    TEXT("StructuralPower.GroupGeneration"), 0,
    TEXT("1 = generators and storage draw structural power on structure"), ECVF_Default);

static TAutoConsoleVariable<int32> CVarStructuralPowerGroupResources(
    TEXT("StructuralPower.GroupResources"), 0,
    TEXT("1 = resource extractors draw structural power on structure"), ECVF_Default);

static TAutoConsoleVariable<int32> CVarStructuralPowerGroupProduction(
    TEXT("StructuralPower.GroupProduction"), 0,
    TEXT("1 = production buildings draw structural power on structure"), ECVF_Default);

static TAutoConsoleVariable<int32> CVarStructuralPowerGroupTransport(
    TEXT("StructuralPower.GroupTransport"), 0,
    TEXT("1 = transport buildings draw structural power on structure"), ECVF_Default);

static TAutoConsoleVariable<int32> CVarStructuralPowerGroupPipes(
    TEXT("StructuralPower.GroupPipes"), 0,
    TEXT("1 = pipe pumps draw structural power on structure"), ECVF_Default);

static TAutoConsoleVariable<int32> CVarStructuralPowerGroupBelts(
    TEXT("StructuralPower.GroupBelts"), 0, TEXT("1 = belt category toggle placeholder"),
    ECVF_Default);

static float ClampHoverpackMultiplier(float Value) {
  return FMath::Clamp(Value, HoverpackMultiplierMin, HoverpackMultiplierMax);
}

static bool ParseBoolField(const TSharedPtr<FJsonObject>& Object, const FString& Key,
                           bool DefaultValue) {
  if (!Object.IsValid() || !Object->HasField(Key)) {
    return DefaultValue;
  }

  const TSharedPtr<FJsonValue> Value = Object->TryGetField(Key);
  if (!Value.IsValid()) {
    return DefaultValue;
  }

  if (Value->Type == EJson::Boolean) {
    return Value->AsBool();
  }

  if (Value->Type == EJson::Number) {
    return Value->AsNumber() != 0.0;
  }

  if (Value->Type == EJson::String) {
    const FString Text = Value->AsString().ToLower();
    return Text == TEXT("1") || Text == TEXT("true") || Text == TEXT("on") || Text == TEXT("yes");
  }

  return DefaultValue;
}

static float ParseFloatField(const TSharedPtr<FJsonObject>& Object, const FString& Key,
                             float DefaultValue) {
  if (!Object.IsValid() || !Object->HasField(Key)) {
    return DefaultValue;
  }

  double Number = DefaultValue;
  if (Object->TryGetNumberField(Key, Number)) {
    return static_cast<float>(Number);
  }

  return DefaultValue;
}

static void ApplyCvarsFromJson(const TSharedPtr<FJsonObject>& Object) {
  if (!Object.IsValid()) {
    return;
  }

  CVarStructuralPowerTrace->Set(ParseBoolField(Object, TEXT("Trace"), false) ? 1 : 0);
  CVarStructuralPowerExtendedDebug->Set(ParseBoolField(Object, TEXT("ExtendedDebug"), false) ? 1
                                                                                             : 0);
  CVarStructuralPowerGroupLighting->Set(ParseBoolField(Object, TEXT("GroupLighting"), false) ? 1
                                                                                             : 0);
  CVarStructuralPowerGroupGeneration->Set(
      ParseBoolField(Object, TEXT("GroupGeneration"), false) ? 1 : 0);
  CVarStructuralPowerGroupResources->Set(ParseBoolField(Object, TEXT("GroupResources"), false) ? 1
                                                                                               : 0);
  CVarStructuralPowerGroupProduction->Set(
      ParseBoolField(Object, TEXT("GroupProduction"), false) ? 1 : 0);
  CVarStructuralPowerGroupTransport->Set(ParseBoolField(Object, TEXT("GroupTransport"), false) ? 1
                                                                                               : 0);
  CVarStructuralPowerGroupPipes->Set(ParseBoolField(Object, TEXT("GroupPipes"), false) ? 1 : 0);
  CVarStructuralPowerGroupBelts->Set(ParseBoolField(Object, TEXT("GroupBelts"), false) ? 1 : 0);

  float Horizontal =
      ParseFloatField(Object, TEXT("HoverpackStructuralHorizontalMultiplier"), -1.0f);
  float Vertical = ParseFloatField(Object, TEXT("HoverpackStructuralVerticalMultiplier"), -1.0f);
  const float Legacy = ParseFloatField(Object, TEXT("HoverpackStructuralRadiusMultiplier"), 1.2f);
  if (Horizontal < 0.0f) {
    Horizontal = Legacy;
  }
  if (Vertical < 0.0f) {
    Vertical = Legacy;
  }

  CVarStructuralPowerHoverpackHorizontalMultiplier->Set(ClampHoverpackMultiplier(Horizontal));
  CVarStructuralPowerHoverpackVerticalMultiplier->Set(ClampHoverpackMultiplier(Vertical));
}

static TSharedPtr<FJsonObject> BuildJsonFromCvars() {
  TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
  Object->SetBoolField(TEXT("Trace"), CVarStructuralPowerTrace.GetValueOnGameThread() != 0);
  Object->SetBoolField(TEXT("ExtendedDebug"),
                       CVarStructuralPowerExtendedDebug.GetValueOnGameThread() != 0);
  Object->SetBoolField(TEXT("GroupLighting"),
                       CVarStructuralPowerGroupLighting.GetValueOnGameThread() != 0);
  Object->SetBoolField(TEXT("GroupGeneration"),
                       CVarStructuralPowerGroupGeneration.GetValueOnGameThread() != 0);
  Object->SetBoolField(TEXT("GroupResources"),
                       CVarStructuralPowerGroupResources.GetValueOnGameThread() != 0);
  Object->SetBoolField(TEXT("GroupProduction"),
                       CVarStructuralPowerGroupProduction.GetValueOnGameThread() != 0);
  Object->SetBoolField(TEXT("GroupTransport"),
                       CVarStructuralPowerGroupTransport.GetValueOnGameThread() != 0);
  Object->SetBoolField(TEXT("GroupPipes"),
                       CVarStructuralPowerGroupPipes.GetValueOnGameThread() != 0);
  Object->SetBoolField(TEXT("GroupBelts"),
                       CVarStructuralPowerGroupBelts.GetValueOnGameThread() != 0);
  Object->SetNumberField(
      TEXT("HoverpackStructuralHorizontalMultiplier"),
      ClampHoverpackMultiplier(
          CVarStructuralPowerHoverpackHorizontalMultiplier.GetValueOnGameThread()));
  Object->SetNumberField(
      TEXT("HoverpackStructuralVerticalMultiplier"),
      ClampHoverpackMultiplier(
          CVarStructuralPowerHoverpackVerticalMultiplier.GetValueOnGameThread()));
  return Object;
}
}  // namespace

void FStructuralPowerModConfig::RegisterConsoleVariables() {
  CVarStructuralPowerTrace.AsVariable();
  CVarStructuralPowerExtendedDebug.AsVariable();
  CVarStructuralPowerGroupLighting.AsVariable();
  CVarStructuralPowerGroupGeneration.AsVariable();
  CVarStructuralPowerGroupResources.AsVariable();
  CVarStructuralPowerGroupProduction.AsVariable();
  CVarStructuralPowerGroupTransport.AsVariable();
  CVarStructuralPowerGroupPipes.AsVariable();
  CVarStructuralPowerGroupBelts.AsVariable();
  CVarStructuralPowerHoverpackHorizontalMultiplier.AsVariable();
  CVarStructuralPowerHoverpackVerticalMultiplier.AsVariable();
}

FString FStructuralPowerModConfig::GetConfigFilePath() {
  return UConfigManager::GetConfigurationFolderPath() +
         FString::Printf(TEXT("%s.cfg"), ModReference);
}

void FStructuralPowerModConfig::LoadRuntimeConfig() {
  const FString Path = GetConfigFilePath();
  FString JsonText;
  if (!FFileHelper::LoadFileToString(JsonText, *Path)) {
    return;
  }

  TSharedPtr<FJsonObject> Object;
  const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
  if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid()) {
    UE_LOG(LogStructuralPower, Warning, TEXT("Failed to parse config %s"), *Path);
    return;
  }

  ApplyCvarsFromJson(Object);
  UE_LOG(LogStructuralPower, Log, TEXT("Loaded StructuralPower config from %s"), *Path);
}

void FStructuralPowerModConfig::SaveLegacyToDisk() {
  if (IsEngineExitRequested()) {
    return;
  }

  const FString Path = GetConfigFilePath();
  IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);

  const TSharedPtr<FJsonObject> Object = BuildJsonFromCvars();
  FString JsonText;
  const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
  if (!FJsonSerializer::Serialize(Object.ToSharedRef(), Writer)) {
    return;
  }

  FFileHelper::SaveStringToFile(JsonText, *Path);
}

bool FStructuralPowerModConfig::CanMutateLiveConfig(UWorld* World) {
  if (IsEngineExitRequested() || !IsValid(World)) {
    return false;
  }

  if (World->bIsTearingDown) {
    return false;
  }

  return World->GetNetMode() != NM_Client;
}

void FStructuralPowerModConfig::RequestGroupLightingReconcile(UWorld* World) {
  if (!CanMutateLiveConfig(World)) {
    return;
  }

  if (AStructuralPowerGraphSubsystem* Graph = AStructuralPowerGraphSubsystem::Find(World)) {
    Graph->ReconcileGroupLightingState();
  }
}

void FStructuralPowerModConfig::RequestGroupGenerationReconcile(UWorld* World) {
  if (!CanMutateLiveConfig(World)) {
    return;
  }

  if (AStructuralPowerGraphSubsystem* Graph = AStructuralPowerGraphSubsystem::Find(World)) {
    Graph->ReconcileGroupGenerationState();
  }
}

void FStructuralPowerModConfig::RequestGroupResourcesReconcile(UWorld* World) {
  if (!CanMutateLiveConfig(World)) {
    return;
  }

  if (AStructuralPowerGraphSubsystem* Graph = AStructuralPowerGraphSubsystem::Find(World)) {
    Graph->ReconcileGroupResourcesState();
  }
}

void FStructuralPowerModConfig::RequestGroupProductionReconcile(UWorld* World) {
  if (!CanMutateLiveConfig(World)) {
    return;
  }

  if (AStructuralPowerGraphSubsystem* Graph = AStructuralPowerGraphSubsystem::Find(World)) {
    Graph->ReconcileGroupProductionState();
  }
}

void FStructuralPowerModConfig::RequestGroupTransportReconcile(UWorld* World) {
  if (!CanMutateLiveConfig(World)) {
    return;
  }

  if (AStructuralPowerGraphSubsystem* Graph = AStructuralPowerGraphSubsystem::Find(World)) {
    Graph->ReconcileGroupTransportState();
  }
}

void FStructuralPowerModConfig::RequestGroupPipesReconcile(UWorld* World) {
  if (!CanMutateLiveConfig(World)) {
    return;
  }

  if (AStructuralPowerGraphSubsystem* Graph = AStructuralPowerGraphSubsystem::Find(World)) {
    Graph->ReconcileGroupPipesState();
  }
}

void FStructuralPowerModConfig::RequestGroupBeltsReconcile(UWorld* /*World*/) {
  // Belts group: toggle persistence only — no attach reconcile.
}

bool FStructuralPowerModConfig::TryApplySetCommand(const TArray<FString>& Args, UWorld* World) {
  if (Args.Num() < 2) {
    UE_LOG(LogStructuralPower, Warning, TEXT("Usage: StructuralPower.Set <key> <value>"));
    return false;
  }

  if (IsValid(World) && World->GetNetMode() == NM_Client) {
    UE_LOG(LogStructuralPower, Warning,
           TEXT("StructuralPower.Set ignored on client — authority only"));
    return false;
  }

  if (IsEngineExitRequested()) {
    return false;
  }

  const FString Key = Args[0];
  const FString ValueText = Args[1];

  auto SetBool = [&](TAutoConsoleVariable<int32>& CVar, const FString& Name) -> bool {
    if (!Key.Equals(Name, ESearchCase::IgnoreCase)) {
      return false;
    }

    const bool bOn = ValueText.Equals(TEXT("1"), ESearchCase::IgnoreCase) ||
                     ValueText.Equals(TEXT("true"), ESearchCase::IgnoreCase) ||
                     ValueText.Equals(TEXT("on"), ESearchCase::IgnoreCase);
    CVar->Set(bOn ? 1 : 0);
    return true;
  };

  bool bChanged = false;
  if (SetBool(CVarStructuralPowerTrace, TEXT("Trace")) ||
      SetBool(CVarStructuralPowerExtendedDebug, TEXT("ExtendedDebug")) ||
      SetBool(CVarStructuralPowerGroupLighting, TEXT("GroupLighting")) ||
      SetBool(CVarStructuralPowerGroupGeneration, TEXT("GroupGeneration")) ||
      SetBool(CVarStructuralPowerGroupResources, TEXT("GroupResources")) ||
      SetBool(CVarStructuralPowerGroupProduction, TEXT("GroupProduction")) ||
      SetBool(CVarStructuralPowerGroupTransport, TEXT("GroupTransport")) ||
      SetBool(CVarStructuralPowerGroupPipes, TEXT("GroupPipes")) ||
      SetBool(CVarStructuralPowerGroupBelts, TEXT("GroupBelts"))) {
    bChanged = true;
  } else if (Key.Equals(TEXT("HoverpackStructuralRadiusMultiplier"), ESearchCase::IgnoreCase)) {
    const float Value = ClampHoverpackMultiplier(FCString::Atof(*ValueText));
    CVarStructuralPowerHoverpackHorizontalMultiplier->Set(Value);
    CVarStructuralPowerHoverpackVerticalMultiplier->Set(Value);
    bChanged = true;
  } else if (Key.Equals(TEXT("HoverpackStructuralHorizontalMultiplier"), ESearchCase::IgnoreCase)) {
    CVarStructuralPowerHoverpackHorizontalMultiplier->Set(
        ClampHoverpackMultiplier(FCString::Atof(*ValueText)));
    bChanged = true;
  } else if (Key.Equals(TEXT("HoverpackStructuralVerticalMultiplier"), ESearchCase::IgnoreCase)) {
    CVarStructuralPowerHoverpackVerticalMultiplier->Set(
        ClampHoverpackMultiplier(FCString::Atof(*ValueText)));
    bChanged = true;
  } else {
    UE_LOG(LogStructuralPower, Warning, TEXT("Unknown config key: %s"), *Key);
    return false;
  }

  SaveLegacyToDisk();

  if (Key.Equals(TEXT("GroupLighting"), ESearchCase::IgnoreCase)) {
    RequestGroupLightingReconcile(World);
  } else if (Key.Equals(TEXT("GroupGeneration"), ESearchCase::IgnoreCase)) {
    RequestGroupGenerationReconcile(World);
  } else if (Key.Equals(TEXT("GroupResources"), ESearchCase::IgnoreCase)) {
    RequestGroupResourcesReconcile(World);
  } else if (Key.Equals(TEXT("GroupProduction"), ESearchCase::IgnoreCase)) {
    RequestGroupProductionReconcile(World);
  } else if (Key.Equals(TEXT("GroupTransport"), ESearchCase::IgnoreCase)) {
    RequestGroupTransportReconcile(World);
  } else if (Key.Equals(TEXT("GroupPipes"), ESearchCase::IgnoreCase)) {
    RequestGroupPipesReconcile(World);
  } else if (Key.Equals(TEXT("GroupBelts"), ESearchCase::IgnoreCase)) {
    RequestGroupBeltsReconcile(World);
  }

  return bChanged;
}

bool FStructuralPowerModConfig::IsGroupLightingEnabled() {
  return CVarStructuralPowerGroupLighting.GetValueOnGameThread() != 0;
}

bool FStructuralPowerModConfig::IsGroupGenerationEnabled() {
  return CVarStructuralPowerGroupGeneration.GetValueOnGameThread() != 0;
}

bool FStructuralPowerModConfig::IsGroupResourcesEnabled() {
  return CVarStructuralPowerGroupResources.GetValueOnGameThread() != 0;
}

bool FStructuralPowerModConfig::IsGroupProductionEnabled() {
  return CVarStructuralPowerGroupProduction.GetValueOnGameThread() != 0;
}

bool FStructuralPowerModConfig::IsGroupTransportEnabled() {
  return CVarStructuralPowerGroupTransport.GetValueOnGameThread() != 0;
}

bool FStructuralPowerModConfig::IsGroupPipesEnabled() {
  return CVarStructuralPowerGroupPipes.GetValueOnGameThread() != 0;
}

bool FStructuralPowerModConfig::IsGroupBeltsEnabled() {
  return CVarStructuralPowerGroupBelts.GetValueOnGameThread() != 0;
}

float FStructuralPowerModConfig::GetHoverpackHorizontalMultiplier() {
  return ClampHoverpackMultiplier(
      CVarStructuralPowerHoverpackHorizontalMultiplier.GetValueOnGameThread());
}

float FStructuralPowerModConfig::GetHoverpackVerticalMultiplier() {
  return ClampHoverpackMultiplier(
      CVarStructuralPowerHoverpackVerticalMultiplier.GetValueOnGameThread());
}

bool FStructuralPowerModConfig::IsTraceEnabled() {
  return CVarStructuralPowerTrace.GetValueOnGameThread() != 0;
}

bool FStructuralPowerModConfig::IsExtendedDebugEnabled() {
  return CVarStructuralPowerExtendedDebug.GetValueOnGameThread() != 0;
}
