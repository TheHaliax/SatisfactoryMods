// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Routing/EStructuralChannel.h"

const TCHAR* StructuralChannelToString(EStructuralChannel Tag)
{
	switch (Tag)
	{
	case EStructuralChannel::Structure: return TEXT("Structure");
	case EStructuralChannel::Light: return TEXT("Light");
	case EStructuralChannel::Generator: return TEXT("Generator");
	case EStructuralChannel::Extractor: return TEXT("Extractor");
	case EStructuralChannel::Manufacturer: return TEXT("Manufacturer");
	case EStructuralChannel::Transport: return TEXT("Transport");
	case EStructuralChannel::Misc: return TEXT("Misc");
	case EStructuralChannel::Switch: return TEXT("Switch");
	default: return TEXT("?");
	}
}
