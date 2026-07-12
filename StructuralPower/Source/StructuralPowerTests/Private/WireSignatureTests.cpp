// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Misc/AutomationTest.h"

#include "Processors/FStructuralSwitchWireEcho.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStructuralWireSignatureNullTest,
	"StructuralPower.Switch.WireSignatureNull",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FStructuralWireSignatureNullTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("null_switch"), FStructuralSwitchWireEcho::BuildWireSignature(nullptr), uint8(0));
	return true;
}
