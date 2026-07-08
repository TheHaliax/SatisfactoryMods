// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Processors/FStructuralPowerGeneratorProcessor.h"

#include "Buildables/FGBuildableGenerator.h"
#include "Core/FStructuralPowerContext.h"
#include "Routing/FStructuralPowerRouter.h"

void FStructuralPowerGeneratorProcessor::Process(
	FStructuralPowerContext& /*Ctx*/,
	AFGBuildableGenerator* /*Generator*/)
{
	if (!FStructuralPowerRouter::IsStructuralGeneratorRoutingEnabled())
	{
		return;
	}
}

void FStructuralPowerGeneratorProcessor::TearDown(
	FStructuralPowerContext& /*Ctx*/,
	AFGBuildableGenerator* /*Generator*/)
{
	if (!FStructuralPowerRouter::IsStructuralGeneratorRoutingEnabled())
	{
		return;
	}
}
