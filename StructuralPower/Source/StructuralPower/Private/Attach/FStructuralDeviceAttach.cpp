// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Attach/FStructuralDeviceAttach.h"

#include "Buildables/FGBuildable.h"
#include "Buildables/FGBuildableLightSource.h"
#include "Circuit/FStructuralCircuitPromotionUtil.h"
#include "Components/UFGStructuralPowerConnectionComponent.h"
#include "Config/FStructuralPowerModConfig.h"
#include "FGPowerConnectionComponent.h"
#include "FGPowerInfoComponent.h"
#include "Routing/FStructuralPowerRouter.h"
#include "Core/FStructuralGraphSession.h"
#include "Circuit/FStructuralGraphCircuitOps.h"
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
	FStructuralGraphSession& Session,
	AFGBuildable* Device,
	UFGPowerConnectionComponent* DevicePlug,
	int32 ComponentRoot,
	const FStructuralChannelKey& DeviceKey)
{
	if (!IsValid(Device) || !IsValid(DevicePlug) || ComponentRoot == INDEX_NONE)
	{
		return false;
	}

	const FStructuralComponentKey CompKey = Session.Ids().MakeComponentKeyForRoot(ComponentRoot);
	if (!CompKey.IsValid() || DeviceKey.Source.IsNone())
	{
		return false;
	}

	UFGPowerConnectionComponent* SourcePower =
		Session.Circuit().ResolveSubnetFeedConnector(ComponentRoot, DeviceKey);
	if (!IsValid(SourcePower))
	{
		return false;
	}

	return Session.Circuit().LinkHiddenPair(DevicePlug, SourcePower);
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
		if (UFGPowerConnectionComponent* Other = Cast<UFGPowerConnectionComponent>(OtherRaw))
		{
			FStructuralCircuitPromotionUtil::DemoteHiddenCircuitLink(DevicePlug, Other);
		}
		else if (IsValid(OtherRaw))
		{
			DevicePlug->RemoveHiddenConnection(OtherRaw);
			OtherRaw->RemoveHiddenConnection(DevicePlug);
		}
	}
}
