// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Misc/AutomationTest.h"

#include "Rules/FStructuralEligibilityRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStructuralEligibilityNullTest,
	"StructuralPower.Eligibility.NullSafe",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FStructuralEligibilityNullTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("bus_member"), FStructuralEligibilityRules::IsBusMember(nullptr));
	TestFalse(TEXT("pole"), FStructuralEligibilityRules::IsPowerBridgePole(nullptr));
	TestFalse(TEXT("switch"), FStructuralEligibilityRules::IsPowerBridgeSwitch(nullptr));
	TestFalse(TEXT("storage"), FStructuralEligibilityRules::IsPowerStorage(nullptr));
	TestFalse(TEXT("light"), FStructuralEligibilityRules::IsStructuralLightConsumer(nullptr));
	TestFalse(TEXT("generator"), FStructuralEligibilityRules::IsStructuralGenerator(nullptr));
	return true;
}
