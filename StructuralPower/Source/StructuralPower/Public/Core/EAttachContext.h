// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

enum class EAttachContext : uint8
{
	BulkLoad,
	RuntimePlace,
	WireDelta,
	Toggle,
};

STRUCTURALPOWER_API bool IsBulkLoadAttachContext(EAttachContext Context);

STRUCTURALPOWER_API bool ShouldSkipRuntimeRestitch(EAttachContext Context);

STRUCTURALPOWER_API EAttachContext AttachContextFromBulkDrain(bool bBulkLoadDrainActive);

STRUCTURALPOWER_API const TCHAR* AttachContextToString(EAttachContext Context);
