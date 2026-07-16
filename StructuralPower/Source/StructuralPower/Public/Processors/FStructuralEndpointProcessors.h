// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class FStructuralEndpointCatalog;
class IStructuralEndpointProcessor;

class STRUCTURALPOWER_API FStructuralEndpointProcessors {
 public:
  static void RegisterAll(FStructuralEndpointCatalog& Catalog);
  static void InitializeRegistries();

  static IStructuralEndpointProcessor& Pole();
  static IStructuralEndpointProcessor& Storage();
  static IStructuralEndpointProcessor& Switch();
  static IStructuralEndpointProcessor& Light();
  static IStructuralEndpointProcessor& Panel();
  static IStructuralEndpointProcessor& Generator();
};
