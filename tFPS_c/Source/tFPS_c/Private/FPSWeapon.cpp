#include "FPSWeapon.h"
#include "FPSCharacter.h"
#include "FPSInventoryComponent.h"
#include "FPSItemDef.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h"
#include "DrawDebugHelpers.h"
#include "Net/UnrealNetwork.h"

#define COLLISION_WEAPON ECC_GameTraceChannel1

AFPSWeapon::AFPSWeapon()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bNetLoadOnClient = true;

	bIsReloading = false;
	bReloadCompleted = false;
	bIsOnGround = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultRoot"));
	RootComponent = Root;

	PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
	PickupSphere->SetupAttachment(RootComponent);
	PickupSphere->InitSphereRadius(PickupRadius);
	PickupSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupSphere->SetCollisionObjectType(ECC_WorldDynamic);
	PickupSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	PickupSphere->SetGenerateOverlapEvents(false);
}

void AFPSWeapon::BeginPlay()
{
	Super::BeginPlay();

	PickupSphere->SetSphereRadius(PickupRadius);
	PickupSphere->OnComponentBeginOverlap.AddDynamic(this, &AFPSWeapon::OnPickupSphereBeginOverlap);
	PickupSphere->OnComponentEndOverlap.AddDynamic(this, &AFPSWeapon::OnPickupSphereEndOverlap);

	if (bIsOnGround)
	{
		PickupSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		PickupSphere->SetGenerateOverlapEvents(true);
	}
}

void AFPSWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AFPSWeapon, bIsFiring, COND_None);
	DOREPLIFETIME_CONDITION(AFPSWeapon, OwningCharacter, COND_None);
	DOREPLIFETIME_CONDITION(AFPSWeapon, LoadedAmmo, COND_None);
	DOREPLIFETIME_CONDITION(AFPSWeapon, bIsReloading, COND_None);
	DOREPLIFETIME_CONDITION(AFPSWeapon, bReloadCompleted, COND_None);
	DOREPLIFETIME_CONDITION(AFPSWeapon, bIsOnGround, COND_None);
}

void AFPSWeapon::SetOwningCharacter(AFPSCharacter* InOwnerCharacter)
{
	OwningCharacter = InOwnerCharacter;
	SetOwner(InOwnerCharacter);
}

bool AFPSWeapon::IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget,
	const FVector& SrcLocation) const
{
	if (bIsOnGround)
		return true;
	if (OwningCharacter)
		return OwningCharacter->IsNetRelevantFor(RealViewer, ViewTarget, SrcLocation);
	return Super::IsNetRelevantFor(RealViewer, ViewTarget, SrcLocation);
}

// ===================================================================
// Pickup / Drop
// ===================================================================

void AFPSWeapon::PlaceInWorld(const FVector& Location)
{
	if (!HasAuthority())
		return;

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetOwner(nullptr);
	OwningCharacter = nullptr;
	SetActorLocation(Location);
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);

	bIsOnGround = true;
	PickupSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupSphere->SetGenerateOverlapEvents(true);
}

void AFPSWeapon::RemoveFromWorld()
{
	if (!HasAuthority())
		return;

	bIsOnGround = false;
	PickupSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PickupSphere->SetGenerateOverlapEvents(false);
}

void AFPSWeapon::OnPickupSphereBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep)
{
	if (!bIsOnGround)
		return;
	if (AFPSCharacter* C = Cast<AFPSCharacter>(OtherActor))
		C->SetWeaponPickupTarget(this);
}

void AFPSWeapon::OnPickupSphereEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!bIsOnGround)
		return;
	if (AFPSCharacter* C = Cast<AFPSCharacter>(OtherActor))
		C->ClearWeaponPickupTarget(this);
}

// ===================================================================
// Firing
// ===================================================================

void AFPSWeapon::StartFire()
{
	if (!HasAuthority())
		return;

	if (!OwningCharacter || OwningCharacter->IsDead())
		return;

	if (bIsReloading || GetCurrentAmmo() <= 0)
		return;

	bIsFiring = true;
	HandleFire();
	GetWorldTimerManager().SetTimer(FireTimerHandle, this, &AFPSWeapon::HandleFire, FireRate, true);
}

void AFPSWeapon::StopFire()
{
	if (!HasAuthority())
		return;

	bIsFiring = false;
	GetWorldTimerManager().ClearTimer(FireTimerHandle);
}

void AFPSWeapon::HandleFire()
{
	if (!HasAuthority())
		return;

	if (!OwningCharacter || OwningCharacter->IsDead())
	{
		StopFire();
		return;
	}

	if (LoadedAmmo.Num() == 0 || GetCurrentAmmo() <= 0)
	{
		StopFire();
		return;
	}

	// 伤害 = 武器基础 + 弹夹顶部弹药加成（当前要发射的那发）
	const float AmmoBonus = (LoadedAmmo[0].AmmoDef ? LoadedAmmo[0].AmmoDef->BaseDamage : 0.0f);
	const float FinalDamage = DamageAmount + AmmoBonus;

	// 从弹夹顶部消耗一发
	LoadedAmmo[0].Count--;
	if (LoadedAmmo[0].Count <= 0)
		LoadedAmmo.RemoveAt(0);

	OnRep_Ammo();

	FVector ViewLocation;
	FRotator ViewRotation;
	GetAimViewPoint(ViewLocation, ViewRotation);

	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * FireRangeEnd;
	const FVector TraceStart = ViewLocation + ViewRotation.Vector() * FireRangeStart;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(OwningCharacter);
	QueryParams.AddIgnoredActor(this);
	QueryParams.bTraceComplex = false;
	QueryParams.bReturnPhysicalMaterial = true;

	FHitResult Hit;
	const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
		Hit, TraceStart, TraceEnd, COLLISION_WEAPON, QueryParams);

	AFPSCharacter* Victim = nullptr;
	if (bBlocked)
	{
		if (AActor* HitActor = Hit.GetActor())
		{
			Victim = Cast<AFPSCharacter>(HitActor);

			AController* InstigatorController = OwningCharacter->GetController();
			FPointDamageEvent DamageEvent(FinalDamage, Hit, -ViewRotation.Vector(), nullptr);
			HitActor->TakeDamage(FinalDamage, DamageEvent, InstigatorController, OwningCharacter);
		}
	}

	const FVector AuthEnd = bBlocked ? Hit.ImpactPoint : TraceEnd;
	MulticastDrawAuthTrace(TraceStart, AuthEnd);

	if (bBlocked)
		MulticastHitConfirmed(Hit.ImpactPoint, Hit.ImpactNormal, FinalDamage, Victim);
}

void AFPSWeapon::GetAimViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	if (!OwningCharacter)
		return;

	if (AController* C = OwningCharacter->GetController())
	{
		FRotator UnusedRot;
		C->GetPlayerViewPoint(OutLocation, UnusedRot);
		OutRotation = C->GetControlRotation();
		return;
	}

	OutLocation = OwningCharacter->GetActorLocation()
		+ FVector(0, 0, OwningCharacter->BaseEyeHeight);
	OutRotation = FRotator(OwningCharacter->GetArmPitch(),
		OwningCharacter->GetActorRotation().Yaw, 0);
}

void AFPSWeapon::GetAimRay(FVector& OutStart, FVector& OutDirection) const
{
	FRotator Rot;
	GetAimViewPoint(OutStart, Rot);
	OutDirection = Rot.Vector();
}

void AFPSWeapon::MulticastDrawAuthTrace_Implementation(FVector AuthTraceStart, FVector AuthTraceEnd)
{
	if (!bDrawAuthoritativeTrace || !GetWorld())
		return;

	const FVector DrawStart = AuthTraceStart
		+ (AuthTraceEnd - AuthTraceStart).GetSafeNormal() * 50.0f;
	DrawDebugLine(GetWorld(), DrawStart, AuthTraceEnd,
		FColor::Yellow, false, 0.1f, 0, 0.3f);
}

void AFPSWeapon::MulticastHitConfirmed_Implementation(FVector ImpactPoint, FVector ImpactNormal,
	float Damage, AFPSCharacter* Victim)
{
	if (OwningCharacter)
		OwningCharacter->DispatchHitFX(ImpactPoint, ImpactNormal, Damage, Victim);
}

// ===================================================================
// 弹药链系统
// 弹夹 = TArray<FWeaponAmmoEntry>（索引 0=顶部先发射，末尾=最后装入）
// 发射：从索引 0 扣一发，Count=0 则 RemoveAt(0)
// 换弹：从尾部插入新弹药类型（若与末尾同类型则合并 Count）
// ===================================================================

int32 AFPSWeapon::GetCurrentAmmo() const
{
	int32 Total = 0;
	for (const FWeaponAmmoEntry& E : LoadedAmmo)
		Total += E.Count;
	return Total;
}

int32 AFPSWeapon::GetComputedReserveAmmo() const
{
	if (!OwningCharacter)
		return 0;

	const UFPSInventoryComponent* Inv = OwningCharacter->GetInventory();
	if (!Inv || AcceptedCaliber.IsNone())
		return 0;

	int32 Total = 0;
	for (const FInventoryEntry& Entry : Inv->GetItems())
	{
		const UFPSAmmoItemDef* AmmoDef = Cast<UFPSAmmoItemDef>(Entry.ItemDef);
		if (AmmoDef && AmmoDef->Caliber == AcceptedCaliber)
			Total += Entry.Count;
	}
	return Total;
}

UFPSAmmoItemDef* AFPSWeapon::GetLoadedAmmoDef() const
{
	return LoadedAmmo.Num() > 0 ? LoadedAmmo[0].AmmoDef : nullptr;
}

void AFPSWeapon::ServerResetAmmo()
{
	if (!HasAuthority())
		return;

	GetWorldTimerManager().ClearTimer(ReloadTimerHandle);
	bReloadCompleted = false;
	bIsReloading = false;

	LoadedAmmo.Empty();
	LoadedAmmo.Add({nullptr, MagSize});

	OnRep_Ammo();
	OnRep_IsReloading();
}

void AFPSWeapon::SetLoadedAmmoState(const TArray<FWeaponAmmoEntry>& InLoadedAmmo)
{
	if (!HasAuthority())
		return;

	LoadedAmmo = InLoadedAmmo;
	OnRep_Ammo();
}

bool AFPSWeapon::CanReload() const
{
	return !bIsReloading && GetCurrentAmmo() < MagSize && GetComputedReserveAmmo() > 0;
}

void AFPSWeapon::ServerBeginReload()
{
	if (!HasAuthority())
		return;

	if (!OwningCharacter || OwningCharacter->IsDead())
		return;

	if (!CanReload())
		return;

	StopFire();

	bReloadCompleted = false;
	bIsReloading = true;
	OnRep_IsReloading();

	GetWorldTimerManager().SetTimer(ReloadTimerHandle, this,
		&AFPSWeapon::FinishReload, ReloadTime, false);
}

void AFPSWeapon::FinishReload()
{
	if (!HasAuthority())
		return;

	UFPSInventoryComponent* Inv = OwningCharacter ? OwningCharacter->GetInventory() : nullptr;
	if (!Inv || AcceptedCaliber.IsNone())
	{
		bReloadCompleted = false;
		bIsReloading = false;
		OnRep_IsReloading();
		return;
	}

	const int32 Needed = MagSize - GetCurrentAmmo();
	if (Needed <= 0)
	{
		bReloadCompleted = true;
		bIsReloading = false;
		OnRep_IsReloading();
		return;
	}

	// 从背包逐格取弹，从尾部插入弹夹（后进=后发射=插入尾部）
	int32 Remaining = Needed;
	TArray<FWeaponAmmoEntry> LoadedEntries;

	// 使用 ConsumeAmmo 分批获取不同弹药类型的消耗
	while (Remaining > 0)
	{
		UFPSAmmoItemDef* BatchDef = nullptr;
		TArray<FWeaponAmmoEntry> Batch;
		const int32 Got = Inv->ConsumeAmmo(AcceptedCaliber, Remaining, BatchDef, &Batch);
		if (Got <= 0)
			break;

		// 将 Batch 追加到 LoadedEntries（保持背包取弹顺序）
		for (const FWeaponAmmoEntry& E : Batch)
		{
			if (LoadedEntries.Num() > 0 && LoadedEntries.Last().AmmoDef == E.AmmoDef)
				LoadedEntries.Last().Count += E.Count;
			else
				LoadedEntries.Add(E);
		}

		Remaining -= Got;
	}

	// 将新弹药追加到弹夹尾部（与末尾同类型则合并）
	for (const FWeaponAmmoEntry& E : LoadedEntries)
	{
		if (LoadedAmmo.Num() > 0 && LoadedAmmo.Last().AmmoDef == E.AmmoDef)
			LoadedAmmo.Last().Count += E.Count;
		else
			LoadedAmmo.Add(E);
	}

	bReloadCompleted = true;
	bIsReloading = false;

	OnRep_Ammo();
	OnRep_IsReloading();

	Inv->MarkItemsDirty();
}

void AFPSWeapon::OnRep_Ammo()
{
	OnAmmoChanged.Broadcast(GetCurrentAmmo(), GetComputedReserveAmmo());
}

void AFPSWeapon::OnRep_IsReloading()
{
	if (bIsReloading)
		OnReloadStart();
	else
		OnReloadFinish();

	if (OwningCharacter)
	{
		if (bIsReloading)
			OwningCharacter->BeginProgress(EFPSProgressType::Reload, ReloadTime);
		else
			OwningCharacter->EndProgress(EFPSProgressType::Reload, bReloadCompleted);
	}
}
