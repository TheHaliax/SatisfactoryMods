// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Store/FPCSwatchSlotDispatch.h"

#include "Equipment/FGBuildGun.h"
#include "Equipment/FGBuildGunPaint.h"
#include "FGBlueprintFunctionLibrary.h"
#include "FGCustomizationRecipe.h"
#include "FGFactoryColoringTypes.h"
#include "FGGameState.h"
#include "FGPlayerController.h"
#include "Network/UPCChatRCO.h"
#include "Patching/NativeHookManager.h"
#include "PipelineColorLog.h"
#include "Store/APCSwatchStoreSubsystem.h"

static TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> GActivePcDesc;

namespace
{
bool GHooksRegistered = false;

APCSwatchStoreSubsystem* StoreFor(UObject* WorldContext)
{
	UWorld* World = IsValid(WorldContext) ? WorldContext->GetWorld() : nullptr;
	return APCSwatchStoreSubsystem::Find(World);
}

UPCChatRCO* LocalRco(UObject* WorldContext)
{
	UWorld* World = IsValid(WorldContext) ? WorldContext->GetWorld() : nullptr;
	if (!IsValid(World))
	{
		return nullptr;
	}

	AFGPlayerController* PC = Cast<AFGPlayerController>(World->GetFirstPlayerController());
	if (!IsValid(PC) || !PC->IsLocalController())
	{
		return nullptr;
	}
	return PC->GetRemoteCallObjectOfClass<UPCChatRCO>();
}

void SyncActivePcToServer(UObject* WorldContext, TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch)
{
	UWorld* World = IsValid(WorldContext) ? WorldContext->GetWorld() : nullptr;
	if (!IsValid(World) || World->GetNetMode() != NM_Client)
	{
		return;
	}

	if (UPCChatRCO* Rco = LocalRco(WorldContext))
	{
		Rco->Server_SetActivePcSwatch(Swatch);
	}
}

void SubmitPcColors(
	UObject* WorldContext,
	TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch,
	const FFactoryCustomizationColorSlot& ColorData)
{
	if (!APCSwatchStoreSubsystem::IsPCCustomization(Swatch))
	{
		return;
	}

	UWorld* World = IsValid(WorldContext) ? WorldContext->GetWorld() : nullptr;
	if (!IsValid(World))
	{
		return;
	}

	if (UPCChatRCO* Rco = LocalRco(WorldContext))
	{
		Rco->Server_SetPcSwatchColors(Swatch, ColorData);
		return;
	}

	if (World->GetNetMode() != NM_Client)
	{
		APCSwatchStoreSubsystem* Store = APCSwatchStoreSubsystem::GetOrCreate(World);
		if (Store && Store->HasAuthority())
		{
			const FName Key = APCSwatchStoreSubsystem::KeyFromSwatchStatic(Swatch);
			if (!Key.IsNone())
			{
				Store->SetFromSlot(Key, ColorData);
			}
		}
	}
}

void RememberIfPc(TSubclassOf<UFGCustomizationRecipe> Recipe, UObject* WorldContext)
{
	if (!Recipe)
	{
		GActivePcDesc = nullptr;
		SyncActivePcToServer(WorldContext, nullptr);
		return;
	}
	const TSubclassOf<UFGFactoryCustomizationDescriptor> Desc =
		UFGCustomizationRecipe::GetCustomizationDescriptor(Recipe);
	TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch(Desc.Get());
	if (APCSwatchStoreSubsystem::IsPCCustomization(Swatch))
	{
		GActivePcDesc = Swatch;
		SyncActivePcToServer(WorldContext, Swatch);
	}
	else
	{
		GActivePcDesc = nullptr;
		SyncActivePcToServer(WorldContext, nullptr);
	}
}

bool FillFromStore(
	TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch,
	UObject* WorldContext,
	FFactoryCustomizationColorSlot& Out)
{
	APCSwatchStoreSubsystem* Store = StoreFor(WorldContext);
	if (!Store || !APCSwatchStoreSubsystem::IsPCCustomization(Swatch))
	{
		return false;
	}

	FPCSwatchEntry Entry;
	if (!Store->TryGetBySwatch(Swatch, Entry))
	{
		return false;
	}

	Out = Entry.ToSlot();
	if (!Out.PaintFinish)
	{
		const FSoftClassPath DefaultPath(TEXT(
			"/Game/FactoryGame/Buildable/-Shared/Customization/PaintFinishes/"
			"PaintFinishDesc_Default.PaintFinishDesc_Default_C"));
		Out.PaintFinish =
			DefaultPath.TryLoadClass<UFGFactoryCustomizationDescriptor_PaintFinish>();
	}
	return true;
}

bool HandleCustomSlotWrite(
	UObject* WorldContext,
	uint8 SlotIdx,
	const FFactoryCustomizationColorSlot& ColorData)
{
	if (SlotIdx != INDEX_CUSTOM_COLOR_SLOT)
	{
		return false;
	}
	if (!APCSwatchStoreSubsystem::IsPCCustomization(GActivePcDesc))
	{
		return false;
	}

	SubmitPcColors(WorldContext, GActivePcDesc, ColorData);
	UE_LOG(LogPipelineColor, Log, TEXT("%s custom slot → RCO/store"),
		PIPELINECOLOR_LOG_PREFIX);
	return true;
}
} // namespace

void FPCSwatchSlotDispatch::SetActivePcDesc(
	TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> Swatch)
{
	GActivePcDesc = APCSwatchStoreSubsystem::IsPCCustomization(Swatch) ? Swatch : nullptr;
}

TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch>
FPCSwatchSlotDispatch::GetActivePcDesc()
{
	return GActivePcDesc;
}

void FPCSwatchSlotDispatch::RegisterHooks()
{
	if (GHooksRegistered)
	{
		return;
	}
	GHooksRegistered = true;

#if WITH_EDITOR
	return;
#else
	SUBSCRIBE_METHOD(
		UFGBlueprintFunctionLibrary::GetSlotDataForSwatchDesc,
		[](auto& Scope,
			TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> SwatchDesc,
			AActor* WorldContext,
			FFactoryCustomizationColorSlot& Out_SlotData)
		{
			if (FillFromStore(SwatchDesc, WorldContext, Out_SlotData))
			{
				if (APCSwatchStoreSubsystem::IsPCCustomization(SwatchDesc))
				{
					GActivePcDesc = SwatchDesc;
					SyncActivePcToServer(WorldContext, SwatchDesc);
				}
				Scope.Cancel();
			}
		});

	SUBSCRIBE_METHOD_AFTER(
		UFGBuildGunStatePaint::SetActiveRecipe,
		[](UFGBuildGunStatePaint* Self,
			TSubclassOf<UFGCustomizationRecipe> Recipe)
		{
			RememberIfPc(Recipe, Self);
		});

	SUBSCRIBE_METHOD_AFTER(
		UFGBuildGunStatePaint::SetActiveSwatchDesc,
		[](UFGBuildGunStatePaint* Self,
			TSubclassOf<UFGFactoryCustomizationDescriptor_Swatch> SwatchDesc)
		{
			if (APCSwatchStoreSubsystem::IsPCCustomization(SwatchDesc))
			{
				GActivePcDesc = SwatchDesc;
				SyncActivePcToServer(Self, SwatchDesc);
			}
			else
			{
				GActivePcDesc = nullptr;
				SyncActivePcToServer(Self, nullptr);
			}
		});

	SUBSCRIBE_METHOD(
		AFGBuildGun::SetCustomizationDataForSlot,
		[](auto& Scope,
			AFGBuildGun* Self,
			uint8 SlotIdx,
			FFactoryCustomizationColorSlot ColorData)
		{
			if (HandleCustomSlotWrite(Self, SlotIdx, ColorData))
			{
				Scope.Cancel();
			}
		});

	SUBSCRIBE_METHOD_VIRTUAL(
		AFGBuildGun::Server_SetCustomizationDataForSlot_Implementation,
		GetMutableDefault<AFGBuildGun>(),
		[](auto& Scope,
			AFGBuildGun* Self,
			uint8 SlotIdx,
			FFactoryCustomizationColorSlot ColorData)
		{
			if (HandleCustomSlotWrite(Self, SlotIdx, ColorData))
			{
				Scope.Cancel();
			}
		});

	SUBSCRIBE_METHOD_VIRTUAL(
		AFGGameState::Server_SetBuildingColorDataForSlot_Implementation,
		GetMutableDefault<AFGGameState>(),
		[](auto& Scope,
			AFGGameState* GameState,
			uint8 SlotIdx,
			FFactoryCustomizationColorSlot ColorData)
		{
			if (HandleCustomSlotWrite(GameState, SlotIdx, ColorData))
			{
				Scope.Cancel();
			}
		});

	UE_LOG(LogPipelineColor, Log, TEXT("%s swatch dispatch hooks registered"),
		PIPELINECOLOR_LOG_PREFIX);
#endif
}
