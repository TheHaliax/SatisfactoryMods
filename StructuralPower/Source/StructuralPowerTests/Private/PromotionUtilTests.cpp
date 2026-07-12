// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Misc/AutomationTest.h"

#include "Circuit/FStructuralCircuitPromotionUtil.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStructuralPromotionUtilNullTest,
	"StructuralPower.Promotion.HiddenLinkRepairNull",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FStructuralPromotionUtilNullTest::RunTest(const FString& Parameters)
{
	TestFalse(
		TEXT("null_pair"),
		FStructuralCircuitPromotionUtil::HiddenLinkNeedsCircuitRepair(nullptr, nullptr));
	return true;
}
