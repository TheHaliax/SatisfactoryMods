// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Misc/AutomationTest.h"

#include "Routing/FStructuralPowerRouter.h"
#include "StructuralPowerConstants.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStructuralRouterSentinelTest,
	"StructuralPower.Router.ReservedSentinel",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FStructuralRouterSentinelTest::RunTest(const FString& Parameters)
{
	TestTrue(
		TEXT("control_unconfigured_reserved"),
		FStructuralPowerRouter::IsReservedSentinel(StructuralPowerConstants::ControlUnconfigured));
	TestFalse(TEXT("none_ok"), FStructuralPowerRouter::IsReservedSentinel(NAME_None));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStructuralRouterKeyedJoinGateTest,
	"StructuralPower.Router.KeyedJoinRequiresSource",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FStructuralRouterKeyedJoinGateTest::RunTest(const FString& Parameters)
{
	FStructuralComponentKey CompKey;
	TestTrue(
		TEXT("publisher_control_matches_joiner_source"),
		FStructuralPowerRouter::ShouldRouteKeyedJoin(TEXT("A"), TEXT("A"), CompKey, CompKey));
	TestFalse(
		TEXT("mismatched_source"),
		FStructuralPowerRouter::ShouldRouteKeyedJoin(TEXT("A"), TEXT("B"), CompKey, CompKey));
	return true;
}
