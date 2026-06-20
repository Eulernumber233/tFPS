#include "FPSPickup.h"
#include "FPSItemDef.h"
#include "FPSCharacter.h"
#include "FPSInventoryComponent.h"
#include "Components/SphereComponent.h"
#include "Net/UnrealNetwork.h"

AFPSPickup::AFPSPickup()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	// C++ 空 Root（同 AFPSWeapon）：蓝图子类在其下挂 Mesh。
	DefaultRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultRoot"));
	SetRootComponent(DefaultRoot);

	// 拾取范围触发器。只做 overlap 检测，不挡任何东西。
	PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
	PickupSphere->SetupAttachment(DefaultRoot);
	PickupSphere->InitSphereRadius(PickupRadius);
	PickupSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupSphere->SetCollisionObjectType(ECC_WorldDynamic);
	PickupSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	PickupSphere->SetGenerateOverlapEvents(true);
}

void AFPSPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AFPSPickup, ItemDef);
	DOREPLIFETIME(AFPSPickup, bHasOverride);
	DOREPLIFETIME(AFPSPickup, OverrideEntry);
}

void AFPSPickup::SetDroppedState(const FInventoryEntry& Entry)
{
	// 服务端在 SpawnActor 后立刻调用：记下丢弃时的耐久/数量，拾取时还原。
	if (!HasAuthority())
		return;

	bHasOverride = true;
	OverrideEntry = Entry;
	OverrideEntry.GridX = -1;   // 落地后坐标作废，捡回时重新装箱
	OverrideEntry.GridY = -1;

	// 同步 ItemDef：丢弃时 Pickup 是动态生成的，ItemDef 需从 Entry 中赋值
	ItemDef = Entry.ItemDef;
}

void AFPSPickup::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Log, TEXT("[Pickup] BeginPlay: %s | HasAuth=%d | ItemDef=%s | bHasOverride=%d | OverrideEntry.ItemDef=%s | OverrideEntry.Count=%d | OverrideEntry.Durability=%d"),
		*GetName(),
		HasAuthority() ? 1 : 0,
		ItemDef ? *ItemDef->GetName() : TEXT("null"),
		bHasOverride ? 1 : 0,
		OverrideEntry.ItemDef ? *OverrideEntry.ItemDef->GetName() : TEXT("null"),
		OverrideEntry.Count,
		OverrideEntry.Durability);

	PickupSphere->SetSphereRadius(PickupRadius);
	PickupSphere->OnComponentBeginOverlap.AddDynamic(this, &AFPSPickup::OnSphereBeginOverlap);
	PickupSphere->OnComponentEndOverlap.AddDynamic(this, &AFPSPickup::OnSphereEndOverlap);
}

void AFPSPickup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
}

void AFPSPickup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 销毁/移除前，清掉所有还把本 Pickup 当作"可拾取目标"的角色，避免悬空指针。
	if (PickupSphere)
	{
		TArray<AActor*> Overlapping;
		PickupSphere->GetOverlappingActors(Overlapping, AFPSCharacter::StaticClass());
		for (AActor* A : Overlapping)
		{
			if (AFPSCharacter* C = Cast<AFPSCharacter>(A))
				C->ClearPickupTarget(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AFPSPickup::OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep)
{
	// overlap 在所有端都会触发：每端的本地角色据此显示"按F"提示（纯本地表现）。
	if (AFPSCharacter* C = Cast<AFPSCharacter>(OtherActor))
		C->SetPickupTarget(this);
}

void AFPSPickup::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (AFPSCharacter* C = Cast<AFPSCharacter>(OtherActor))
		C->ClearPickupTarget(this);
}

void AFPSPickup::ServerTryPickup(AFPSCharacter* Picker)
{
	if (!HasAuthority() || !Picker || !ItemDef)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Pickup] ServerTryPickup EARLY RETURN: HasAuth=%d Picker=%s ItemDef=%s"),
			HasAuthority() ? 1 : 0,
			Picker ? *Picker->GetName() : TEXT("null"),
			ItemDef ? *ItemDef->GetName() : TEXT("null"));
		return;
	}

	UFPSInventoryComponent* Inv = Picker->GetInventory();
	if (!Inv)
		return;

	bool bAdded;
	// Only trust bHasOverride when SetDroppedState actually wrote OverrideEntry.
	// If OverrideEntry.ItemDef is still nullptr, bHasOverride was leaked (e.g. blueprint
	// default) and we must fall through to ServerAddItem for correct DefaultValue init.
	const bool bUseOverride = bHasOverride && OverrideEntry.ItemDef != nullptr;
	if (bUseOverride)
	{
		FInventoryEntry Entry = OverrideEntry;
		Entry.ItemDef = ItemDef;
		UE_LOG(LogTemp, Log, TEXT("[Pickup] ServerTryPickup: %s via ServerAddEntry (bHasOverride=true) | Entry.Count=%d Entry.Durability=%d"),
			*ItemDef->GetName(), Entry.Count, Entry.Durability);
		bAdded = Inv->ServerAddEntry(Entry);
	}
	else
	{
		if (bHasOverride)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Pickup] ServerTryPickup: %s bHasOverride=true but OverrideEntry.ItemDef=null — falling back to ServerAddItem. Check blueprint defaults for bHasOverride!"),
				*ItemDef->GetName());
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("[Pickup] ServerTryPickup: %s via ServerAddItem (bHasOverride=false)"),
				*ItemDef->GetName());
		}
		bAdded = Inv->ServerAddItem(ItemDef);
	}

	if (bAdded)
	{
		Destroy();
	}
	else
	{
		OnPickupBlocked();
	}
}
