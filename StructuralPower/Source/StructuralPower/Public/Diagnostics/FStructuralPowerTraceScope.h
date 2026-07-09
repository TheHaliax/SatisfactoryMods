// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

#ifndef ANONYMOUS_VARIABLE
#define ANONYMOUS_VARIABLE_IMPL(Name, Line) Name##Line
#define ANONYMOUS_VARIABLE(Name) ANONYMOUS_VARIABLE_IMPL(Name, __LINE__)
#endif

/** RAII scope timer — logs enter/exit when Trace is on. */
class STRUCTURALPOWER_API FStructuralPowerTraceScope
{
public:
	FStructuralPowerTraceScope(
		const TCHAR* InDomain,
		const TCHAR* InOp,
		const FString& InDetail = FString());

	~FStructuralPowerTraceScope();

	FStructuralPowerTraceScope(const FStructuralPowerTraceScope&) = delete;
	FStructuralPowerTraceScope& operator=(const FStructuralPowerTraceScope&) = delete;

private:
	const TCHAR* Domain = nullptr;
	const TCHAR* Op = nullptr;
	FString Detail;
	uint64 StartCycles = 0;
	int32 Depth = 0;
	bool bActive = false;
};

#define HALSP_TRACE_SCOPE(DomainLiteral, OpLiteral) \
	FStructuralPowerTraceScope ANONYMOUS_VARIABLE(HALSP_Scope_)(DomainLiteral, OpLiteral)

#define HALSP_TRACE_SCOPE_DETAIL(DomainLiteral, OpLiteral, DetailExpr) \
	FStructuralPowerTraceScope ANONYMOUS_VARIABLE(HALSP_Scope_)(DomainLiteral, OpLiteral, DetailExpr)
