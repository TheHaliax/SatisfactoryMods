// SPDX-FileCopyrightText: 2026 Haliax
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Circuit/FStructuralHiddenConnectionUtil.h"

#include "FGCircuitConnectionComponent.h"
#include "UObject/UnrealType.h"

namespace
{
FArrayProperty* GetHiddenConnectionsArrayProperty()
{
	return FindFProperty<FArrayProperty>(
		UFGCircuitConnectionComponent::StaticClass(),
		TEXT("mHiddenConnections"));
}

bool AppendHiddenConnectionEntry(
	UFGCircuitConnectionComponent* Owner,
	UFGCircuitConnectionComponent* Other)
{
	FArrayProperty* ArrayProperty = GetHiddenConnectionsArrayProperty();
	if (!ArrayProperty || !IsValid(Owner) || !IsValid(Other))
	{
		return false;
	}

	void* ArrayPtr = ArrayProperty->ContainerPtrToValuePtr<void>(Owner);
	FScriptArrayHelper Helper(ArrayProperty, ArrayPtr);

	for (int32 Index = 0; Index < Helper.Num(); ++Index)
	{
		UFGCircuitConnectionComponent* const* Existing =
			reinterpret_cast<UFGCircuitConnectionComponent* const*>(
				Helper.GetRawPtr(Index));
		if (Existing && *Existing == Other)
		{
			return false;
		}
	}

	const int32 NewIndex = Helper.AddValue();
	*reinterpret_cast<TObjectPtr<UFGCircuitConnectionComponent>*>(
		Helper.GetRawPtr(NewIndex)) = Other;
	return true;
}

bool RemoveHiddenConnectionEntry(
	UFGCircuitConnectionComponent* Owner,
	UFGCircuitConnectionComponent* Other)
{
	FArrayProperty* ArrayProperty = GetHiddenConnectionsArrayProperty();
	if (!ArrayProperty || !IsValid(Owner) || !IsValid(Other))
	{
		return false;
	}

	void* ArrayPtr = ArrayProperty->ContainerPtrToValuePtr<void>(Owner);
	FScriptArrayHelper Helper(ArrayProperty, ArrayPtr);

	for (int32 Index = 0; Index < Helper.Num(); ++Index)
	{
		UFGCircuitConnectionComponent* const* Existing =
			reinterpret_cast<UFGCircuitConnectionComponent* const*>(
				Helper.GetRawPtr(Index));
		if (Existing && *Existing == Other)
		{
			Helper.RemoveValues(Index);
			return true;
		}
	}

	return false;
}
}

bool FStructuralHiddenConnectionUtil::AddMeshOnlyHiddenLink(
	UFGCircuitConnectionComponent* A,
	UFGCircuitConnectionComponent* B)
{
	if (!IsValid(A) || !IsValid(B) || A == B)
	{
		return false;
	}

	if (!A->IsHidden() && !B->IsHidden())
	{
		return false;
	}

	if (A->HasHiddenConnection(B))
	{
		return false;
	}

	bool bAdded = false;
	if (A->IsHidden())
	{
		bAdded |= AppendHiddenConnectionEntry(A, B);
	}
	if (B->IsHidden())
	{
		bAdded |= AppendHiddenConnectionEntry(B, A);
	}

	return bAdded;
}

bool FStructuralHiddenConnectionUtil::RemoveMeshOnlyHiddenLink(
	UFGCircuitConnectionComponent* A,
	UFGCircuitConnectionComponent* B)
{
	if (!IsValid(A) || !IsValid(B) || A == B)
	{
		return false;
	}

	bool bRemoved = false;
	if (A->IsHidden())
	{
		bRemoved |= RemoveHiddenConnectionEntry(A, B);
	}
	if (B->IsHidden())
	{
		bRemoved |= RemoveHiddenConnectionEntry(B, A);
	}

	return bRemoved;
}
