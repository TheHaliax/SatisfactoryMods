// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Processors/FStructuralPowerGeneratorProcessor.h"

#include "Buildables/FGBuildableGenerator.h"
#include "Routing/FStructuralPowerRouter.h"
#include "Save/AStructuralPowerGraphSubsystem.h"

void FStructuralPowerGeneratorProcessor::Process(
	AStructuralPowerGraphSubsystem& /*Graph*/,
	AFGBuildableGenerator* /*Generator*/,
	const EAttachContext /*AttachContext*/)
{
	if (!FStructuralPowerRouter::IsStructuralGeneratorRoutingEnabled())
	{
		return;
	}
}

void FStructuralPowerGeneratorProcessor::RestitchOnRoot(
	AStructuralPowerGraphSubsystem& /*Graph*/,
	const int32 /*Root*/,
	const EAttachContext /*AttachContext*/)
{
	if (!FStructuralPowerRouter::IsStructuralGeneratorRoutingEnabled())
	{
		return;
	}
}

void FStructuralPowerGeneratorProcessor::TearDown(
	AStructuralPowerGraphSubsystem& /*Graph*/,
	AFGBuildableGenerator* /*Generator*/)
{
	if (!FStructuralPowerRouter::IsStructuralGeneratorRoutingEnabled())
	{
		return;
	}
}
