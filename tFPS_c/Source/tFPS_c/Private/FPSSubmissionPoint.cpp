#include "FPSSubmissionPoint.h"
#include "FPSCharacter.h"
#include "FPSPlayerState.h"
#include "FPSInventoryComponent.h"
#include "FPSItemDef.h"
#include "Components/SphereComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "TimerManager.h"

AFPSSubmissionPoint::AFPSSubmissionPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	DefaultRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultRoot"));
	SetRootComponent(DefaultRoot);

	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(DefaultRoot);
	InteractionSphere->InitSphereRadius(InteractionRadius);
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionSphere->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractionSphere->SetGenerateOverlapEvents(true);
}

void AFPSSubmissionPoint::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AFPSSubmissionPoint, SubmissionState);
}

void AFPSSubmissionPoint::BeginPlay()
{
	Super::BeginPlay();

	InteractionSphere->SetSphereRadius(InteractionRadius);
	InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &AFPSSubmissionPoint::OnSphereBeginOverlap);
	InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &AFPSSubmissionPoint::OnSphereEndOverlap);

	if (HasAuthority())
		ScheduleNextStateChange();
}

void AFPSSubmissionPoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (InteractionSphere)
	{
		TArray<AActor*> Overlapping;
		InteractionSphere->GetOverlappingActors(Overlapping, AFPSCharacter::StaticClass());
		for (AActor* A : Overlapping)
		{
			if (AFPSCharacter* C = Cast<AFPSCharacter>(A))
				C->ClearSubmissionTarget(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------
// State machine (server only)
// ---------------------------------------------------------------------------

void AFPSSubmissionPoint::SetOpen(bool bOpen)
{
	if (!HasAuthority())
		return;

	const EFPSSubmissionState NewState = bOpen ? EFPSSubmissionState::Open : EFPSSubmissionState::Closed;
	if (SubmissionState == NewState)
		return;

	SubmissionState = NewState;
	ScheduleNextStateChange();
	// UpdateOverlappingCharacters + OnSubmissionStateChanged are handled in OnRep_SubmissionState,
	// which fires on both clients AND the listen-server host when the variable changes.
}

void AFPSSubmissionPoint::ScheduleNextStateChange()
{
	if (!HasAuthority())
		return;

	const bool bCurrentlyOpen = IsOpen();
	const float MinTime = bCurrentlyOpen ? MinOpenTime : MinClosedTime;
	const float MaxTime = bCurrentlyOpen ? MaxOpenTime : MaxClosedTime;
	const float Delay = FMath::FRandRange(MinTime, MaxTime);

	GetWorld()->GetTimerManager().SetTimer(StateTimer, [this, bCurrentlyOpen]()
	{
		if (HasAuthority())
			SetOpen(!bCurrentlyOpen);
	}, Delay, false);
}

void AFPSSubmissionPoint::UpdateOverlappingCharacters()
{
	if (!InteractionSphere)
		return;

	TArray<AActor*> Overlapping;
	InteractionSphere->GetOverlappingActors(Overlapping, AFPSCharacter::StaticClass());

	if (IsOpen())
	{
		for (AActor* A : Overlapping)
		{
			if (AFPSCharacter* C = Cast<AFPSCharacter>(A))
				C->SetSubmissionTarget(this);
		}
	}
	else
	{
		for (AActor* A : Overlapping)
		{
			if (AFPSCharacter* C = Cast<AFPSCharacter>(A))
				C->ClearSubmissionTarget(this);
		}
	}
}

// ---------------------------------------------------------------------------
// Replication
// ---------------------------------------------------------------------------

void AFPSSubmissionPoint::OnRep_SubmissionState()
{
	UpdateOverlappingCharacters();
	OnSubmissionStateChanged(IsOpen());
}

// ---------------------------------------------------------------------------
// Overlap
// ---------------------------------------------------------------------------

void AFPSSubmissionPoint::OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep)
{
	if (!IsOpen())
		return;

	if (AFPSCharacter* C = Cast<AFPSCharacter>(OtherActor))
		C->SetSubmissionTarget(this);
}

void AFPSSubmissionPoint::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (AFPSCharacter* C = Cast<AFPSCharacter>(OtherActor))
		C->ClearSubmissionTarget(this);
}

// ---------------------------------------------------------------------------
// Submission (server only, called from Character RPC)
// ---------------------------------------------------------------------------

float AFPSSubmissionPoint::SubmitAllValuables(AFPSCharacter* Submitter)
{
	if (!HasAuthority() || !Submitter || !IsOpen())
		return 0.0f;

	UFPSInventoryComponent* Inv = Submitter->GetInventory();
	if (!Inv)
		return 0.0f;

	const TArray<FInventoryEntry>& Items = Inv->GetItems();
	TArray<int32> ToRemove;
	float TotalValue = 0.0f;

	for (int32 i = 0; i < Items.Num(); ++i)
	{
		const FInventoryEntry& Entry = Items[i];
		if (!Entry.ItemDef || !Entry.ItemDef->IsA<UFPSValuableItemDef>())
			continue;

		TotalValue += Entry.ItemDef->GetCurrentValue(Entry);
		ToRemove.Add(i);
	}

	if (ToRemove.Num() == 0)
		return 0.0f;

	for (int32 i = ToRemove.Num() - 1; i >= 0; --i)
		Inv->ServerRemoveItemAt(ToRemove[i]);

	if (AFPSPlayerState* PS = Submitter->GetPlayerState<AFPSPlayerState>())
		PS->AddCarryValue(TotalValue);

	return TotalValue;
}

float AFPSSubmissionPoint::SubmitSingleItem(AFPSCharacter* Submitter, int32 InventoryIndex)
{
	if (!HasAuthority() || !Submitter || !IsOpen())
		return 0.0f;

	UFPSInventoryComponent* Inv = Submitter->GetInventory();
	if (!Inv)
		return 0.0f;

	const TArray<FInventoryEntry>& Items = Inv->GetItems();
	if (!Items.IsValidIndex(InventoryIndex))
		return 0.0f;

	const FInventoryEntry& Entry = Items[InventoryIndex];
	if (!Entry.ItemDef || !Entry.ItemDef->IsA<UFPSValuableItemDef>())
		return 0.0f;

	const float Value = (float)Entry.ItemDef->GetCurrentValue(Entry);
	if (Value <= 0.0f)
		return 0.0f;

	Inv->ServerRemoveItemAt(InventoryIndex);

	if (AFPSPlayerState* PS = Submitter->GetPlayerState<AFPSPlayerState>())
		PS->AddCarryValue(Value);

	return Value;
}
