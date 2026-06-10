#include "FPSInteractionComponent.h"

UFPSInteractionComponent::UFPSInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UFPSInteractionComponent::RegisterInteraction(AActor* Source, EFPSInteractionType Type,
	const FText& Prompt, int32 Priority)
{
	if (!Source)
		return;

	for (const FFPSInteractionEntry& E : Entries)
	{
		if (E.Source == Source)
			return;
	}

	FFPSInteractionEntry Entry;
	Entry.Source = Source;
	Entry.Type = Type;
	Entry.PromptText = Prompt;
	Entry.Priority = Priority;

	InsertSorted(Entry);

	if (Entries.Num() == 1)
		SelectedIndex = 0;

	OnInteractionsChanged.Broadcast();
}

void UFPSInteractionComponent::UnregisterInteraction(AActor* Source)
{
	if (!Source)
		return;

	int32 RemovedIndex = INDEX_NONE;
	for (int32 i = 0; i < Entries.Num(); ++i)
	{
		if (Entries[i].Source == Source)
		{
			RemovedIndex = i;
			break;
		}
	}

	if (RemovedIndex == INDEX_NONE)
		return;

	Entries.RemoveAt(RemovedIndex);

	if (Entries.Num() == 0)
	{
		SelectedIndex = INDEX_NONE;
	}
	else if (RemovedIndex <= SelectedIndex)
	{
		SelectedIndex = FMath::Min(SelectedIndex, Entries.Num() - 1);
	}

	OnInteractionsChanged.Broadcast();
}

void UFPSInteractionComponent::CycleNext()
{
	if (Entries.Num() == 0)
		return;

	SelectedIndex = (SelectedIndex + 1) % Entries.Num();
	OnInteractionsChanged.Broadcast();
}

void UFPSInteractionComponent::CyclePrev()
{
	if (Entries.Num() == 0)
		return;

	SelectedIndex = (SelectedIndex - 1 + Entries.Num()) % Entries.Num();
	OnInteractionsChanged.Broadcast();
}

AActor* UFPSInteractionComponent::GetActiveTarget() const
{
	return Entries.IsValidIndex(SelectedIndex) ? Entries[SelectedIndex].Source : nullptr;
}

EFPSInteractionType UFPSInteractionComponent::GetActiveType() const
{
	return Entries.IsValidIndex(SelectedIndex)
		? Entries[SelectedIndex].Type
		: EFPSInteractionType::Pickup;
}

FText UFPSInteractionComponent::GetActivePrompt() const
{
	return Entries.IsValidIndex(SelectedIndex) ? Entries[SelectedIndex].PromptText : FText::GetEmpty();
}

void UFPSInteractionComponent::InsertSorted(const FFPSInteractionEntry& Entry)
{
	for (int32 i = 0; i < Entries.Num(); ++i)
	{
		if (Entry.Priority > Entries[i].Priority)
		{
			Entries.Insert(Entry, i);
			if (i <= SelectedIndex)
				++SelectedIndex;
			return;
		}
	}
	Entries.Add(Entry);
}
