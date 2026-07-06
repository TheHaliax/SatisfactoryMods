// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Attach/FStructuralDeviceAttach.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableLightSource.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Config/FStructuralPowerModConfig.h"
#include "FGPowerConnectionComponent.h"
#include "FGPowerInfoComponent.h"
#include "Routing/FStructuralPowerRouter.h"
#include "Save/AStructuralPowerGraphSubsystem.h"

UFGPowerConnectionComponent* FStructuralDeviceAttach::FindLightWireConnection(
	const AFGBuildableLightSource* Light)
{
	if (!IsValid(Light))
	{
		return nullptr;
	}

	TInlineComponentArray<UFGPowerInfoComponent*> Infos(Light);
	UFGPowerInfoComponent* const PowerInfo = Infos.Num() > 0 ? Infos[0] : nullptr;

	TInlineComponentArray<UFGPowerConnectionComponent*> Conns(Light);
	for (UFGPowerConnectionComponent* Conn : Conns)
	{
		if (!IsValid(Conn) || Conn->IsHidden())
		{
			continue;
		}

		if (PowerInfo && Conn->GetPowerInfo() == PowerInfo)
		{
			return Conn;
		}
	}

	for (UFGPowerConnectionComponent* Conn : Conns)
	{
		if (IsValid(Conn) && !Conn->IsHidden())
		{
			return Conn;
		}
	}

	return nullptr;
}

bool FStructuralDeviceAttach::TryAttachConsumer(
	AStructuralPowerGraphSubsystem& Graph,
	AFGBuildable* Device,
	UFGPowerConnectionComponent* DevicePlug,
	int32 ComponentRoot,
	const FStructuralChannelKey& DeviceKey)
{
	if (!IsValid(Device) || !IsValid(DevicePlug) || ComponentRoot == INDEX_NONE)
	{
		return false;
	}

	const FStructuralComponentKey CompKey = Graph.MakeComponentKeyForRoot(ComponentRoot);
	if (!CompKey.IsValid() || DeviceKey.Source.IsNone())
	{
		return false;
	}

	if (!FStructuralPowerModConfig::IsPowerSwitchManualGroupsEnabled())
	{
		FStructuralChannelKey FeedKey;
		FeedKey.Tag = EStructuralChannel::Light;
		FeedKey.Source = DeviceKey.Source;
		if (!FStructuralPowerRouter::ShouldRouteChannelLink(DeviceKey, FeedKey, CompKey, CompKey))
		{
			return false;
		}
	}

	UFGPowerConnectionComponent* SourcePower =
		Graph.ResolveSubnetFeedConnector(ComponentRoot, DeviceKey);
	if (!IsValid(SourcePower))
	{
		return false;
	}

	return Graph.LinkHiddenPair(DevicePlug, SourcePower);
}

void FStructuralDeviceAttach::TearDownConsumerLinks(UFGPowerConnectionComponent* DevicePlug)
{
	if (!IsValid(DevicePlug))
	{
		return;
	}

	TArray<UFGCircuitConnectionComponent*> HiddenLinks;
	DevicePlug->GetHiddenConnections(HiddenLinks);
	for (UFGCircuitConnectionComponent* OtherRaw : HiddenLinks)
	{
		DevicePlug->RemoveHiddenConnection(OtherRaw);
		if (IsValid(OtherRaw))
		{
			OtherRaw->RemoveHiddenConnection(DevicePlug);
		}
	}
}
