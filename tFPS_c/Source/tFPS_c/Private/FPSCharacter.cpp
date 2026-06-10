#include "FPSCharacter.h"
#include "FPSWeapon.h"
#include "FPSGameMode.h"
#include "FPSPlayerState.h"
#include "FPSInventoryComponent.h"
#include "FPSInteractionComponent.h"
#include "FPSItemDef.h"
#include "FPSPickup.h"
#include "FPSSubmissionPoint.h"
#include "FPSPlayerController.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Engine/World.h"

#define COLLISION_WEAPON ECC_GameTraceChannel1

AFPSCharacter::AFPSCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	Health = MaxHealth;
	Stamina = MaxStamina;
	bIsDead = false;
	bWantsToRun = false;
	bWantsToAim = false;
	bWeaponEquipped = false;

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(COLLISION_WEAPON, ECR_Ignore);
	}

	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		MeshComp->SetCollisionObjectType(ECC_Pawn);
		MeshComp->SetCollisionResponseToAllChannels(ECR_Ignore);
		MeshComp->SetCollisionResponseToChannel(COLLISION_WEAPON, ECR_Block);
		MeshComp->SetGenerateOverlapEvents(false);
	}

	Inventory = CreateDefaultSubobject<UFPSInventoryComponent>(TEXT("Inventory"));

	InteractionManager = CreateDefaultSubobject<UFPSInteractionComponent>(TEXT("InteractionManager"));
}

AFPSWeapon* AFPSCharacter::GetWeaponInSlot(int32 Slot) const
{
	return Slot == 0 ? CurrentWeapon : (Slot == 1 ? SecondaryWeapon : nullptr);
}

AFPSWeapon* AFPSCharacter::GetActiveWeapon() const
{
	return ActiveWeaponSlot == 0 ? CurrentWeapon : SecondaryWeapon;
}

void AFPSCharacter::BeginPlay()
{
	Super::BeginPlay();

	// Weapons are only spawned on the server.
	if (HasAuthority() && WeaponClass && !CurrentWeapon)
	{
		CurrentWeapon = GetWorld()->SpawnActorDeferred<AFPSWeapon>(WeaponClass, FTransform::Identity, this, this);
		if (CurrentWeapon)
		{
			CurrentWeapon->SetOwningCharacter(this);
			CurrentWeapon->FinishSpawning(FTransform::Identity);
			CurrentWeapon->ServerResetAmmo();
		}

		if (CurrentWeapon)
		{
			GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
			{
				if (CurrentWeapon && !bWeaponEquipped)
				{
					bWeaponEquipped = true;
					OnWeaponEquipped();
				}
			});
		}
	}

	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	GetCharacterMovement()->JumpZVelocity = JumpZVelocity;

	GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
	{
		if (!IsLocallyControlled() || !InputMappingContext)
			return;

		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			if (ULocalPlayer* LP = PC->GetLocalPlayer())
			{
				if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
					ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
				{
					Subsystem->AddMappingContext(InputMappingContext, 0);
				}
			}
		}
	});
}

void AFPSCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateMovementState();

	if (HasAuthority() && !bIsDead)
	{
		UpdateStamina(DeltaTime);
		ApplyMovementSpeed();
	}

	if (!bIsDead && IsLocallyControlled() && Controller)
	{
		const float CurrentPitch = Controller->GetControlRotation().GetNormalized().Pitch;
		ArmPitch = CurrentPitch;

		if (!HasAuthority())
			ServerUpdateAimPitch(CurrentPitch);
	}

	AFPSWeapon* ActiveWeapon = GetActiveWeapon();
	WeaponFireState = (ActiveWeapon && ActiveWeapon->IsFiring())
		? EFPSWeaponFireState::Firing
		: EFPSWeaponFireState::Idle;
}

void AFPSCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (InputMappingContext)
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
				ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
			{
				Subsystem->RemoveMappingContext(InputMappingContext);
			}
		}
	}

	if (HasAuthority())
	{
		if (CurrentWeapon && !CurrentWeapon->IsOnGround())
		{
			CurrentWeapon->Destroy();
			CurrentWeapon = nullptr;
		}
		if (SecondaryWeapon && !SecondaryWeapon->IsOnGround())
		{
			SecondaryWeapon->Destroy();
			SecondaryWeapon = nullptr;
		}
	}

	Super::EndPlay(EndPlayReason);
}

bool AFPSCharacter::IsFiring() const
{
	AFPSWeapon* W = GetActiveWeapon();
	return W && W->IsFiring();
}

void AFPSCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AFPSCharacter, Health, COND_None);
	DOREPLIFETIME_CONDITION(AFPSCharacter, Stamina, COND_None);
	DOREPLIFETIME_CONDITION(AFPSCharacter, bIsDead, COND_None);
	DOREPLIFETIME_CONDITION(AFPSCharacter, CurrentWeapon, COND_None);
	DOREPLIFETIME_CONDITION(AFPSCharacter, SecondaryWeapon, COND_None);
	DOREPLIFETIME_CONDITION(AFPSCharacter, ActiveWeaponSlot, COND_None);
	DOREPLIFETIME_CONDITION(AFPSCharacter, ArmPitch, COND_None);
	DOREPLIFETIME_CONDITION(AFPSCharacter, bIsUsingItem, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AFPSCharacter, ItemUseTotalDuration, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AFPSCharacter, bItemUseCompleted, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(AFPSCharacter, HoTState, COND_OwnerOnly);
}

float AFPSCharacter::TakeDamage(float Damage, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority() || bIsDead || Health <= 0.0f)
		return 0.0f;

	float ActualDamage = Super::TakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser);
	Health = FMath::Clamp(Health - ActualDamage, 0.0f, MaxHealth);

	if (ActualDamage > 0.0f && EventInstigator && EventInstigator != GetController())
	{
		if (AFPSPlayerState* AttackerPS = EventInstigator->GetPlayerState<AFPSPlayerState>())
			AttackerPS->AddDamage(ActualDamage);
	}

	OnHealthChanged.Broadcast(Health, MaxHealth);

	if (Health <= 0.0f)
		Die(EventInstigator);

	return ActualDamage;
}

void AFPSCharacter::Die(AController* Killer)
{
	if (!HasAuthority())
		return;

	bIsDead = true;
	Health = 0.0f;

	CancelItemUse();
	DeactivateHoT();

	// Stop fire on all equipped weapons.
	if (CurrentWeapon)
		CurrentWeapon->StopFire();
	if (SecondaryWeapon)
		SecondaryWeapon->StopFire();

	GetCharacterMovement()->DisableMovement();

	SetDeathPresentation(true);
	OnDeathStart();

	if (AFPSPlayerState* KPS = Killer ? Killer->GetPlayerState<AFPSPlayerState>() : nullptr)
		KPS->AddKill();

	if (AFPSPlayerState* VPS = GetPlayerState<AFPSPlayerState>())
		VPS->AddDeath();

	// Drop the highest-value weapon, destroy the other.
	AFPSWeapon* WeaponToDrop = nullptr;
	if (CurrentWeapon && SecondaryWeapon)
	{
		WeaponToDrop = (CurrentWeapon->GetWeaponValue() >= SecondaryWeapon->GetWeaponValue())
			? CurrentWeapon.Get() : SecondaryWeapon.Get();
	}
	else if (CurrentWeapon)
	{
		WeaponToDrop = CurrentWeapon;
	}
	else if (SecondaryWeapon)
	{
		WeaponToDrop = SecondaryWeapon;
	}

	if (WeaponToDrop)
	{
		const bool bDroppedActive = (WeaponToDrop == GetActiveWeapon());
		const int32 DroppedSlot = (WeaponToDrop == CurrentWeapon) ? 0 : 1;

		// Detach from character and place in world.
		const FVector SpawnLoc = GetActorLocation()
			+ GetActorForwardVector() * 80.0f
			+ FVector(0, 0, -40.0f);
		WeaponToDrop->PlaceInWorld(SpawnLoc);

		if (DroppedSlot == 0)
			CurrentWeapon = nullptr;
		else
			SecondaryWeapon = nullptr;

		// Destroy the remaining weapon.
		if (DroppedSlot == 0 && SecondaryWeapon)
		{
			SecondaryWeapon->Destroy();
			SecondaryWeapon = nullptr;
		}
		else if (DroppedSlot == 1 && CurrentWeapon)
		{
			CurrentWeapon->Destroy();
			CurrentWeapon = nullptr;
		}

		// If we dropped the active weapon, switch to the surviving one.
		if (bDroppedActive && SecondaryWeapon)
			ActiveWeaponSlot = 1;
		else if (bDroppedActive && CurrentWeapon)
			ActiveWeaponSlot = 0;
	}

	if (AFPSGameMode* GM = Cast<AFPSGameMode>(GetWorld()->GetAuthGameMode()))
		GM->OnPlayerDied(GetController(), Killer);
}

void AFPSCharacter::DropWeaponAsPickup(AFPSWeapon* Weapon)
{
	if (!HasAuthority() || !Weapon)
		return;

	const FVector SpawnLoc = GetActorLocation()
		+ GetActorForwardVector() * 80.0f
		+ FVector(0, 0, -40.0f);

	Weapon->PlaceInWorld(SpawnLoc);
}

void AFPSCharacter::Respawn(const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	if (!HasAuthority())
		return;

	SetActorLocationAndRotation(SpawnLocation, SpawnRotation, /*bSweep=*/false);

	if (AController* C = GetController())
		C->SetControlRotation(SpawnRotation);

	Health = MaxHealth;
	bWantsToRun = false;
	bWantsToAim = false;
	bWantsToFire = false;
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;

	OnHealthChanged.Broadcast(Health, MaxHealth);

	Stamina = MaxStamina;
	OnStaminaChanged.Broadcast(Stamina, MaxStamina);

	// Reset surviving weapon ammo; respawn default if only weapon was dropped.
	if (CurrentWeapon)
	{
		CurrentWeapon->ServerResetAmmo();
	}
	else if (WeaponClass)
	{
		CurrentWeapon = GetWorld()->SpawnActorDeferred<AFPSWeapon>(WeaponClass, FTransform::Identity, this, this);
		if (CurrentWeapon)
		{
			CurrentWeapon->SetOwningCharacter(this);
			CurrentWeapon->FinishSpawning(FTransform::Identity);
			CurrentWeapon->ServerResetAmmo();

			GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
			{
				if (CurrentWeapon)
					OnWeaponEquipped();
			});
		}
	}

	if (SecondaryWeapon)
		SecondaryWeapon->ServerResetAmmo();

	bIsDead = false;

	ClearItemUseState();
	DeactivateHoT();

	SetDeathPresentation(false);
	OnRespawn();
}

void AFPSCharacter::SetDeathPresentation(bool bDead)
{
	if (USkeletalMeshComponent* MeshComp = GetMesh())
		MeshComp->SetVisibility(!bDead, /*bPropagateToChildren=*/true);

	if (CurrentWeapon)
		CurrentWeapon->SetActorHiddenInGame(bDead);

	if (SecondaryWeapon)
		SecondaryWeapon->SetActorHiddenInGame(bDead);

	SetActorEnableCollision(!bDead);
}

void AFPSCharacter::OnRep_Health(float OldHealth)
{
	OnHealthChanged.Broadcast(Health, MaxHealth);
}

void AFPSCharacter::OnRep_CurrentWeapon(AFPSWeapon* OldWeapon)
{
	if (CurrentWeapon && !bWeaponEquipped)
	{
		bWeaponEquipped = true;
		OnWeaponEquipped();
	}
}

void AFPSCharacter::OnRep_SecondaryWeapon(AFPSWeapon* OldWeapon)
{
	if (SecondaryWeapon)
	{
		SecondaryWeapon->SetActorHiddenInGame(true);
		OnSecondaryWeaponEquipped();
	}
}

void AFPSCharacter::OnRep_ActiveWeaponSlot()
{
	if (CurrentWeapon)
		CurrentWeapon->SetActorHiddenInGame(ActiveWeaponSlot != 0);
	if (SecondaryWeapon)
		SecondaryWeapon->SetActorHiddenInGame(ActiveWeaponSlot != 1);
}

void AFPSCharacter::OnRep_bIsDead()
{
	if (bIsDead)
	{
		SetDeathPresentation(true);
		OnDeathStart();
	}
	else
	{
		SetDeathPresentation(false);
		OnRespawn();
	}
}

// ---------------------------------------------------------------------------
// Enhanced Input
// ---------------------------------------------------------------------------

void AFPSCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		UE_LOG(LogTemp, Error, TEXT("[Input] InputComponent is NOT EnhancedInputComponent! Class=%s"),
			*PlayerInputComponent->GetClass()->GetName());
		return;
	}

	if (IsLocallyControlled())
	{
		GetMesh()->SetOwnerNoSee(true);
	}

	if (InputMove)
	{
		EIC->BindAction(InputMove, ETriggerEvent::Triggered, this, &AFPSCharacter::Move);
		EIC->BindAction(InputMove, ETriggerEvent::Completed, this, &AFPSCharacter::MoveCompleted);
	}

	if (InputLook)
	{
		EIC->BindAction(InputLook, ETriggerEvent::Triggered, this, &AFPSCharacter::Look);
	}

	if (InputJump)
	{
		EIC->BindAction(InputJump, ETriggerEvent::Started, this, &ACharacter::Jump);
		EIC->BindAction(InputJump, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	}

	if (InputRun)
	{
		EIC->BindAction(InputRun, ETriggerEvent::Started, this, &AFPSCharacter::StartRun);
		EIC->BindAction(InputRun, ETriggerEvent::Completed, this, &AFPSCharacter::StopRun);
	}

	if (InputAim)
	{
		EIC->BindAction(InputAim, ETriggerEvent::Started, this, &AFPSCharacter::StartAim);
		EIC->BindAction(InputAim, ETriggerEvent::Completed, this, &AFPSCharacter::StopAim);
	}

	if (InputFire)
	{
		EIC->BindAction(InputFire, ETriggerEvent::Started, this, &AFPSCharacter::StartFire);
		EIC->BindAction(InputFire, ETriggerEvent::Completed, this, &AFPSCharacter::StopFire);
	}

	if (InputReload)
	{
		EIC->BindAction(InputReload, ETriggerEvent::Started, this, &AFPSCharacter::Reload);
	}

	if (InputInteract)
	{
		EIC->BindAction(InputInteract, ETriggerEvent::Started, this, &AFPSCharacter::Interact);
	}

	if (InputInventory)
	{
		EIC->BindAction(InputInventory, ETriggerEvent::Started, this, &AFPSCharacter::ToggleInventory);
	}

	if (InputSwitchWeapon1)
	{
		EIC->BindAction(InputSwitchWeapon1, ETriggerEvent::Started, this, &AFPSCharacter::SwitchToPrimaryWeapon);
	}

	if (InputSwitchWeapon2)
	{
		EIC->BindAction(InputSwitchWeapon2, ETriggerEvent::Started, this, &AFPSCharacter::SwitchToSecondaryWeapon);
	}

	if (InputCycleInteraction)
	{
		EIC->BindAction(InputCycleInteraction, ETriggerEvent::Triggered, this, &AFPSCharacter::CycleInteractionInput);
	}
}

// ---------------------------------------------------------------------------
// Move (Axis2D, X=Forward, Y=Right)
// ---------------------------------------------------------------------------

void AFPSCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Input = Value.Get<FVector2D>();
	MoveInputAxis = Input;

	if (!Controller || bIsDead || bInputLocked)
		return;

	const FRotator YawRotation(0, Controller->GetControlRotation().Yaw, 0);
	const FRotationMatrix YawMatrix(YawRotation);

	if (!FMath::IsNearlyZero(Input.X))
	{
		AddMovementInput(YawMatrix.GetUnitAxis(EAxis::X), Input.X);
	}
	if (!FMath::IsNearlyZero(Input.Y))
	{
		AddMovementInput(YawMatrix.GetUnitAxis(EAxis::Y), Input.Y);
	}
}

void AFPSCharacter::MoveCompleted(const FInputActionValue& Value)
{
	MoveInputAxis = FVector2D::ZeroVector;
}

// ---------------------------------------------------------------------------
// Run / Aim
// ---------------------------------------------------------------------------

void AFPSCharacter::StartRun()
{
	if (IsActionLocked())
		return;
	bWantsToRun = true;
	OnRunStarted.Broadcast();
	ServerSetWantsToRun(true);
}

void AFPSCharacter::StopRun()
{
	if(bWantsToRun == false)
		return;

	bWantsToRun = false;
	OnRunStopped.Broadcast();
	ServerSetWantsToRun(false);

	if (bAutoResumeFireAfterRun && bWantsToFire && !bIsDead)
	{
		OnFireStarted.Broadcast();
		ServerStartFire();
	}
}

void AFPSCharacter::StartAim()
{
	if (IsActionLocked())
		return;

	if (AFPSPlayerController* PC = Cast<AFPSPlayerController>(GetController()))
	{
		if (PC->IsScoreboardOpen())
		{
			PC->ToggleScoreboardMouseLock();
			return;
		}
	}

	bWantsToAim = true;
	OnAimStarted.Broadcast();
	ServerSetWantsToAim(true);
}

void AFPSCharacter::StopAim()
{
	if (AFPSPlayerController* PC = Cast<AFPSPlayerController>(GetController()))
	{
		if (PC->IsScoreboardOpen())
			return;
	}

	bWantsToAim = false;
	OnAimStopped.Broadcast();
	ServerSetWantsToAim(false);
}

void AFPSCharacter::ServerSetWantsToRun_Implementation(bool bNewWantsToRun)
{
	if (bNewWantsToRun && Stamina <= RunStartStaminaThreshold)
		return;

	bWantsToRun = bNewWantsToRun;
	UpdateMovementState();
	if (!bIsDead)
		ApplyMovementSpeed();
	MulticastOnRunStateChanged(bNewWantsToRun);
}

void AFPSCharacter::MulticastOnRunStateChanged_Implementation(bool bNewWantsToRun)
{
	if (IsLocallyControlled())
		return;

	if (bNewWantsToRun)
		OnRunStarted.Broadcast();
	else
		OnRunStopped.Broadcast();
}

void AFPSCharacter::ServerSetWantsToAim_Implementation(bool bNewWantsToAim)
{
	bWantsToAim = bNewWantsToAim;
	UpdateMovementState();
	if (!bIsDead)
		ApplyMovementSpeed();
	MulticastOnAimStateChanged(bNewWantsToAim);
}

void AFPSCharacter::MulticastOnAimStateChanged_Implementation(bool bNewWantsToAim)
{
	if (IsLocallyControlled())
		return;

	if (bNewWantsToAim)
		OnAimStarted.Broadcast();
	else
		OnAimStopped.Broadcast();
}

// ---------------------------------------------------------------------------
// Fire
// ---------------------------------------------------------------------------

void AFPSCharacter::StartFire()
{
	if (IsActionLocked())
		return;

	if (IsScoreboardMouseLocked())
		return;

	bWantsToFire = true;
	if(MovementState == EFPSMovementState::Running)
		return;

	OnFireStarted.Broadcast();
	ServerStartFire();
}

void AFPSCharacter::StopFire()
{
	bWantsToFire = false;
	OnFireStopped.Broadcast();
	ServerStopFire();

	AFPSWeapon* ActiveWeapon = GetActiveWeapon();
	if (ActiveWeapon && ActiveWeapon->GetCurrentAmmo() <= 0)
		Reload();
}

void AFPSCharacter::ServerStartFire_Implementation()
{
	AFPSWeapon* ActiveWeapon = GetActiveWeapon();
	if (ActiveWeapon)
		ActiveWeapon->StartFire();
	MulticastOnFireStateChanged(true);
}

void AFPSCharacter::ServerStopFire_Implementation()
{
	AFPSWeapon* ActiveWeapon = GetActiveWeapon();
	if (ActiveWeapon)
		ActiveWeapon->StopFire();
	MulticastOnFireStateChanged(false);

	if (ActiveWeapon && ActiveWeapon->GetCurrentAmmo() <= 0)
		ActiveWeapon->ServerBeginReload();
}

// ---------------------------------------------------------------------------
// Reload
// ---------------------------------------------------------------------------

void AFPSCharacter::Reload()
{
	AFPSWeapon* ActiveWeapon = GetActiveWeapon();
	if (IsActionLocked() || !ActiveWeapon)
		return;

	if (!ActiveWeapon->CanReload())
		return;

	ServerReload();
}

void AFPSCharacter::ServerReload_Implementation()
{
	if (bIsDead)
		return;

	AFPSWeapon* ActiveWeapon = GetActiveWeapon();
	if (ActiveWeapon)
		ActiveWeapon->ServerBeginReload();
}

void AFPSCharacter::ServerCancelUseItem_Implementation()
{
	CancelItemUse();
}

// ---------------------------------------------------------------------------
// Weapon switching
// ---------------------------------------------------------------------------

void AFPSCharacter::SwitchWeapon(int32 NewSlot)
{
	if (NewSlot < 0 || NewSlot > 1 || NewSlot == ActiveWeaponSlot)
		return;

	AFPSWeapon* NewWeapon = GetWeaponInSlot(NewSlot);
	if (!NewWeapon)
		return;

	// Stop firing/aiming on current active weapon.
	if (bWantsToFire)
		StopFire();
	if (bWantsToAim)
		StopAim();

	ServerSwitchWeapon(NewSlot);
}

void AFPSCharacter::SwitchToPrimaryWeapon()
{
	if (!IsLocallyControlled())
		return;

	if (!SecondaryWeapon || ActiveWeaponSlot == 0)
		return;

	SwitchWeapon(0);
}

void AFPSCharacter::SwitchToSecondaryWeapon()
{
	if (!IsLocallyControlled())
		return;

	if (!SecondaryWeapon || ActiveWeaponSlot == 1)
		return;

	SwitchWeapon(1);
}

void AFPSCharacter::ServerSwitchWeapon_Implementation(int32 NewSlot)
{
	if (!HasAuthority() || NewSlot < 0 || NewSlot > 1 || NewSlot == ActiveWeaponSlot)
		return;

	AFPSWeapon* NewWeapon = GetWeaponInSlot(NewSlot);
	if (!NewWeapon)
		return;

	// Stop old weapon fire.
	AFPSWeapon* OldWeapon = GetActiveWeapon();
	if (OldWeapon)
		OldWeapon->StopFire();

	// Switch slot. Visibility updates on all clients via OnRep_ActiveWeaponSlot.
	ActiveWeaponSlot = NewSlot;

	// Server host manually toggles visibility.
	if (CurrentWeapon)
		CurrentWeapon->SetActorHiddenInGame(ActiveWeaponSlot != 0);
	if (SecondaryWeapon)
		SecondaryWeapon->SetActorHiddenInGame(ActiveWeaponSlot != 1);
}

void AFPSCharacter::EquipWeapon(AFPSWeapon* Weapon, int32 Slot)
{
	if (!HasAuthority() || !Weapon || Slot < 0 || Slot > 1)
		return;

	Weapon->RemoveFromWorld();
	Weapon->SetOwningCharacter(this);
	Weapon->ServerResetAmmo();

	if (Slot == 0)
	{
		CurrentWeapon = Weapon;
	}
	else
	{
		SecondaryWeapon = Weapon;
	}

	// Set visibility based on active slot.
	if (CurrentWeapon)
		CurrentWeapon->SetActorHiddenInGame(ActiveWeaponSlot != 0);
	if (SecondaryWeapon)
		SecondaryWeapon->SetActorHiddenInGame(ActiveWeaponSlot != 1);

	if (Slot == 0)
		OnWeaponEquipped();
	else
		OnSecondaryWeaponEquipped();
}

// ---------------------------------------------------------------------------
// Weapon pickup
// ---------------------------------------------------------------------------

void AFPSCharacter::SetWeaponPickupTarget(AFPSWeapon* Weapon)
{
	if (CurrentWeaponPickupTarget == Weapon)
		return;

	CurrentWeaponPickupTarget = Weapon;

	if (InteractionManager && Weapon)
	{
		InteractionManager->RegisterInteraction(Weapon,
			EFPSInteractionType::WeaponPickup,
			FText::FromString(TEXT("Swap weapon")),
			100);
	}

	OnWeaponPickupTargetChanged.Broadcast();
}

void AFPSCharacter::ClearWeaponPickupTarget(AFPSWeapon* Weapon)
{
	if (CurrentWeaponPickupTarget != Weapon)
		return;

	if (InteractionManager && CurrentWeaponPickupTarget)
		InteractionManager->UnregisterInteraction(CurrentWeaponPickupTarget);

	CurrentWeaponPickupTarget = nullptr;
	OnWeaponPickupTargetChanged.Broadcast();
}

void AFPSCharacter::ServerTryPickupWeapon_Implementation()
{
	if (!HasAuthority())
		return;

	// 服务端也用 CurrentWeaponPickupTarget（由武器 overlap 在服务端设置），加距离校验。
	if (!CurrentWeaponPickupTarget || !CurrentWeaponPickupTarget->IsOnGround())
		return;

	const float Dist = FVector::Dist(GetActorLocation(), CurrentWeaponPickupTarget->GetActorLocation());
	if (Dist > CurrentWeaponPickupTarget->GetPickupRadius() + 50.0f)
		return;

	AFPSWeapon* Weapon = CurrentWeaponPickupTarget;

	// 0 guns: equip as primary.
	if (!CurrentWeapon && !SecondaryWeapon)
	{
		EquipWeapon(Weapon, 0);
		return;
	}

	// 1 gun: equip to free slot.
	if (!CurrentWeapon)
	{
		EquipWeapon(Weapon, 0);
		return;
	}
	if (!SecondaryWeapon)
	{
		EquipWeapon(Weapon, 1);
		return;
	}

	// 2 guns: swap with active weapon - drop old, equip new.
	AFPSWeapon* OldWeapon = GetActiveWeapon();
	const int32 Slot = ActiveWeaponSlot;

	const FVector SpawnLoc = GetActorLocation()
		+ GetActorForwardVector() * 80.0f
		+ FVector(0, 0, -40.0f);
	OldWeapon->PlaceInWorld(SpawnLoc);

	EquipWeapon(Weapon, Slot);
}

// ---------------------------------------------------------------------------
// Progress bar
// ---------------------------------------------------------------------------

void AFPSCharacter::BeginProgress(EFPSProgressType Type, float Duration)
{
	if (Type == EFPSProgressType::None)
		return;

	if (ActiveProgress != EFPSProgressType::None)
		OnProgressEnd.Broadcast(ActiveProgress, false);

	ActiveProgress = Type;
	OnProgressBegin.Broadcast(Type, Duration);
}

void AFPSCharacter::EndProgress(EFPSProgressType Type, bool bCompleted)
{
	if (ActiveProgress != Type)
		return;

	ActiveProgress = EFPSProgressType::None;
	OnProgressEnd.Broadcast(Type, bCompleted);
}

void AFPSCharacter::ServerUpdateAimPitch_Implementation(float Pitch)
{
	ArmPitch = Pitch;
}

void AFPSCharacter::MulticastOnFireStateChanged_Implementation(bool bNewFiring)
{
	if (IsLocallyControlled())
		return;

	if (bNewFiring)
		OnFireStarted.Broadcast();
	else
		OnFireStopped.Broadcast();
}

void AFPSCharacter::DispatchHitFX(const FVector& ImpactPoint, const FVector& ImpactNormal,
	float Damage, AFPSCharacter* Victim)
{
	if (IsLocallyControlled())
		OnDealtHit.Broadcast(ImpactPoint, ImpactNormal, Damage, Victim);

	if (Victim && Victim->IsLocallyControlled())
		Victim->OnReceivedHit.Broadcast(ImpactPoint, ImpactNormal, Damage, Victim);

	OnHitWorld.Broadcast(ImpactPoint, ImpactNormal, Damage, Victim);
}

// ---------------------------------------------------------------------------
// Mouse look
// ---------------------------------------------------------------------------

void AFPSCharacter::Look(const FInputActionValue& Value)
{
	if (!Controller || bInputLocked)
		return;

	const FVector2D Input = Value.Get<FVector2D>();
	const float Sens = MouseSensitivity * (bWantsToAim ? AimSensitivityMultiplier : 1.0f);

	if (!FMath::IsNearlyZero(Input.X))
		AddControllerYawInput(Input.X * Sens);
	if (!FMath::IsNearlyZero(Input.Y))
		AddControllerPitchInput(Input.Y * Sens);
}

// ---------------------------------------------------------------------------
// Movement state
// ---------------------------------------------------------------------------

void AFPSCharacter::UpdateMovementState()
{
	if (bIsDead || !Controller)
	{
		MovementState = EFPSMovementState::Idle;
		return;
	}

	const FVector Velocity = GetVelocity();
	const float VelocityLength = Velocity.Length();
	const bool bIsMoving = VelocityLength > UE_KINDA_SMALL_NUMBER;

	const FRotator ControlRotation = Controller->GetControlRotation();
	const FRotator YawOnlyRotation(0, ControlRotation.Yaw, 0);
	const FVector ForwardDir = YawOnlyRotation.RotateVector(FVector::ForwardVector);
	const float ForwardSpeed = Velocity.Dot(ForwardDir);

	if (bWantsToAim)
	{
		MovementState = EFPSMovementState::Aiming;
	}
	else if (ForwardSpeed > UE_KINDA_SMALL_NUMBER)
	{
		if (bWantsToRun && (Stamina >= RunStartStaminaThreshold || MovementState == EFPSMovementState::Running))
		{
			MovementState = EFPSMovementState::Running;
			AFPSWeapon* ActiveWeapon = GetActiveWeapon();
			if (ActiveWeapon && ActiveWeapon->IsFiring())
			{
				OnFireStopped.Broadcast();
				if (HasAuthority() || IsLocallyControlled())
					ServerStopFire();
			}
		}
		else
		{
			MovementState = EFPSMovementState::Walking;
		}
	}
	else if (bIsMoving)
	{
		MovementState = EFPSMovementState::Walking;
	}
	else
	{
		MovementState = EFPSMovementState::Idle;
	}
}

void AFPSCharacter::ApplyMovementSpeed()
{
	switch (MovementState)
	{
	case EFPSMovementState::Aiming:
		GetCharacterMovement()->MaxWalkSpeed = AimSpeed;
		break;
	case EFPSMovementState::Running:
		GetCharacterMovement()->MaxWalkSpeed = RunSpeed;
		break;
	default:
		GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
		break;
	}
}

// ---------------------------------------------------------------------------
// Stamina
// ---------------------------------------------------------------------------

void AFPSCharacter::UpdateStamina(float DeltaTime)
{
	const float OldStamina = Stamina;

	if (MovementState == EFPSMovementState::Running)
	{
		Stamina = FMath::Max(0.0f, Stamina - StaminaDrainRate * DeltaTime);

		if (Stamina <= 0.0f)
		{
			ServerStopRunForced();
		}
	}
	else
	{
		const float RegenRate = (MovementState == EFPSMovementState::Idle)
			? StaminaRegenRateIdle
			: StaminaRegenRateWalking;
		Stamina = FMath::Min(MaxStamina, Stamina + RegenRate * DeltaTime);
	}

	if (!FMath::IsNearlyEqual(Stamina, OldStamina))
		OnStaminaChanged.Broadcast(Stamina, MaxStamina);
}

void AFPSCharacter::ServerStopRunForced()
{
	bWantsToRun = false;
	UpdateMovementState();
	ApplyMovementSpeed();

	MulticastOnRunStateChanged(false);

	if (IsLocallyControlled())
	{
		OnRunStopped.Broadcast();
	}
	else
	{
		ClientForceStopRun();
	}
}

void AFPSCharacter::ClientForceStopRun_Implementation()
{
	bWantsToRun = false;
	OnRunStopped.Broadcast();
}

void AFPSCharacter::OnRep_Stamina()
{
	OnStaminaChanged.Broadcast(Stamina, MaxStamina);
}

// ---------------------------------------------------------------------------
// Interact (F key): Weapon pickup > Item pickup > Cancel item use
// ---------------------------------------------------------------------------

void AFPSCharacter::Interact()
{
	// F cancels item use.
	if (bIsUsingItem)
	{
		ServerCancelUseItem();
		return;
	}

	if (IsActionLocked())
		return;

	// 交互管理器优先：按选中的交互条目路由行为。
	if (InteractionManager && InteractionManager->GetEntryCount() > 0)
	{
		switch (InteractionManager->GetActiveType())
		{
		case EFPSInteractionType::SubmissionPoint:
			ServerSubmitAllValuables();
			return;
		case EFPSInteractionType::WeaponPickup:
			ServerTryPickupWeapon();
			return;
		case EFPSInteractionType::Pickup:
			ServerTryPickup();
			return;
		}
	}

	// 兜底（交互管理器无条目时走旧逻辑）。
	if (CurrentWeaponPickupTarget)
	{
		ServerTryPickupWeapon();
		return;
	}

	if (!CurrentPickupTarget)
		return;

	ServerTryPickup();
}

// ---------------------------------------------------------------------------
// Interaction manager (scroll wheel + prompt list)
// ---------------------------------------------------------------------------

void AFPSCharacter::CycleInteractionNext()
{
	if (InteractionManager)
		InteractionManager->CycleNext();
}

void AFPSCharacter::CycleInteractionPrev()
{
	if (InteractionManager)
		InteractionManager->CyclePrev();
}

void AFPSCharacter::CycleInteractionInput(const FInputActionValue& Value)
{
	const float Axis = Value.Get<float>();
	if (Axis > 0.1f)
		CycleInteractionNext();
	else if (Axis < -0.1f)
		CycleInteractionPrev();
}

// ---------------------------------------------------------------------------
// Submission system
// ---------------------------------------------------------------------------

void AFPSCharacter::SetSubmissionTarget(AFPSSubmissionPoint* Point)
{
	if (CurrentSubmissionTarget == Point)
		return;

	CurrentSubmissionTarget = Point;

	if (InteractionManager && Point)
	{
		InteractionManager->RegisterInteraction(Point,
			EFPSInteractionType::SubmissionPoint,
			FText::FromString(TEXT("Submit valuables")),
			50);
	}
	else if (InteractionManager)
	{
		InteractionManager->UnregisterInteraction(Point);
	}

	OnSubmissionTargetChanged.Broadcast();
}

void AFPSCharacter::ClearSubmissionTarget(AFPSSubmissionPoint* Point)
{
	if (CurrentSubmissionTarget != Point)
		return;

	if (InteractionManager && CurrentSubmissionTarget)
		InteractionManager->UnregisterInteraction(CurrentSubmissionTarget);

	CurrentSubmissionTarget = nullptr;
	OnSubmissionTargetChanged.Broadcast();
}

bool AFPSCharacter::CanSubmitItems() const
{
	return CurrentSubmissionTarget && CurrentSubmissionTarget->IsOpen();
}

void AFPSCharacter::SubmitInventoryItem(int32 Index)
{
	if (!CanSubmitItems())
		return;

	ServerSubmitSingleItem(Index);
}

void AFPSCharacter::ServerSubmitAllValuables_Implementation()
{
	if (!HasAuthority())
		return;

	if (CurrentSubmissionTarget && CurrentSubmissionTarget->IsOpen())
		CurrentSubmissionTarget->SubmitAllValuables(this);
}

void AFPSCharacter::ServerSubmitSingleItem_Implementation(int32 Index)
{
	if (!HasAuthority())
		return;

	if (CurrentSubmissionTarget && CurrentSubmissionTarget->IsOpen())
		CurrentSubmissionTarget->SubmitSingleItem(this, Index);
}

// ---------------------------------------------------------------------------
// Inventory / Item system
// ---------------------------------------------------------------------------

void AFPSCharacter::ServerApplyHeal(float Amount)
{
	if (!HasAuthority() || bIsDead || Amount <= 0.0f)
		return;

	Health = FMath::Clamp(Health + Amount, 0.0f, MaxHealth);
	OnHealthChanged.Broadcast(Health, MaxHealth);
}

void AFPSCharacter::UseInventoryItem(int32 Index)
{
	AFPSWeapon* ActiveWeapon = GetActiveWeapon();
	if (bIsDead || bIsUsingItem || (ActiveWeapon && ActiveWeapon->IsReloading()))
		return;

	if (bInventoryOpen)
		CloseInventory();

	ServerUseInventoryItem(Index);
}

void AFPSCharacter::ServerUseInventoryItem_Implementation(int32 Index)
{
	if (!Inventory)
		return;

	AFPSWeapon* ActiveWeapon = GetActiveWeapon();
	if (bIsDead || bIsUsingItem || (ActiveWeapon && ActiveWeapon->IsReloading()))
		return;

	FItemUseResult Result = Inventory->BeginItemUse(Index);
	if (Result.UseType == EFPSItemUseType::None)
		return;

	if (Result.UseType == EFPSItemUseType::ChanneledHeal ||
		Result.UseType == EFPSItemUseType::HoTApply)
	{
		StartItemUseProcess(Result);
	}
}

void AFPSCharacter::DropInventoryItem(int32 Index)
{
	ServerDropInventoryItem(Index);
}

void AFPSCharacter::ServerDropInventoryItem_Implementation(int32 Index)
{
	if (Inventory)
		Inventory->ServerDropItem(Index);
}

void AFPSCharacter::ForceStopFireAndAim()
{
	if (!IsLocallyControlled())
		return;

	if (bWantsToFire)
		StopFire();
	if (bWantsToAim)
		StopAim();
}

bool AFPSCharacter::IsScoreboardMouseLocked() const
{
	if (const AFPSPlayerController* PC = Cast<AFPSPlayerController>(GetController()))
		return PC->IsScoreboardMouseLocked();
	return false;
}

void AFPSCharacter::ToggleInventory()
{
	if (!IsLocallyControlled())
		return;

	if (const AFPSPlayerController* PC = Cast<AFPSPlayerController>(GetController()))
	{
		if (PC->IsScoreboardOpen())
			return;
	}

	bInventoryOpen = !bInventoryOpen;
	bInputLocked = bInventoryOpen;

	if (bInventoryOpen)
	{
		if (bWantsToFire)
			StopFire();
		if (bWantsToRun)
			StopRun();
		if (bWantsToAim)
			StopAim();
	}

	OnToggleInventory(bInventoryOpen);
}

void AFPSCharacter::CloseInventory()
{
	if (!IsLocallyControlled() || !bInventoryOpen)
		return;

	ToggleInventory();
}

void AFPSCharacter::ServerTryPickup_Implementation()
{
	if (!HasAuthority())
		return;

	if (!CurrentPickupTarget)
		return;

	const float Dist = FVector::Dist(GetActorLocation(), CurrentPickupTarget->GetActorLocation());
	if (Dist > CurrentPickupTarget->GetPickupRadius() + 50.0f)
		return;

	CurrentPickupTarget->ServerTryPickup(this);
}

void AFPSCharacter::SetPickupTarget(AFPSPickup* Pickup)
{
	if (CurrentPickupTarget == Pickup)
		return;

	CurrentPickupTarget = Pickup;

	if (InteractionManager && Pickup && Pickup->GetItemDef())
	{
		const FText Prompt = FText::Format(
			FText::FromString(TEXT("Pick up {0}")),
			Pickup->GetItemDef()->DisplayName);
		InteractionManager->RegisterInteraction(Pickup,
			EFPSInteractionType::Pickup, Prompt, 0);
	}

	OnPickupTargetChanged.Broadcast();
}

void AFPSCharacter::ClearPickupTarget(AFPSPickup* Pickup)
{
	if (CurrentPickupTarget != Pickup)
		return;

	if (InteractionManager && CurrentPickupTarget)
		InteractionManager->UnregisterInteraction(CurrentPickupTarget);

	CurrentPickupTarget = nullptr;
	OnPickupTargetChanged.Broadcast();
}

// ---------------------------------------------------------------------------
// Action lock
// ---------------------------------------------------------------------------

bool AFPSCharacter::IsActionLocked() const
{
	if (bIsDead || bInputLocked || bIsUsingItem)
		return true;

	AFPSWeapon* ActiveWeapon = GetActiveWeapon();
	if (ActiveWeapon && ActiveWeapon->IsReloading())
		return true;

	return false;
}

// ---------------------------------------------------------------------------
// Item use state machine (server only)
// ---------------------------------------------------------------------------

void AFPSCharacter::StartItemUseProcess(const FItemUseResult& Result)
{
	if (!HasAuthority())
		return;

	ActiveItemUseType = Result.UseType;

	if (Result.UseType == EFPSItemUseType::ChanneledHeal)
	{
		RemainingHealAmount = Result.ConsumedAmount;
		ActiveHealPerTick = Result.HealPerTick;
		ActiveHealInterval = Result.HealInterval;

		const int32 TickCount = FMath::CeilToInt((float)Result.ConsumedAmount / (float)Result.HealPerTick);
		const float TotalDuration = Result.UseTime + TickCount * Result.HealInterval;

		bItemUseCompleted = false;
		ItemUseTotalDuration = TotalDuration;
		bIsUsingItem = true;

		BeginProgress(EFPSProgressType::UseItem, TotalDuration);

		GetWorldTimerManager().SetTimer(ItemUsePhaseTimerHandle, this,
			&AFPSCharacter::OnChanneledHealWindUpComplete, Result.UseTime, false);
	}
	else if (Result.UseType == EFPSItemUseType::HoTApply)
	{
		PendingHoTBaseDuration = Result.HoTBaseDuration;

		bItemUseCompleted = false;
		ItemUseTotalDuration = Result.UseTime;
		bIsUsingItem = true;

		BeginProgress(EFPSProgressType::UseItem, Result.UseTime);

		GetWorldTimerManager().SetTimer(ItemUsePhaseTimerHandle, this,
			&AFPSCharacter::OnHoTApplicationComplete, Result.UseTime, false);
	}
}

void AFPSCharacter::OnChanneledHealWindUpComplete()
{
	if (!HasAuthority() || RemainingHealAmount <= 0)
	{
		CompleteItemUse();
		return;
	}

	GetWorldTimerManager().SetTimer(HealTickTimerHandle, this,
		&AFPSCharacter::OnChanneledHealTick, ActiveHealInterval, true);

	OnChanneledHealTick();
}

void AFPSCharacter::OnChanneledHealTick()
{
	if (!HasAuthority() || RemainingHealAmount <= 0)
	{
		CompleteItemUse();
		return;
	}

	const int32 TickHeal = FMath::Min(ActiveHealPerTick, RemainingHealAmount);
	ServerApplyHeal((float)TickHeal);
	RemainingHealAmount -= TickHeal;

	if (RemainingHealAmount <= 0)
		CompleteItemUse();
}

void AFPSCharacter::OnHoTApplicationComplete()
{
	if (!HasAuthority())
		return;

	CompleteItemUse();
	ActivateHoT(PendingHoTBaseDuration);
}

void AFPSCharacter::CompleteItemUse()
{
	if (!HasAuthority())
		return;

	bItemUseCompleted = true;
	ClearItemUseState();
	bIsUsingItem = false;
	ItemUseTotalDuration = 0.0f;
	EndProgress(EFPSProgressType::UseItem, true);
}

void AFPSCharacter::CancelItemUse()
{
	if (!HasAuthority() || !bIsUsingItem)
		return;

	bItemUseCompleted = false;
	ClearItemUseState();
	bIsUsingItem = false;
	ItemUseTotalDuration = 0.0f;
	EndProgress(EFPSProgressType::UseItem, false);
}

void AFPSCharacter::ClearItemUseState()
{
	if (ItemUsePhaseTimerHandle.IsValid())
	{
		GetWorldTimerManager().ClearTimer(ItemUsePhaseTimerHandle);
		ItemUsePhaseTimerHandle.Invalidate();
	}
	if (HealTickTimerHandle.IsValid())
	{
		GetWorldTimerManager().ClearTimer(HealTickTimerHandle);
		HealTickTimerHandle.Invalidate();
	}

	ActiveItemUseType = EFPSItemUseType::None;
	RemainingHealAmount = 0;
	ActiveHealPerTick = 0;
	ActiveHealInterval = 0.0f;
	PendingHoTBaseDuration = 0.0f;
}

// ---------------------------------------------------------------------------
// OnRep - item use (client progress bar)
// ---------------------------------------------------------------------------

void AFPSCharacter::OnRep_bIsUsingItem()
{
	if (HasAuthority())
		return;

	if (bIsUsingItem)
		BeginProgress(EFPSProgressType::UseItem, ItemUseTotalDuration);
	else
		EndProgress(EFPSProgressType::UseItem, bItemUseCompleted);
}

// ---------------------------------------------------------------------------
// HoT buff
// ---------------------------------------------------------------------------

void AFPSCharacter::ActivateHoT(float AddedDuration)
{
	if (!HasAuthority())
		return;

	if (HoTState.bActive)
	{
		HoTRemainingTime = FMath::Min(HoTRemainingTime + AddedDuration, HoTMaxBuffDuration);
	}
	else
	{
		HoTRemainingTime = AddedDuration;
		HoTHealPerTick = HoTHealPerSecond * HoTTickInterval;

		GetWorldTimerManager().SetTimer(HoTTimerHandle, this,
			&AFPSCharacter::OnHoTTick, HoTTickInterval, true);
	}

	HoTState.bActive = true;
	HoTState.RemainingDuration = HoTRemainingTime;
	HoTState.MaxDuration = HoTMaxBuffDuration;
	HoTState.HealPerSecond = HoTHealPerSecond;

	OnHoTChanged.Broadcast(true, HoTRemainingTime, HoTMaxBuffDuration);
}

void AFPSCharacter::OnHoTTick()
{
	if (!HasAuthority() || !HoTState.bActive)
		return;

	ServerApplyHeal(HoTHealPerTick);
	HoTRemainingTime -= HoTTickInterval;

	if (HoTRemainingTime <= 0.0f)
	{
		DeactivateHoT();
	}
}

void AFPSCharacter::DeactivateHoT()
{
	if (!HasAuthority())
		return;

	if (HoTTimerHandle.IsValid())
	{
		GetWorldTimerManager().ClearTimer(HoTTimerHandle);
		HoTTimerHandle.Invalidate();
	}

	HoTRemainingTime = 0.0f;
	HoTHealPerTick = 0.0f;

	HoTState.bActive = false;
	HoTState.RemainingDuration = 0.0f;

	OnHoTChanged.Broadcast(false, 0.0f, 0.0f);
}

// ---------------------------------------------------------------------------
// OnRep - HoT buff (client UI)
// ---------------------------------------------------------------------------

void AFPSCharacter::OnRep_HoTState()
{
	if (HasAuthority())
		return;

	OnHoTChanged.Broadcast(HoTState.bActive, HoTState.RemainingDuration, HoTState.MaxDuration);
}
