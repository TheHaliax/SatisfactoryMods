// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"

namespace FPCMetallicStyle
{
inline FSoftClassPath MetallicFinishPath()
{
	return FSoftClassPath(TEXT("/Script/PipelineColor.UPCFinish_MetallicColor"));
}

inline bool IsMetallicFinishPath(const FSoftClassPath& Path)
{
	if (!Path.IsValid())
	{
		return false;
	}
	const FString Asset = Path.GetAssetName();
	return Asset.Contains(TEXT("MetallicColor"), ESearchCase::IgnoreCase);
}
} // namespace FPCMetallicStyle
