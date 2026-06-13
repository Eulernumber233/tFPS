#include "FPSPickup.h"
#include "FPSItemDef.h"
#include "FPSCharacter.h"
#include "FPSInventoryComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"

AFPSPickup::AFPSPickup()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	// C++ 空 Root（同 AFPSWeapon）：蓝图子类在其下挂 Mesh。
	DefaultRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultRoot"));
	SetRootComponent(DefaultRoot);

	// 地上网格体：OnConstruction 从 ItemDef->PickupMesh 自动赋值。
	// 专用蓝图子类如不需要此组件，可在蓝图层隐藏/替换。
	PickupMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
	PickupMeshComponent->SetupAttachment(DefaultRoot);
	PickupMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

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
}

void AFPSPickup::BeginPlay()
{
	Super::BeginPlay();

	PickupSphere->SetSphereRadius(PickupRadius);
	PickupSphere->OnComponentBeginOverlap.AddDynamic(this, &AFPSPickup::OnSphereBeginOverlap);
	PickupSphere->OnComponentEndOverlap.AddDynamic(this, &AFPSPickup::OnSphereEndOverlap);

	// 运行时也刷一次：OnConstruction 可能只在服务端 SpawnActor 时跑过，
	// 客户端 BeginPlay 补一次保证网格体在各端都显示。
	ApplyPickupMeshFromItemDef();
}

void AFPSPickup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	ApplyPickupMeshFromItemDef();
}

void AFPSPickup::ApplyPickupMeshFromItemDef()
{
	if (!PickupMeshComponent || !ItemDef || ItemDef->PickupMesh.IsNull())
		return;

	UStaticMesh* Mesh = ItemDef->PickupMesh.LoadSynchronous();
	if (Mesh)
	{
		PickupMeshComponent->SetStaticMesh(Mesh);
		PickupMeshComponent->SetVisibility(true);
	}
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
		return;

	UFPSInventoryComponent* Inv = Picker->GetInventory();
	if (!Inv)
		return;

	// 丢弃落地的 Pickup 带原耐久/数量（bHasOverride）→ 走 ServerAddEntry 还原状态；
	// 地图手摆的 Pickup → 走 ServerAddItem 用 DefaultValue。
	bool bAdded;
	if (bHasOverride)
	{
		FInventoryEntry Entry = OverrideEntry;
		Entry.ItemDef = ItemDef;   // 以 Pickup 自己的 ItemDef 为准
		bAdded = Inv->ServerAddEntry(Entry);
	}
	else
	{
		bAdded = Inv->ServerAddItem(ItemDef);
	}

	if (bAdded)
	{
		// 进背包成功 → 销毁地上的 Pickup（复制销毁，所有端同步消失）。
		Destroy();
	}
	else
	{
		// 背包满：保留在地上，通知蓝图做提示（"背包已满"）。
		OnPickupBlocked();
	}
}
