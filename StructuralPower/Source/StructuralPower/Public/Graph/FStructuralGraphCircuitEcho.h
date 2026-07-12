// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "CoreMinimal.h"

class AFGBuildableCircuitSwitch;
class AFGBuildableLightsControlPanel;

class STRUCTURALPOWER_API FStructuralGraphCircuitEcho
{
public:
	FStructuralGraphCircuitEcho() = default;

	void Bind(class FStructuralGraphSession* InSession);

	bool ShouldSkipPanelCircuitEcho(
		AFGBuildableLightsControlPanel* Panel,
		const TCHAR** OutReason = nullptr);
	bool ShouldSkipSwitchCircuitEcho(
		AFGBuildableCircuitSwitch* Switch,
		const TCHAR** OutReason = nullptr);
	void MarkEchoDirtyForSwitchToggle(AFGBuildableCircuitSwitch* Switch, int32 LocalRoot);
	void NotePanelCircuitEchoProcessed(AFGBuildableLightsControlPanel* Panel);
	void NotePanelToggleHandled(AFGBuildableLightsControlPanel* Panel);
	void NoteSwitchCircuitEchoProcessed(AFGBuildableCircuitSwitch* Switch);
	void NoteSwitchToggleHandled(AFGBuildableCircuitSwitch* Switch);

private:
	class FStructuralGraphSession* Session = nullptr;
};
