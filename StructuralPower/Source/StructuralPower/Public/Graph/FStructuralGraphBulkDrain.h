// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class STRUCTURALPOWER_API FStructuralGraphBulkDrain
{
public:
	FStructuralGraphBulkDrain() = default;

	void Bind(class FStructuralGraphSession* InSession);

	void FinishBulkLoadDrain();

private:
	class FStructuralGraphSession* Session = nullptr;
};
