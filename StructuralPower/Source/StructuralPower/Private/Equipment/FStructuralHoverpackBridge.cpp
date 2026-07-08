// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Equipment/FStructuralHoverpackBridge.h"

#include "Config/FStructuralPowerModConfig.h"
#include "Diagnostics/FStructuralPowerTrace.h"
#include "Buildables/FGBuildableRailroadTrack.h"
#include "FGPowerConnectionComponent.h"
#include "FGPowerInfoComponent.h"
#include "Equipment/FGHoverPack.h"
#include "FGPlayerController.h"
#include "Network/UStructuralPowerRCO.h"
#include "Patching/NativeHookManager.h"
#include "Save/AStructuralPowerGraphSubsystem.h"
#include "Session/FStructuralPowerSessionSettings.h"
#include "StructuralPowerLog.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"

namespace
{
constexpr float HoverpackFeedStickinessDistance = 800.0f;

struct FStructuralHoverpackCandidate
{
	EStructuralHoverpackTetherSource Source = EStructuralHoverpackTetherSource::None;
	UFGPowerConnectionComponent* Feed = nullptr;
	FVector Anchor = FVector::ZeroVector;
	int32 StructuralRoot = INDEX_NONE;
	float Distance = 0.0f;
};

struct FStructuralHoverpackPropertyCache
{
	FBoolProperty* HasConnection = nullptr;
	FStructProperty* ConnectionLocation = nullptr;
	FFloatProperty* SearchRadius = nullptr;
	FFloatProperty* SearchTickRate = nullptr;
	FFloatProperty* DisconnectionTime = nullptr;
	FFloatProperty* DisconnectionTimer = nullptr;
	FFloatProperty* PowerConsumption = nullptr;
	FObjectProperty* CurrentPowerConnection = nullptr;
	FObjectProperty* PowerInfo = nullptr;
	FObjectProperty* OwnPowerConnection = nullptr;
	bool bInitialized = false;

	void Ensure(const UClass* Class)
	{
		if (bInitialized || !Class)
		{
			return;
		}

		HasConnection = FindFProperty<FBoolProperty>(Class, TEXT("mHasConnection"));
		ConnectionLocation = FindFProperty<FStructProperty>(Class, TEXT("mCurrentConnectionLocation"));
		SearchRadius = FindFProperty<FFloatProperty>(Class, TEXT("mPowerConnectionSearchRadius"));
		SearchTickRate = FindFProperty<FFloatProperty>(Class, TEXT("mPowerConnectionSearchTickRate"));
		DisconnectionTime = FindFProperty<FFloatProperty>(Class, TEXT("mPowerConnectionDisconnectionTime"));
		DisconnectionTimer = FindFProperty<FFloatProperty>(Class, TEXT("mPowerDisconnectionTimer"));
		PowerConsumption = FindFProperty<FFloatProperty>(Class, TEXT("mPowerConsumption"));
		CurrentPowerConnection = FindFProperty<FObjectProperty>(Class, TEXT("mCurrentPowerConnection"));
		PowerInfo = FindFProperty<FObjectProperty>(Class, TEXT("mPowerInfo"));
		OwnPowerConnection = FindFProperty<FObjectProperty>(Class, TEXT("mPowerConnection"));
		bInitialized = true;
	}
};

FStructuralHoverpackPropertyCache GHoverpackProps;

void SetHasConnection(AFGHoverPack* Pack, bool bValue)
{
	GHoverpackProps.Ensure(Pack ? Pack->GetClass() : nullptr);
	if (GHoverpackProps.HasConnection)
	{
		GHoverpackProps.HasConnection->SetPropertyValue_InContainer(Pack, bValue);
	}
}

void SetConnectionLocation(AFGHoverPack* Pack, const FVector& Location)
{
	GHoverpackProps.Ensure(Pack ? Pack->GetClass() : nullptr);
	if (GHoverpackProps.ConnectionLocation)
	{
		if (FVector* Ptr = GHoverpackProps.ConnectionLocation->ContainerPtrToValuePtr<FVector>(Pack))
		{
			*Ptr = Location;
		}
	}
}

void SetCurrentPowerConnection(AFGHoverPack* Pack, UFGPowerConnectionComponent* Connection)
{
	GHoverpackProps.Ensure(Pack ? Pack->GetClass() : nullptr);
	if (GHoverpackProps.CurrentPowerConnection)
	{
		GHoverpackProps.CurrentPowerConnection->SetObjectPropertyValue_InContainer(Pack, Connection);
	}
}

void WriteSearchRadius(AFGHoverPack* Pack, float Value)
{
	GHoverpackProps.Ensure(Pack ? Pack->GetClass() : nullptr);
	if (GHoverpackProps.SearchRadius)
	{
		GHoverpackProps.SearchRadius->SetPropertyValue_InContainer(Pack, Value);
	}
}

void WriteDisconnectionTimer(AFGHoverPack* Pack, float Value)
{
	GHoverpackProps.Ensure(Pack ? Pack->GetClass() : nullptr);
	if (GHoverpackProps.DisconnectionTimer)
	{
		GHoverpackProps.DisconnectionTimer->SetPropertyValue_InContainer(Pack, Value);
	}
}

UFGPowerInfoComponent* GetHoverpackPowerInfo(AFGHoverPack* Pack)
{
	GHoverpackProps.Ensure(Pack ? Pack->GetClass() : nullptr);
	if (!GHoverpackProps.PowerInfo)
	{
		return nullptr;
	}

	return Cast<UFGPowerInfoComponent>(
		GHoverpackProps.PowerInfo->GetObjectPropertyValue_InContainer(Pack));
}

UFGPowerConnectionComponent* GetHoverpackOwnConnection(AFGHoverPack* Pack)
{
	GHoverpackProps.Ensure(Pack ? Pack->GetClass() : nullptr);
	if (!GHoverpackProps.OwnPowerConnection)
	{
		return nullptr;
	}

	return Cast<UFGPowerConnectionComponent>(
		GHoverpackProps.OwnPowerConnection->GetObjectPropertyValue_InContainer(Pack));
}

float ReadPowerConsumption(const AFGHoverPack* Pack)
{
	GHoverpackProps.Ensure(Pack ? Pack->GetClass() : nullptr);
	if (GHoverpackProps.PowerConsumption)
	{
		return GHoverpackProps.PowerConsumption->GetPropertyValue_InContainer(Pack);
	}

	return 0.0f;
}

float ReadSearchRadius(const AFGHoverPack* Pack)
{
	GHoverpackProps.Ensure(Pack ? Pack->GetClass() : nullptr);
	if (GHoverpackProps.SearchRadius)
	{
		return GHoverpackProps.SearchRadius->GetPropertyValue_InContainer(Pack);
	}

	return 3000.0f;
}

float ReadSearchTickRate(const AFGHoverPack* Pack)
{
	GHoverpackProps.Ensure(Pack ? Pack->GetClass() : nullptr);
	if (GHoverpackProps.SearchTickRate)
	{
		return GHoverpackProps.SearchTickRate->GetPropertyValue_InContainer(Pack);
	}

	return 0.1f;
}

float ReadDisconnectionTime(const AFGHoverPack* Pack)
{
	GHoverpackProps.Ensure(Pack ? Pack->GetClass() : nullptr);
	if (GHoverpackProps.DisconnectionTime)
	{
		return GHoverpackProps.DisconnectionTime->GetPropertyValue_InContainer(Pack);
	}

	return 0.5f;
}

struct FCylindricalTetherMetrics
{
	float Horizontal = 0.0f;
	float Vertical = 0.0f;
	float Limiting = 0.0f;
	float StraightLine = 0.0f;
};

FCylindricalTetherMetrics ComputeCylindricalTetherMetrics(
	const FVector& OwnerLoc,
	const FVector& AnchorLoc,
	float MaxDistance)
{
	FCylindricalTetherMetrics Metrics;
	Metrics.Horizontal = FVector2D::Distance(
		FVector2D(OwnerLoc.X, OwnerLoc.Y),
		FVector2D(AnchorLoc.X, AnchorLoc.Y));
	Metrics.Vertical = FMath::Abs(OwnerLoc.Z - AnchorLoc.Z);
	Metrics.Limiting = FMath::Max(Metrics.Horizontal, Metrics.Vertical);
	Metrics.StraightLine = FVector::Dist(OwnerLoc, AnchorLoc);
	(void)MaxDistance;
	return Metrics;
}

bool IsWithinCylindricalTetherRange(
	const FCylindricalTetherMetrics& Metrics,
	float MaxHorizontal,
	float MaxVertical)
{
	return MaxHorizontal > 0.0f
		&& MaxVertical > 0.0f
		&& Metrics.Horizontal <= MaxHorizontal
		&& Metrics.Vertical <= MaxVertical;
}

float NormalizedCylindricalTetherRange(
	const FCylindricalTetherMetrics& Metrics,
	float MaxHorizontal,
	float MaxVertical)
{
	if (MaxHorizontal <= 0.0f || MaxVertical <= 0.0f)
	{
		return 0.0f;
	}

	const float Normalized = FMath::Max(
		Metrics.Horizontal / MaxHorizontal,
		Metrics.Vertical / MaxVertical);
	return FMath::Clamp(Normalized, 0.0f, 1.0f);
}

bool ReadConnectionLocation(const AFGHoverPack* Pack, FVector& OutLocation)
{
	GHoverpackProps.Ensure(Pack ? Pack->GetClass() : nullptr);
	if (!GHoverpackProps.ConnectionLocation)
	{
		return false;
	}

	if (const FVector* Ptr = GHoverpackProps.ConnectionLocation->ContainerPtrToValuePtr<FVector>(Pack))
	{
		OutLocation = *Ptr;
		return true;
	}

	return false;
}

struct FClientHoverpackTetherMirror
{
	bool bValid = false;
	FVector Anchor = FVector::ZeroVector;
	float MaxHorizontal = 0.0f;
	float MaxVertical = 0.0f;
};

FClientHoverpackTetherMirror GClientHoverpackTetherMirror;

bool IsLocallyOwnedHoverpack(const AFGHoverPack* Pack)
{
	if (!IsValid(Pack))
	{
		return false;
	}

	const APawn* Pawn = Cast<APawn>(Pack->GetOwner());
	return IsValid(Pawn) && Pawn->IsLocallyControlled();
}

FCylindricalTetherMetrics SessionTetherMetrics(
	const AFGHoverPack* Pack,
	const FStructuralHoverpackSession& Session)
{
	const FVector OwnerLoc = Pack && Pack->GetOwner()
		? Pack->GetOwner()->GetActorLocation()
		: (Pack ? Pack->GetActorLocation() : FVector::ZeroVector);
	return ComputeCylindricalTetherMetrics(OwnerLoc, Session.Anchor, 0.0f);
}
}

TMap<TWeakObjectPtr<AFGHoverPack>, FStructuralHoverpackSession>
	FStructuralHoverpackBridge::Sessions;

void FStructuralHoverpackBridge::RegisterHooks()
{
	AFGHoverPack* Sample = GetMutableDefault<AFGHoverPack>();
	GHoverpackProps.Ensure(Sample ? Sample->GetClass() : nullptr);

	SUBSCRIBE_METHOD_VIRTUAL(
		AFGHoverPack::ConnectToNearestPowerConnection,
		Sample,
		[](auto& Scope, AFGHoverPack* Pack)
		{
			if (!OwnsConnectivity(Pack) || Pack->GetCurrentRailroadTrack())
			{
				return;
			}

			ResolveTether(Pack);
			Scope.Cancel();
		});

	SUBSCRIBE_METHOD_VIRTUAL(
		AFGHoverPack::Tick,
		Sample,
		[](auto& /*Scope*/, AFGHoverPack* Pack, float DeltaTime)
		{
			BeforeTick(Pack, DeltaTime);
		});

	SUBSCRIBE_METHOD_VIRTUAL_AFTER(
		AFGHoverPack::Tick,
		Sample,
		[](AFGHoverPack* Pack, float DeltaTime)
		{
			AfterTick(Pack, DeltaTime);
		});

	SUBSCRIBE_METHOD_VIRTUAL_AFTER(
		AFGHoverPack::UnEquip,
		Sample,
		[](AFGHoverPack* Pack)
		{
			OnUnEquip(Pack);
		});

	SUBSCRIBE_METHOD_VIRTUAL(
		AFGHoverPack::GetMaxDistanceFromCurrentConnection,
		Sample,
		[](auto& Scope, const AFGHoverPack* Pack)
		{
			if (!OwnsConnectivity(Pack) || Pack->GetCurrentRailroadTrack())
			{
				return;
			}

			FVector Anchor = FVector::ZeroVector;
			float MaxHorizontal = 0.0f;
			float MaxVertical = 0.0f;
			FStructuralHoverpackTetherMetrics Metrics;
			if (ResolvePublishedTether(Pack, Anchor, MaxHorizontal, MaxVertical, Metrics))
			{
				Scope.Override(FMath::Min(MaxHorizontal, MaxVertical));
			}
		});

	SUBSCRIBE_METHOD_VIRTUAL(
		AFGHoverPack::GetDistanceFromCurrentConnection,
		Sample,
		[](auto& Scope, const AFGHoverPack* Pack)
		{
			FVector Anchor = FVector::ZeroVector;
			float MaxHorizontal = 0.0f;
			float MaxVertical = 0.0f;
			FStructuralHoverpackTetherMetrics Metrics;
			if (ResolvePublishedTether(Pack, Anchor, MaxHorizontal, MaxVertical, Metrics))
			{
				Scope.Override(Metrics.StraightLine);
			}
		});

	SUBSCRIBE_METHOD_VIRTUAL(
		AFGHoverPack::GetCurrentConnectionLocation,
		Sample,
		[](auto& Scope, const AFGHoverPack* Pack)
		{
			FVector Anchor = FVector::ZeroVector;
			float MaxHorizontal = 0.0f;
			float MaxVertical = 0.0f;
			FStructuralHoverpackTetherMetrics Metrics;
			if (ResolvePublishedTether(Pack, Anchor, MaxHorizontal, MaxVertical, Metrics))
			{
				Scope.Override(Anchor);
			}
		});

	SUBSCRIBE_METHOD_VIRTUAL(
		AFGHoverPack::GetNormalizedDistanceFromConnection,
		Sample,
		[](auto& Scope, const AFGHoverPack* Pack)
		{
			FVector Anchor = FVector::ZeroVector;
			float MaxHorizontal = 0.0f;
			float MaxVertical = 0.0f;
			FStructuralHoverpackTetherMetrics Metrics;
			if (ResolvePublishedTether(Pack, Anchor, MaxHorizontal, MaxVertical, Metrics))
			{
				FCylindricalTetherMetrics Cylindrical;
				Cylindrical.Horizontal = Metrics.Horizontal;
				Cylindrical.Vertical = Metrics.Vertical;
				Cylindrical.Limiting = Metrics.Limiting;
				Cylindrical.StraightLine = Metrics.StraightLine;
				Scope.Override(NormalizedCylindricalTetherRange(
					Cylindrical,
					MaxHorizontal,
					MaxVertical));
			}
		});

	SUBSCRIBE_METHOD_VIRTUAL(
		AFGHoverPack::GetHeightAboveCurrentConnection,
		Sample,
		[](auto& Scope, const AFGHoverPack* Pack)
		{
			FVector Anchor = FVector::ZeroVector;
			float MaxHorizontal = 0.0f;
			float MaxVertical = 0.0f;
			FStructuralHoverpackTetherMetrics Metrics;
			if (ResolvePublishedTether(Pack, Anchor, MaxHorizontal, MaxVertical, Metrics))
			{
				Scope.Override(FMath::Clamp(
					Metrics.Vertical / MaxVertical,
					0.0f,
					1.0f));
			}
		});
}

bool FStructuralHoverpackBridge::OwnsConnectivity(const AFGHoverPack* Pack)
{
	return IsStructuralPathEnabled(Pack);
}

const FStructuralHoverpackSession* FStructuralHoverpackBridge::FindSession(const AFGHoverPack* Pack)
{
	if (!IsValid(Pack))
	{
		return nullptr;
	}

	return Sessions.Find(Pack);
}

FStructuralHoverpackSession& FStructuralHoverpackBridge::GetOrCreateSession(AFGHoverPack* Pack)
{
	for (auto It = Sessions.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}

	return Sessions.FindOrAdd(Pack);
}

bool FStructuralHoverpackBridge::IsStructuralPathEnabled(const AFGHoverPack* Pack)
{
	return IsValid(Pack);
}

float FStructuralHoverpackBridge::GetBaseSearchRadius(const AFGHoverPack* Pack)
{
	(void)Pack;

	static float CdoRadius = 0.0f;
	if (CdoRadius <= 0.0f)
	{
		const AFGHoverPack* Cdo = GetDefault<AFGHoverPack>();
		GHoverpackProps.Ensure(Cdo ? Cdo->GetClass() : nullptr);
		CdoRadius = FMath::Max(1.0f, ReadSearchRadius(Cdo));
	}

	return CdoRadius;
}

float FStructuralHoverpackBridge::GetVanillaFieldRadius(const FStructuralHoverpackSession& Session)
{
	return FMath::Min(Session.MaxHorizontal, Session.MaxVertical);
}

float FStructuralHoverpackBridge::GetReconnectCooldownSeconds(const AFGHoverPack* Pack)
{
	return FMath::Max(0.25f, ReadDisconnectionTime(Pack));
}

FVector FStructuralHoverpackBridge::GetOwnerLocation(const AFGHoverPack* Pack)
{
	if (!IsValid(Pack))
	{
		return FVector::ZeroVector;
	}

	if (const AActor* Owner = Pack->GetOwner())
	{
		return Owner->GetActorLocation();
	}

	return Pack->GetActorLocation();
}

void FStructuralHoverpackBridge::LogTransition(
	const AFGHoverPack* Pack,
	const TCHAR* Event,
	const FStructuralHoverpackSession& Session,
	float Distance)
{
	if (!FStructuralPowerTrace::IsEnabled() || !IsValid(Pack))
	{
		return;
	}

	UFGPowerConnectionComponent* Feed = Session.Feed.Get();
	const FCylindricalTetherMetrics Metrics = SessionTetherMetrics(Pack, Session);
	(void)Distance;
	UE_LOG(LogStructuralPower, Log,
		TEXT("[HALSP] hoverpack %s %s src=%d root=%d h=%.0f v=%.0f lim=%.0f maxH=%.0f maxV=%.0f feed=%s circuit=%d hasPower=%d"),
		*Pack->GetName(),
		Event,
		static_cast<int32>(Session.Source),
		Session.StructuralRoot,
		Metrics.Horizontal,
		Metrics.Vertical,
		Metrics.Limiting,
		Session.MaxHorizontal,
		Session.MaxVertical,
		Feed ? *Feed->GetName() : TEXT("-"),
		Feed ? Feed->GetCircuitID() : INDEX_NONE,
		Feed && Feed->HasPower() ? 1 : 0);
}

void FStructuralHoverpackBridge::DisconnectPower(AFGHoverPack* Pack)
{
	if (!IsValid(Pack))
	{
		return;
	}

	if (UFGPowerInfoComponent* PowerInfo = GetHoverpackPowerInfo(Pack))
	{
		PowerInfo->SetTargetConsumption(0.0f);
		PowerInfo->SetCircuitID(INDEX_NONE);
	}

	SetCurrentPowerConnection(Pack, nullptr);
	SetHasConnection(Pack, false);
	WriteDisconnectionTimer(Pack, 0.0f);
}

bool FStructuralHoverpackBridge::SyncPowerFeed(
	AFGHoverPack* Pack,
	UFGPowerConnectionComponent* Feed)
{
	if (!IsValid(Pack) || !IsValid(Feed) || !Feed->HasPower())
	{
		return false;
	}

	const int32 CircuitId = Feed->GetCircuitID();
	if (CircuitId == INDEX_NONE)
	{
		return false;
	}

	UFGPowerInfoComponent* PowerInfo = GetHoverpackPowerInfo(Pack);
	if (!PowerInfo)
	{
		return false;
	}

	const float Consumption = ReadPowerConsumption(Pack);
	if (Pack->GetCurrentPowerConnection() == Feed
		&& Pack->HasConnection()
		&& PowerInfo->IsConnected()
		&& PowerInfo->GetTargetConsumption() == Consumption)
	{
		return true;
	}

	PowerInfo->SetCircuitID(CircuitId);
	PowerInfo->SetTargetConsumption(Consumption);
	PowerInfo->SetMaximumTargetConsumption(Consumption);
	SetCurrentPowerConnection(Pack, Feed);
	SetHasConnection(Pack, true);
	return true;
}

FStructuralHoverpackAxisLimits FStructuralHoverpackBridge::GetSessionAxisLimits(
	EStructuralHoverpackTetherSource Source,
	float BaseSearchRadius)
{
	const float Base = FMath::Max(1.0f, BaseSearchRadius);
	FStructuralHoverpackAxisLimits Limits;
	if (Source == EStructuralHoverpackTetherSource::StructuralGeometry)
	{
		Limits.Horizontal = Base * FStructuralPowerModConfig::GetHoverpackHorizontalMultiplier();
		Limits.Vertical = Base * FStructuralPowerModConfig::GetHoverpackVerticalMultiplier();
	}
	else
	{
		Limits.Horizontal = Base;
		Limits.Vertical = Base;
	}

	return Limits;
}

void FStructuralHoverpackBridge::SyncStructuralVirtualTether(
	AFGHoverPack* Pack,
	FStructuralHoverpackSession& Session)
{
	if (Session.Source != EStructuralHoverpackTetherSource::StructuralGeometry)
	{
		return;
	}

	WriteSearchRadius(Pack, GetVanillaFieldRadius(Session));
	SetConnectionLocation(Pack, Session.Anchor);
	SetCurrentPowerConnection(Pack, nullptr);

	UFGPowerConnectionComponent* Feed = Session.Feed.Get();
	if (!IsValid(Feed) || !Feed->HasPower())
	{
		SetHasConnection(Pack, false);
		PublishStructuralTetherToOwningClient(Pack, Session, /*bClear=*/true);
		return;
	}

	UFGPowerInfoComponent* PowerInfo = GetHoverpackPowerInfo(Pack);
	if (!PowerInfo)
	{
		SetHasConnection(Pack, false);
		PublishStructuralTetherToOwningClient(Pack, Session, /*bClear=*/true);
		return;
	}

	const int32 CircuitId = Feed->GetCircuitID();
	if (CircuitId == INDEX_NONE)
	{
		SetHasConnection(Pack, false);
		PublishStructuralTetherToOwningClient(Pack, Session, /*bClear=*/true);
		return;
	}

	const float Consumption = ReadPowerConsumption(Pack);
	PowerInfo->SetCircuitID(CircuitId);
	PowerInfo->SetTargetConsumption(Consumption);
	PowerInfo->SetMaximumTargetConsumption(Consumption);
	SetHasConnection(Pack, true);
	PublishStructuralTetherToOwningClient(Pack, Session, /*bClear=*/false);
}

void FStructuralHoverpackBridge::PublishStructuralTetherToOwningClient(
	AFGHoverPack* Pack,
	FStructuralHoverpackSession& Session,
	bool bClear)
{
	if (!IsValid(Pack) || !Pack->HasAuthority())
	{
		return;
	}

	UWorld* World = Pack->GetWorld();
	if (!IsValid(World) || World->GetNetMode() == NM_Standalone)
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(Pack->GetOwner());
	if (!IsValid(Pawn))
	{
		return;
	}

	AFGPlayerController* PlayerController = Cast<AFGPlayerController>(Pawn->GetController());
	if (!IsValid(PlayerController))
	{
		return;
	}

	UStructuralPowerRCO* Rco = PlayerController->GetRemoteCallObjectOfClass<UStructuralPowerRCO>();
	if (!IsValid(Rco))
	{
		return;
	}

	if (bClear)
	{
		if (!Session.bClientTetherPublished)
		{
			return;
		}

		Session.bClientTetherPublished = false;
		Rco->Client_ClearHoverpackTether();
		return;
	}

	if (Session.Source != EStructuralHoverpackTetherSource::StructuralGeometry
		|| !ShouldPublishSessionToVanilla(Pack, Session))
	{
		PublishStructuralTetherToOwningClient(Pack, Session, /*bClear=*/true);
		return;
	}

	const bool bUnchanged = Session.bClientTetherPublished
		&& Session.ClientPublishedAnchor.Equals(Session.Anchor, 1.0f)
		&& FMath::IsNearlyEqual(Session.ClientPublishedMaxHorizontal, Session.MaxHorizontal)
		&& FMath::IsNearlyEqual(Session.ClientPublishedMaxVertical, Session.MaxVertical);
	if (bUnchanged)
	{
		return;
	}

	Session.bClientTetherPublished = true;
	Session.ClientPublishedAnchor = Session.Anchor;
	Session.ClientPublishedMaxHorizontal = Session.MaxHorizontal;
	Session.ClientPublishedMaxVertical = Session.MaxVertical;
	Rco->Client_SyncHoverpackTether(Session.Anchor, Session.MaxHorizontal, Session.MaxVertical);
}

void FStructuralHoverpackBridge::ApplyClientTetherMirror(
	const FVector& Anchor,
	float MaxHorizontal,
	float MaxVertical)
{
	GClientHoverpackTetherMirror.bValid = true;
	GClientHoverpackTetherMirror.Anchor = Anchor;
	GClientHoverpackTetherMirror.MaxHorizontal = MaxHorizontal;
	GClientHoverpackTetherMirror.MaxVertical = MaxVertical;
}

void FStructuralHoverpackBridge::ClearClientTetherMirror()
{
	GClientHoverpackTetherMirror = FClientHoverpackTetherMirror{};
}

bool FStructuralHoverpackBridge::TryGetClientTetherMirror(
	FVector& OutAnchor,
	float& OutMaxHorizontal,
	float& OutMaxVertical)
{
	if (!GClientHoverpackTetherMirror.bValid)
	{
		return false;
	}

	OutAnchor = GClientHoverpackTetherMirror.Anchor;
	OutMaxHorizontal = GClientHoverpackTetherMirror.MaxHorizontal;
	OutMaxVertical = GClientHoverpackTetherMirror.MaxVertical;
	return OutMaxHorizontal > 0.0f && OutMaxVertical > 0.0f;
}

void FStructuralHoverpackBridge::SyncReplicationFields(
	AFGHoverPack* Pack,
	FStructuralHoverpackSession& Session)
{
	if (Session.Source == EStructuralHoverpackTetherSource::None)
	{
		return;
	}

	WriteSearchRadius(Pack, GetVanillaFieldRadius(Session));
	SetConnectionLocation(Pack, Session.Anchor);
	if (Session.Source == EStructuralHoverpackTetherSource::StructuralGeometry)
	{
		SyncStructuralVirtualTether(Pack, Session);
		return;
	}

	if (UFGPowerConnectionComponent* Feed = Session.Feed.Get())
	{
		SyncPowerFeed(Pack, Feed);
	}
}

void FStructuralHoverpackBridge::ClearSession(
	AFGHoverPack* Pack,
	bool bDisconnectPower,
	const TCHAR* Event)
{
	if (!IsValid(Pack))
	{
		return;
	}

	double SuppressUntil = 0.0;
	if (UWorld* World = Pack->GetWorld())
	{
		SuppressUntil = World->GetTimeSeconds() + GetReconnectCooldownSeconds(Pack);
	}

	if (FStructuralHoverpackSession* Session = Sessions.Find(Pack))
	{
		if (Session->Source != EStructuralHoverpackTetherSource::None)
		{
			LogTransition(Pack, Event, *Session, 0.0f);
		}

		PublishStructuralTetherToOwningClient(Pack, *Session, /*bClear=*/true);
	}

	if (bDisconnectPower)
	{
		DisconnectPower(Pack);
	}

	WriteSearchRadius(Pack, GetBaseSearchRadius(Pack));

	FStructuralHoverpackSession Cooldown;
	Cooldown.SuppressReconnectUntil = SuppressUntil;
	Sessions.Add(Pack, Cooldown);
}

bool FStructuralHoverpackBridge::FindNearestVisibleConnector(
	AFGHoverPack* Pack,
	float MaxDistance,
	UFGPowerConnectionComponent* PreferredFeed,
	UFGPowerConnectionComponent*& OutFeed,
	FVector& OutAnchor,
	float& OutDistance)
{
	OutFeed = nullptr;
	OutAnchor = FVector::ZeroVector;
	OutDistance = 0.0f;

	UWorld* World = Pack->GetWorld();
	if (!IsValid(World))
	{
		return false;
	}

	const FVector OwnerLoc = GetOwnerLocation(Pack);
	UFGPowerConnectionComponent* OwnConn = GetHoverpackOwnConnection(Pack);

	if (IsValid(PreferredFeed) && PreferredFeed != OwnConn && PreferredFeed->GetWorld() == World
		&& !PreferredFeed->IsHidden() && PreferredFeed->HasPower()
		&& PreferredFeed->GetCircuitID() != INDEX_NONE)
	{
		const FVector PreferredAnchor = PreferredFeed->GetComponentLocation();
		const FCylindricalTetherMetrics PreferredMetrics = ComputeCylindricalTetherMetrics(
			OwnerLoc,
			PreferredAnchor,
			MaxDistance);
		if (IsWithinCylindricalTetherRange(PreferredMetrics, MaxDistance, MaxDistance))
		{
			OutFeed = PreferredFeed;
			OutAnchor = PreferredAnchor;
			OutDistance = PreferredMetrics.Limiting;
			return true;
		}
	}

	UFGPowerConnectionComponent* Best = nullptr;
	float BestLimiting = FLT_MAX;

	for (TObjectIterator<UFGPowerConnectionComponent> It; It; ++It)
	{
		UFGPowerConnectionComponent* Conn = *It;
		if (!IsValid(Conn) || Conn->GetWorld() != World || Conn == OwnConn)
		{
			continue;
		}

		if (Conn->IsHidden() || !Conn->HasPower() || Conn->GetCircuitID() == INDEX_NONE
			|| Conn->HasAnyFlags(RF_ClassDefaultObject))
		{
			continue;
		}

		const FVector Anchor = Conn->GetComponentLocation();
		const FCylindricalTetherMetrics Metrics = ComputeCylindricalTetherMetrics(
			OwnerLoc,
			Anchor,
			MaxDistance);
		if (!IsWithinCylindricalTetherRange(Metrics, MaxDistance, MaxDistance))
		{
			continue;
		}

		if (!Best
			|| Metrics.Limiting + HoverpackFeedStickinessDistance < BestLimiting)
		{
			BestLimiting = Metrics.Limiting;
			Best = Conn;
		}
	}

	if (!Best)
	{
		return false;
	}

	OutFeed = Best;
	OutAnchor = Best->GetComponentLocation();
	OutDistance = BestLimiting;
	return true;
}

bool FStructuralHoverpackBridge::FindStructuralTether(
	AFGHoverPack* Pack,
	float MaxHorizontal,
	float MaxVertical,
	UFGPowerConnectionComponent*& OutFeed,
	FVector& OutAnchor,
	int32& OutRoot,
	float& OutDistance)
{
	OutFeed = nullptr;
	OutAnchor = FVector::ZeroVector;
	OutRoot = INDEX_NONE;
	OutDistance = 0.0f;

	UWorld* World = Pack->GetWorld();
	AStructuralPowerGraphSubsystem* Graph = IsValid(World)
		? AStructuralPowerGraphSubsystem::Find(World)
		: nullptr;
	if (!Graph)
	{
		return false;
	}

	FStructuralHoverpackAnchorQuery Query;
	const FVector OwnerLoc = GetOwnerLocation(Pack);
	if (!Graph->QueryHoverpackStructuralAnchor(OwnerLoc, MaxHorizontal, MaxVertical, Query))
	{
		return false;
	}

	OutFeed = Query.FeedConnector.Get();
	if (!Query.bPowered || !IsValid(OutFeed) || !OutFeed->HasPower())
	{
		return false;
	}

	OutAnchor = Query.Anchor;
	OutRoot = Query.ComponentRoot;
	const FCylindricalTetherMetrics Metrics = ComputeCylindricalTetherMetrics(
		OwnerLoc,
		OutAnchor,
		0.0f);
	if (!IsWithinCylindricalTetherRange(Metrics, MaxHorizontal, MaxVertical))
	{
		return false;
	}

	OutDistance = Metrics.Limiting;
	return true;
}

bool FStructuralHoverpackBridge::ApplySession(
	AFGHoverPack* Pack,
	FStructuralHoverpackSession& Session,
	EStructuralHoverpackTetherSource Source,
	UFGPowerConnectionComponent* Feed,
	const FVector& Anchor,
	int32 StructuralRoot,
	float Distance,
	bool bLogTransition)
{
	if (!IsValid(Feed) || !Feed->HasPower())
	{
		return false;
	}

	const bool bSourceChanged = Session.Source != Source
		|| Session.Feed.Get() != Feed
		|| Session.StructuralRoot != StructuralRoot;

	Session.Source = Source;
	Session.Feed = Feed;
	Session.Anchor = Anchor;
	Session.StructuralRoot = StructuralRoot;
	if (Session.BaseSearchRadius <= 0.0f)
	{
		Session.BaseSearchRadius = GetBaseSearchRadius(Pack);
	}

	const FStructuralHoverpackAxisLimits Limits = GetSessionAxisLimits(Source, Session.BaseSearchRadius);
	Session.MaxHorizontal = Limits.Horizontal;
	Session.MaxVertical = Limits.Vertical;
	Session.DisconnectionTimer = 0.0f;
	Session.ResolveMissTimer = 0.0f;

	SyncReplicationFields(Pack, Session);
	WriteDisconnectionTimer(Pack, 0.0f);

	if (bLogTransition && bSourceChanged)
	{
		const TCHAR* Event = Source == EStructuralHoverpackTetherSource::VisibleConnector
			? TEXT("tether_connector")
			: TEXT("tether_structural");
		LogTransition(Pack, Event, Session, Distance);
	}

	return Pack->HasConnection();
}

bool FStructuralHoverpackBridge::ResolveTether(AFGHoverPack* Pack)
{
	if (!IsValid(Pack) || !Pack->HasAuthority())
	{
		return false;
	}

	if (Pack->GetCurrentRailroadTrack())
	{
		return false;
	}

	UWorld* World = Pack->GetWorld();
	const double Now = IsValid(World) ? World->GetTimeSeconds() : 0.0;
	FStructuralHoverpackSession& Session = GetOrCreateSession(Pack);
	if (!Pack->HasConnection() && Now < Session.SuppressReconnectUntil)
	{
		return false;
	}

	const float BaseRadius = Session.BaseSearchRadius > 0.0f
		? Session.BaseSearchRadius
		: GetBaseSearchRadius(Pack);
	const FStructuralHoverpackAxisLimits VisibleLimits = GetSessionAxisLimits(
		EStructuralHoverpackTetherSource::VisibleConnector,
		BaseRadius);
	const FStructuralHoverpackAxisLimits StructuralLimits = GetSessionAxisLimits(
		EStructuralHoverpackTetherSource::StructuralGeometry,
		BaseRadius);
	const float VisibleMax = VisibleLimits.Horizontal;

	UFGPowerConnectionComponent* PreferredFeed = nullptr;
	if (Session.Source == EStructuralHoverpackTetherSource::VisibleConnector)
	{
		PreferredFeed = Session.Feed.Get();
	}

	UFGPowerConnectionComponent* VisibleFeed = nullptr;
	FVector VisibleAnchor = FVector::ZeroVector;
	float VisibleDistance = 0.0f;
	const bool bHasVisible = FindNearestVisibleConnector(
		Pack,
		VisibleMax,
		PreferredFeed,
		VisibleFeed,
		VisibleAnchor,
		VisibleDistance);

	UFGPowerConnectionComponent* StructuralFeed = nullptr;
	FVector StructuralAnchor = FVector::ZeroVector;
	int32 StructuralRoot = INDEX_NONE;
	float StructuralDistance = 0.0f;
	const bool bHasStructural = FindStructuralTether(
		Pack,
		StructuralLimits.Horizontal,
		StructuralLimits.Vertical,
		StructuralFeed,
		StructuralAnchor,
		StructuralRoot,
		StructuralDistance);

	FStructuralHoverpackCandidate Challenger;
	if (bHasVisible && bHasStructural)
	{
		if (VisibleDistance <= StructuralDistance)
		{
			Challenger.Source = EStructuralHoverpackTetherSource::VisibleConnector;
			Challenger.Feed = VisibleFeed;
			Challenger.Anchor = VisibleAnchor;
			Challenger.StructuralRoot = INDEX_NONE;
			Challenger.Distance = VisibleDistance;
		}
		else
		{
			Challenger.Source = EStructuralHoverpackTetherSource::StructuralGeometry;
			Challenger.Feed = StructuralFeed;
			Challenger.Anchor = StructuralAnchor;
			Challenger.StructuralRoot = StructuralRoot;
			Challenger.Distance = StructuralDistance;
		}
	}
	else if (bHasVisible)
	{
		Challenger.Source = EStructuralHoverpackTetherSource::VisibleConnector;
		Challenger.Feed = VisibleFeed;
		Challenger.Anchor = VisibleAnchor;
		Challenger.Distance = VisibleDistance;
	}
	else if (bHasStructural)
	{
		Challenger.Source = EStructuralHoverpackTetherSource::StructuralGeometry;
		Challenger.Feed = StructuralFeed;
		Challenger.Anchor = StructuralAnchor;
		Challenger.StructuralRoot = StructuralRoot;
		Challenger.Distance = StructuralDistance;
	}
	else
	{
		return false;
	}

	if (Session.Source != EStructuralHoverpackTetherSource::None)
	{
		const FStructuralHoverpackAxisLimits Limits = GetSessionAxisLimits(Session.Source, BaseRadius);
		Session.MaxHorizontal = Limits.Horizontal;
		Session.MaxVertical = Limits.Vertical;
		if (ValidateAndRefreshSession(Pack, Session))
		{
			const FCylindricalTetherMetrics CurrentMetrics = SessionTetherMetrics(Pack, Session);
			if (ShouldPublishSessionToVanilla(Pack, Session)
				&& CurrentMetrics.Limiting
					<= Challenger.Distance + HoverpackFeedStickinessDistance)
			{
				SyncReplicationFields(Pack, Session);
				return true;
			}
		}
	}

	return ApplySession(
		Pack,
		Session,
		Challenger.Source,
		Challenger.Feed,
		Challenger.Anchor,
		Challenger.StructuralRoot,
		Challenger.Distance,
		/*bLogTransition=*/true);
}

bool FStructuralHoverpackBridge::ValidateAndRefreshSession(
	AFGHoverPack* Pack,
	FStructuralHoverpackSession& Session)
{
	if (Session.Source == EStructuralHoverpackTetherSource::None)
	{
		return false;
	}

	UFGPowerConnectionComponent* Feed = Session.Feed.Get();
	if (!IsValid(Feed) || !Feed->HasPower())
	{
		return false;
	}

	if (Session.Source == EStructuralHoverpackTetherSource::StructuralGeometry)
	{
		UFGPowerConnectionComponent* RefreshedFeed = nullptr;
		FVector RefreshedAnchor = FVector::ZeroVector;
		int32 RefreshedRoot = INDEX_NONE;
		float RefreshedDistance = 0.0f;
		if (FindStructuralTether(
				Pack,
				Session.MaxHorizontal,
				Session.MaxVertical,
				RefreshedFeed,
				RefreshedAnchor,
				RefreshedRoot,
				RefreshedDistance)
			&& RefreshedFeed == Feed)
		{
			Session.Anchor = RefreshedAnchor;
			Session.StructuralRoot = RefreshedRoot;
			SetConnectionLocation(Pack, RefreshedAnchor);
		}
	}
	else
	{
		Session.Anchor = Feed->GetComponentLocation();
		SetConnectionLocation(Pack, Session.Anchor);
	}

	return IsValid(Feed) && Feed->HasPower();
}

void FStructuralHoverpackBridge::MaintainSession(AFGHoverPack* Pack, float DeltaTime)
{
	if (!IsValid(Pack) || !Pack->HasAuthority())
	{
		return;
	}

	if (Pack->GetCurrentRailroadTrack())
	{
		if (Sessions.Contains(Pack))
		{
			ClearSession(Pack, /*bDisconnectPower=*/true, TEXT("tether_yield_rail"));
		}

		return;
	}

	FStructuralHoverpackSession& Session = GetOrCreateSession(Pack);
	const float TickRate = FMath::Max(0.05f, ReadSearchTickRate(Pack));
	Session.SearchTimer += DeltaTime;

	const bool bShouldSearch = Session.SearchTimer >= TickRate
		|| Session.Source == EStructuralHoverpackTetherSource::None;

	if (bShouldSearch)
	{
		Session.SearchTimer = 0.0f;
		ResolveTether(Pack);
	}

	if (Session.Source == EStructuralHoverpackTetherSource::None)
	{
		return;
	}

	UFGPowerConnectionComponent* Feed = Session.Feed.Get();
	if (!IsValid(Feed) || !Feed->HasPower())
	{
		ClearSession(Pack, /*bDisconnectPower=*/true, TEXT("tether_no_power"));
		return;
	}

	const FCylindricalTetherMetrics Metrics = SessionTetherMetrics(Pack, Session);
	if (!IsWithinCylindricalTetherRange(Metrics, Session.MaxHorizontal, Session.MaxVertical))
	{
		DisconnectPower(Pack);
		PublishStructuralTetherToOwningClient(Pack, Session, /*bClear=*/true);
		Session.DisconnectionTimer += DeltaTime;
		WriteDisconnectionTimer(Pack, Session.DisconnectionTimer);

		if (Session.DisconnectionTimer >= ReadDisconnectionTime(Pack))
		{
			ClearSession(Pack, /*bDisconnectPower=*/true, TEXT("tether_out_of_range"));
		}

		return;
	}

	Session.DisconnectionTimer = 0.0f;
	WriteDisconnectionTimer(Pack, 0.0f);
	if (Session.Source == EStructuralHoverpackTetherSource::StructuralGeometry)
	{
		ValidateAndRefreshSession(Pack, Session);
		SyncStructuralVirtualTether(Pack, Session);
	}
	else
	{
		ValidateAndRefreshSession(Pack, Session);
		SyncReplicationFields(Pack, Session);
	}
}

bool FStructuralHoverpackBridge::ShouldPublishSessionToVanilla(
	const AFGHoverPack* Pack,
	const FStructuralHoverpackSession& Session)
{
	if (Session.Source == EStructuralHoverpackTetherSource::None
		|| Session.MaxHorizontal <= 0.0f
		|| Session.MaxVertical <= 0.0f)
	{
		return false;
	}

	UFGPowerConnectionComponent* Feed = Session.Feed.Get();
	if (!IsValid(Feed) || !Feed->HasPower())
	{
		return false;
	}

	const FCylindricalTetherMetrics Metrics = SessionTetherMetrics(Pack, Session);
	return IsWithinCylindricalTetherRange(Metrics, Session.MaxHorizontal, Session.MaxVertical);
}

bool FStructuralHoverpackBridge::ResolvePublishedTether(
	const AFGHoverPack* Pack,
	FVector& OutAnchor,
	float& OutMaxHorizontal,
	float& OutMaxVertical,
	FStructuralHoverpackTetherMetrics& OutMetrics)
{
	if (!OwnsConnectivity(Pack) || Pack->GetCurrentRailroadTrack())
	{
		return false;
	}

	const FStructuralHoverpackSession* Session = FindSession(Pack);
	if (Session && ShouldPublishSessionToVanilla(Pack, *Session))
	{
		OutAnchor = Session->Anchor;
		OutMaxHorizontal = Session->MaxHorizontal;
		OutMaxVertical = Session->MaxVertical;
		const FCylindricalTetherMetrics Metrics = SessionTetherMetrics(Pack, *Session);
		OutMetrics.Horizontal = Metrics.Horizontal;
		OutMetrics.Vertical = Metrics.Vertical;
		OutMetrics.Limiting = Metrics.Limiting;
		OutMetrics.StraightLine = Metrics.StraightLine;
		return true;
	}

	if (!Pack->HasConnection())
	{
		return false;
	}

	if (const UFGPowerConnectionComponent* Feed = Pack->GetCurrentPowerConnection())
	{
		if (!IsValid(Feed))
		{
			return false;
		}

		OutAnchor = Feed->GetComponentLocation();
		const float Radius = ReadSearchRadius(Pack);
		OutMaxHorizontal = Radius;
		OutMaxVertical = Radius;
	}
	else
	{
		// Dedicated client: pack connection component never gets anchor — mirror from RCO.
		const bool bHasClientMirror =
			IsLocallyOwnedHoverpack(Pack)
			&& TryGetClientTetherMirror(OutAnchor, OutMaxHorizontal, OutMaxVertical);
		if (!bHasClientMirror)
		{
			if (!ReadConnectionLocation(Pack, OutAnchor) || OutAnchor.IsNearlyZero())
			{
				return false;
			}

			const FStructuralHoverpackAxisLimits Limits = GetSessionAxisLimits(
				EStructuralHoverpackTetherSource::StructuralGeometry,
				GetBaseSearchRadius(Pack));
			OutMaxHorizontal = Limits.Horizontal;
			OutMaxVertical = Limits.Vertical;
		}
	}

	const FCylindricalTetherMetrics Metrics = ComputeCylindricalTetherMetrics(
		GetOwnerLocation(Pack),
		OutAnchor,
		0.0f);
	OutMetrics.Horizontal = Metrics.Horizontal;
	OutMetrics.Vertical = Metrics.Vertical;
	OutMetrics.Limiting = Metrics.Limiting;
	OutMetrics.StraightLine = Metrics.StraightLine;
	return OutMaxHorizontal > 0.0f && OutMaxVertical > 0.0f;
}

void FStructuralHoverpackBridge::RestoreAuthoritativeState(AFGHoverPack* Pack)
{
	if (!IsValid(Pack) || !Pack->HasAuthority() || Pack->GetCurrentRailroadTrack())
	{
		return;
	}

	const FStructuralHoverpackSession* Session = Sessions.Find(Pack);
	if (!Session || !ShouldPublishSessionToVanilla(Pack, *Session))
	{
		return;
	}

	FStructuralHoverpackSession* MutableSession = Sessions.Find(Pack);

	// Vanilla UpdateCurrentConnectionLocation clobbers structural anchors when a plug is set.
	if (Session->Source == EStructuralHoverpackTetherSource::StructuralGeometry)
	{
		if (MutableSession)
		{
			SyncStructuralVirtualTether(Pack, *MutableSession);
		}
		WriteDisconnectionTimer(Pack, Session->DisconnectionTimer);
		return;
	}

	if (!Pack->HasConnection() || Pack->GetCurrentPowerConnection() != Session->Feed.Get())
	{
		if (MutableSession)
		{
			SyncReplicationFields(Pack, *MutableSession);
		}
	}

	SetConnectionLocation(Pack, Session->Anchor);
	WriteSearchRadius(Pack, GetVanillaFieldRadius(*Session));
	WriteDisconnectionTimer(Pack, Session->DisconnectionTimer);
}

void FStructuralHoverpackBridge::BeforeTick(AFGHoverPack* Pack, float DeltaTime)
{
	if (!OwnsConnectivity(Pack) || !Pack->HasAuthority())
	{
		return;
	}

	MaintainSession(Pack, DeltaTime);
}

void FStructuralHoverpackBridge::AfterTick(AFGHoverPack* Pack, float DeltaTime)
{
	(void)DeltaTime;

	if (!OwnsConnectivity(Pack) || !Pack->HasAuthority())
	{
		return;
	}

	RestoreAuthoritativeState(Pack);
}

void FStructuralHoverpackBridge::OnUnEquip(AFGHoverPack* Pack)
{
	if (!IsValid(Pack))
	{
		return;
	}

	if (Sessions.Contains(Pack))
	{
		ClearSession(Pack, /*bDisconnectPower=*/true, TEXT("tether_unequip"));
	}
}
