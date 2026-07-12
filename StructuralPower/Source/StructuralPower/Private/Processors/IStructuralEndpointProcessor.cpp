// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Processors/IStructuralEndpointProcessor.h"

TUniquePtr<IStructuralConnectionPoint> IStructuralEndpointProcessor::MakeConnectionPoint(
	FStructuralGraphSession& /*Session*/,
	AFGBuildable* /*Host*/)
{
	return nullptr;
}
