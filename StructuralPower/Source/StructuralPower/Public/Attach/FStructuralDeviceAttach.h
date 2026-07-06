// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "Routing/EStructuralChannel.h"

class AFGBuildable;
class AFGBuildableLightSource;
class AStructuralPowerGraphSubsystem;
class UFGPowerConnectionComponent;

/** M3 consumer attach — shared primitive for lights and later factory categories. */
class STRUCTURALPOWER_API FStructuralDeviceAttach
{
public:
	static UFGPowerConnectionComponent* FindLightWireConnection(const AFGBuildableLightSource* Light);

	static bool TryAttachConsumer(
		AStructuralPowerGraphSubsystem& Graph,
		AFGBuildable* Device,
		UFGPowerConnectionComponent* DevicePlug,
		int32 ComponentRoot,
		const FStructuralChannelKey& DeviceKey);

	static void TearDownConsumerLinks(UFGPowerConnectionComponent* DevicePlug);
};
