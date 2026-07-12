// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Misc/AutomationTest.h"

#include "Graph/FStructuralEndpointTypes.h"
#include "Processors/FStructuralEndpointCatalog.h"
#include "Processors/FStructuralEndpointProcessors.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStructuralCatalogCompletenessTest,
	"StructuralPower.Catalog.AllKindsRegistered",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FStructuralCatalogCompletenessTest::RunTest(const FString& Parameters)
{
	FStructuralEndpointProcessors::InitializeRegistries();
	const FStructuralEndpointCatalog& Catalog = FStructuralEndpointCatalog::Get();

	TestTrue(TEXT("pole"), Catalog.Find(EStructuralEndpointKind::Pole) != nullptr);
	TestTrue(TEXT("switch"), Catalog.Find(EStructuralEndpointKind::Switch) != nullptr);
	TestTrue(TEXT("light"), Catalog.Find(EStructuralEndpointKind::Light) != nullptr);
	TestTrue(TEXT("panel"), Catalog.Find(EStructuralEndpointKind::Panel) != nullptr);
	TestTrue(TEXT("storage"), Catalog.Find(EStructuralEndpointKind::Storage) != nullptr);
	TestTrue(TEXT("generator"), Catalog.Find(EStructuralEndpointKind::Generator) != nullptr);
	TestEqual(TEXT("kind_count"), Catalog.GetRegisteredKindCount(), 6);
	return true;
}
