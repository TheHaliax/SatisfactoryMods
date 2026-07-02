// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Components/UFGStructuralPowerConnectionComponent.h"

#include "StructuralPowerConstants.h"

UFGStructuralPowerConnectionComponent::UFGStructuralPowerConnectionComponent()
{
	SetIsHidden(true);
	mMaxNumConnectionLinks = StructuralPowerConstants::MaxHiddenConnectionLinks;
}
