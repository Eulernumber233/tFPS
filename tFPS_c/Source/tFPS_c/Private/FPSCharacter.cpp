#include "FPSCharacter.h"
#include "FPSWeapon.h"
#include "FPSGameMode.h"
#include "FPSPlayerState.h"
#include "FPSInventoryComponent.h"
#include "FPSPickup.h"
#include "FPSPlayerController.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "CollisionQueryParams.h"

// 子弹/射击专用 Trace 通道，与 FPSWeapon.cpp 的 COLLISION_WEAPON 一致（DefaultEngine.ini Name="Weapon"）。
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

	// ---- 射击命中体设置 ----
	// 子弹打骨骼网格体（贴合真实身体，可做部位判定），不打胶囊体（圆柱判定粗糙）。
	// 胶囊体继续负责移动/落地碰撞；网格体只参与 query（射线），不参与物理。
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		// 胶囊体忽略子弹通道 —— 否则圆柱和网格双重命中，且命中点落在圆柱表面不贴模型。
		Capsule->SetCollisionResponseToChannel(COLLISION_WEAPON, ECR_Ignore);
	}

	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		// 仅 query：射线能命中，但不产生物理碰撞（移动仍靠胶囊）。
		MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		MeshComp->SetCollisionObjectType(ECC_Pawn);
		MeshComp->SetCollisionResponseToAllChannels(ECR_Ignore);
		MeshComp->SetCollisionResponseToChannel(COLLISION_WEAPON, ECR_Block);
		// 命中走 Physics Asset 的简化碰撞体 —— 角色 Mesh 必须配 Physics Asset，否则射线打不中。
		MeshComp->SetGenerateOverlapEvents(false);
	}

	// 背包组件（服务端权威，复制给本人）。
	Inventory = CreateDefaultSubobject<UFPSInventoryComponent>(TEXT("Inventory"));
}

void AFPSCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 武器仅在服务端生成（权威版本，bReplicates=true，负责开火/伤害）。
	// 客户端不生成本地武器 —— 服务端 CurrentWeapon 复制到客户端后，
	// OnRep_CurrentWeapon 触发 OnWeaponEquipped。
	if (HasAuthority() && WeaponClass && !CurrentWeapon)
	{
		CurrentWeapon = GetWorld()->SpawnActorDeferred<AFPSWeapon>(WeaponClass, FTransform::Identity, this, this);
		if (CurrentWeapon)
		{
			CurrentWeapon->SetOwningCharacter(this);
			CurrentWeapon->FinishSpawning(FTransform::Identity);
			// 服务端权威：装满弹夹 + 初始备弹，复制到客户端
			CurrentWeapon->ServerResetAmmo();
		}

		// 服务端在下一帧触发 OnWeaponEquipped：
		// FinishSpawning 同步完成 Weapon BeginPlay，但其蓝图组件（Skeletal Mesh）
		// 可能在事件 BeginPlay 中动态构造。延迟一帧确保蓝图层 AttachToComponent
		// 拿到的 mesh 已完全就绪 —— 这是 Listen Server host 看不到自己武器的根因。
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

	// 本地控制端在下一帧注册 IMC：BeginPlay 时 Controller 可能尚未 Possess。
	// IMC / InputActions 由蓝图资产提供（在子类 default 中赋值），不再动态创建。
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
		// 体力先于速度更新：UpdateStamina 可能在耗尽时强制停跑（改 MovementState），
		// 紧接着 ApplyMovementSpeed 立即把速度切回 WalkSpeed，无延迟。
		UpdateStamina(DeltaTime);
		ApplyMovementSpeed();
	}

	// 所有本地控制的角色：从本地 Controller 读 Pitch
	// ArmPitch 是 Replicated，服务端赋值后自动复制到所有客户端
	if (!bIsDead && IsLocallyControlled() && Controller)
	{
		const float CurrentPitch = Controller->GetControlRotation().GetNormalized().Pitch;
		ArmPitch = CurrentPitch;

		// Listen Server host already has authority — no RPC needed, replication does the work.
		// Pure clients must send their pitch to server via RPC.
		if (!HasAuthority())
			ServerUpdateAimPitch(CurrentPitch);
	}

	WeaponFireState = (CurrentWeapon && CurrentWeapon->IsFiring())
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

	if (CurrentWeapon && HasAuthority())
	{
		CurrentWeapon->Destroy();
		CurrentWeapon = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

bool AFPSCharacter::IsFiring() const
{
	return CurrentWeapon && CurrentWeapon->IsFiring();
}

void AFPSCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AFPSCharacter, Health, COND_None);
	DOREPLIFETIME_CONDITION(AFPSCharacter, Stamina, COND_None);
	DOREPLIFETIME_CONDITION(AFPSCharacter, bIsDead, COND_None);
	DOREPLIFETIME_CONDITION(AFPSCharacter, CurrentWeapon, COND_None);
	DOREPLIFETIME_CONDITION(AFPSCharacter, ArmPitch, COND_None);
}

float AFPSCharacter::TakeDamage(float Damage, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority() || bIsDead || Health <= 0.0f)
		return 0.0f;

	float ActualDamage = Super::TakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser);
	Health = FMath::Clamp(Health - ActualDamage, 0.0f, MaxHealth);

	// 计分板「造成的总伤害」：把权威 ActualDamage 累加到攻击者的 PlayerState。
	// 排除自伤（攻击者就是自己）——自伤不计入战绩。EventInstigator 命中环境/世界时可能为空。
	if (ActualDamage > 0.0f && EventInstigator && EventInstigator != GetController())
	{
		if (AFPSPlayerState* AttackerPS = EventInstigator->GetPlayerState<AFPSPlayerState>())
			AttackerPS->AddDamage(ActualDamage);
	}

	// 服务端本机广播血量变化（客户端靠 OnRep_Health）。Die 内会再设 Health=0，
	// 但此处先播一次保证 listen server host 的血条即时更新。
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

	// 服务端权威状态：停火、禁用移动。
	if (CurrentWeapon)
		CurrentWeapon->StopFire();
	GetCharacterMovement()->DisableMovement();

	// 服务端本机的表现（隐藏网格/武器、关碰撞、播死亡相机）。
	// 客户端通过 OnRep_bIsDead 触发同样的表现 —— 见下方。
	SetDeathPresentation(true);
	OnDeathStart();

	if (AFPSPlayerState* KPS = Killer ? Killer->GetPlayerState<AFPSPlayerState>() : nullptr)
		KPS->AddKill();

	if (AFPSPlayerState* VPS = GetPlayerState<AFPSPlayerState>())
		VPS->AddDeath();

	if (AFPSGameMode* GM = Cast<AFPSGameMode>(GetWorld()->GetAuthGameMode()))
		GM->OnPlayerDied(GetController(), Killer);
}

void AFPSCharacter::Respawn(const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	if (!HasAuthority())
		return;

	// 1) 传送到出生点。bIsDead 还是 true 时碰撞已关，传送不会被卡。
	SetActorLocationAndRotation(SpawnLocation, SpawnRotation, /*bSweep=*/false);

	// 让本地控制端的视角也对准出生朝向（否则复活后还看着死前的方向）。
	if (AController* C = GetController())
		C->SetControlRotation(SpawnRotation);

	// 2) 复位权威状态：满血、恢复移动。
	Health = MaxHealth;
	bWantsToRun = false;
	bWantsToAim = false;
	bWantsToFire = false;
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;

	// 服务端本机刷血条（客户端靠 OnRep_Health）。
	OnHealthChanged.Broadcast(Health, MaxHealth);

	// 复活补满体力，复制到客户端刷 UI。
	Stamina = MaxStamina;
	OnStaminaChanged.Broadcast(Stamina, MaxStamina);

	// 复活补满弹药（满夹 + 初始备弹），复制到客户端刷 UI。
	if (CurrentWeapon)
		CurrentWeapon->ServerResetAmmo();

	// 3) 清死亡状态（复制到客户端 → OnRep_bIsDead 复位各端表现）。
	bIsDead = false;

	// 4) 服务端本机表现复位（客户端走 OnRep_bIsDead）。
	SetDeathPresentation(false);
	OnRespawn();
}

void AFPSCharacter::SetDeathPresentation(bool bDead)
{
	// 纯本地表现，每端各自执行。隐藏尸体网格 + 武器，关闭/恢复碰撞。
	if (USkeletalMeshComponent* MeshComp = GetMesh())
		MeshComp->SetVisibility(!bDead, /*bPropagateToChildren=*/true);

	if (CurrentWeapon)
		CurrentWeapon->SetActorHiddenInGame(bDead);

	// 死亡：完全关碰撞（子弹穿过尸体、不挡活人移动）。
	// 复活：恢复碰撞。
	SetActorEnableCollision(!bDead);
}

void AFPSCharacter::OnRep_Health(float OldHealth)
{
	// 客户端收到服务端权威血量 → 广播给 UI / 蓝图刷血条。
	OnHealthChanged.Broadcast(Health, MaxHealth);
}

void AFPSCharacter::OnRep_CurrentWeapon(AFPSWeapon* OldWeapon)
{
	// 客户端收到服务端复制武器（首次或替换），每个角色仅触发一次 OnWeaponEquipped。
	// 不在客户端调用 OldWeapon->Destroy() —— 客户端无权销毁 replicated actor，
	// 服务端销毁后会通过复制通道传到客户端。
	if (CurrentWeapon && !bWeaponEquipped)
	{
		bWeaponEquipped = true;
		OnWeaponEquipped();
	}
}

void AFPSCharacter::OnRep_bIsDead()
{
	// 客户端镜像服务端的死亡/复活表现。权威移动/碰撞由服务端控制，
	// 这里客户端也同步关/开本地碰撞与表现，并触发蓝图死亡相机事件。
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

	// 本地玩家隐藏第三人称身体；武器的 OwnerNoSee 由蓝图层在 OnWeaponEquipped 中设置
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
}

// ---------------------------------------------------------------------------
// 移动 (Axis2D, X=Forward, Y=Right)
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
	if (bInputLocked)
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

	// 奔跑打断开火后，松开 Shift 时若 LMB 仍按住且允许自动恢复，则恢复开火
	if (bAutoResumeFireAfterRun && bWantsToFire && !bIsDead)
	{
		OnFireStarted.Broadcast();
		ServerStartFire();
	}
}

void AFPSCharacter::StartAim()
{
	if (bInputLocked)
		return;

	// 计分板打开期间，右键不瞄准 —— 改为 toggle 鼠标锁定（未锁→锁，已锁→解锁）。
	// 这样：面板开但未锁时右键去锁鼠标（左键仍可射击）；锁后右键再按解锁回正常游戏。
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
	// 计分板打开期间右键被挪作 toggle 鼠标锁定，按下时没进入瞄准；松开时也不必走停瞄逻辑。
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
	// 起跑准入门槛：从其他状态进入奔跑时，体力必须 > 起跑线才放行。
	// 这只限制"起跑那一刻"；一旦跑起来，体力跌破阈值仍可继续跑，直到 UpdateStamina 在 0 时强制停。
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
	// 发送方（按 RMB 的客户端）已在 StartAim/StopAim 中本地广播，跳过避免重复
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
	if (bIsDead || bInputLocked)
		return;

	// 计分板锁鼠标后，左键点击进 UI（点排序按钮），不开火。
	if (IsScoreboardMouseLocked())
		return;

	bWantsToFire = true;
	if(MovementState == EFPSMovementState::Running)
		return;

	// 本地广播 OnFireStarted（0 延迟），蓝图层负责所有特效（粒子/音效/动画/连发 timer）
	OnFireStarted.Broadcast();

	// 服务端权威：LineTrace + 伤害判定走 RPC
	ServerStartFire();
}

void AFPSCharacter::StopFire()
{
	bWantsToFire = false;
	OnFireStopped.Broadcast();
	ServerStopFire();

	// 打空弹夹后松开开火键 → 自动换弹。
	// 只在弹夹真的空了（CurrentAmmo<=0）时触发；弹夹还有子弹时松手不换（玩家想提前换自行按 R）。
	// Reload() 内部有 CanReload 本地预判 + ServerReload 服务端权威兜底，半 RTT 的弹药延迟不会误触。
	if (CurrentWeapon && CurrentWeapon->GetCurrentAmmo() <= 0)
		Reload();
}

void AFPSCharacter::ServerStartFire_Implementation()
{
	if (CurrentWeapon)
		CurrentWeapon->StartFire();
	MulticastOnFireStateChanged(true);
}

void AFPSCharacter::ServerStopFire_Implementation()
{
	if (CurrentWeapon)
		CurrentWeapon->StopFire();
	MulticastOnFireStateChanged(false);

	// 服务端权威兜底：松开开火键时若弹夹空了，直接换弹。
	// 本地 StopFire 已尝试过 Reload()，但最后一发扣减可能晚于本地松手 → 本地漏判。
	// 服务端这里用权威 CurrentAmmo 再判一次，保证打空必换。
	// ServerBeginReload 内有 CanReload 守卫，与本地 Reload 重复调用不会换两次。
	if (CurrentWeapon && CurrentWeapon->GetCurrentAmmo() <= 0)
		CurrentWeapon->ServerBeginReload();
}

// ---------------------------------------------------------------------------
// Reload
// ---------------------------------------------------------------------------

void AFPSCharacter::Reload()
{
	if (bIsDead || bInputLocked || !CurrentWeapon)
		return;

	// 本地预判：不能换弹（满夹/无备弹/换弹中）直接忽略，省一次 RPC。
	// 真正的换弹由服务端权威执行（CanReload 在服务端再校验一次）。
	if (!CurrentWeapon->CanReload())
		return;

	// 不在此处直接起进度条：换弹是 reliable RPC，服务端立即把 bIsReloading 复制回来，
	// 经 Weapon 的 OnRep_IsReloading→BeginProgress(Reload) 统一驱动进度条（所有端同源），
	// 避免本地预判与服务端信号双重触发，也避免服务端 CanReload 不过时进度条空转。
	ServerReload();
}

void AFPSCharacter::ServerReload_Implementation()
{
	if (bIsDead || !CurrentWeapon)
		return;

	CurrentWeapon->ServerBeginReload();
}

// ---------------------------------------------------------------------------
// 持续动作进度条（本地表现，单进度模型 — 同时只有一个活跃进度）
// ---------------------------------------------------------------------------

void AFPSCharacter::BeginProgress(EFPSProgressType Type, float Duration)
{
	if (Type == EFPSProgressType::None)
		return;

	// 已有别的进度在跑 → 先打断它（bCompleted=false），再开新进度。
	// 这天然实现"换弹打断用药"等需求：新动作开始即顶替旧动作。
	// TODO 未来可能有bug，可能同一事件多次触发，比如多个同种药物前后分别使用
	if (ActiveProgress != EFPSProgressType::None)
		OnProgressEnd.Broadcast(ActiveProgress, false);

	ActiveProgress = Type;
	OnProgressBegin.Broadcast(Type, Duration);
}

void AFPSCharacter::EndProgress(EFPSProgressType Type, bool bCompleted)
{
	// 仅当结束的正是当前活跃进度才处理：若该进度已被新动作打断顶替，
	// 此处的旧结束信号应被忽略，避免误关掉新进度条。
	if (ActiveProgress != Type)
		return;

	ActiveProgress = EFPSProgressType::None;
	OnProgressEnd.Broadcast(Type, bCompleted);
}

void AFPSCharacter::ServerUpdateAimPitch_Implementation(float Pitch)
{
	ArmPitch = Pitch;
	static int32 LogCounter = 0;
	if (++LogCounter <= 10)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Pitch] ServerUpdateAimPitch(%.1f) for %s | LocallyControlled=%d"),
			Pitch, *GetName(), IsLocallyControlled());
	}
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

// 有命中的时候在攻击者角色实例上调用，分发三条通知（按本机角色身份分发）：
void AFPSCharacter::DispatchHitFX(const FVector& ImpactPoint, const FVector& ImpactNormal,
	float Damage, AFPSCharacter* Victim)
{
	// 本函数在每端的攻击者角色实例上执行（由武器的 MulticastHitConfirmed 调用）。
	// 三条通知都是纯本地表现，按本机角色身份分发：

	// 1) 攻击者本机 —— hitmarker / 命中音 / 准星打勾
	if (IsLocallyControlled())
		OnDealtHit.Broadcast(ImpactPoint, ImpactNormal, Damage, Victim);

	// 2) 被命中角色本机 —— 受伤镜头模糊 / 受击抖动（仅命中角色且该角色由本机控制时）
	if (Victim && Victim->IsLocallyControlled())
		Victim->OnReceivedHit.Broadcast(ImpactPoint, ImpactNormal, Damage, Victim);

	// 3) 世界共享表现 —— 命中音效 / 冲击特效 / 权威弹道（所有端无条件）
	OnHitWorld.Broadcast(ImpactPoint, ImpactNormal, Damage, Victim);
}

// ---------------------------------------------------------------------------
// 鼠标视角 (Axis2D, X=Yaw, Y=Pitch)
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
// 移动状态
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

	// 用控制器朝向计算角色的向前速度（和蓝图逻辑一致）
	const FRotator ControlRotation = Controller->GetControlRotation();
	const FRotator YawOnlyRotation(0, ControlRotation.Yaw, 0);
	const FVector ForwardDir = YawOnlyRotation.RotateVector(FVector::ForwardVector);
	const float ForwardSpeed = Velocity.Dot(ForwardDir); // 角色"前方"的速度分量

	if (bWantsToAim)
	{
		MovementState = EFPSMovementState::Aiming;
	}
	else if (ForwardSpeed > UE_KINDA_SMALL_NUMBER) // 向前移动
	{
		if (bWantsToRun && (Stamina >= RunStartStaminaThreshold || MovementState == EFPSMovementState::Running))
		{
			MovementState = EFPSMovementState::Running;
			// 奔跑优先于开火：切换跑步时强制停火
			// 不调用 StopFire()，避免把 bWantsToFire 清掉（LMB 仍按住）
			if (CurrentWeapon && CurrentWeapon->IsFiring())
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
	else if (bIsMoving) // 移动并且没有向前分量 -> 只有左右/向后移动
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
	default: // Idle, Walking
		GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
		break;
	}
}

// ---------------------------------------------------------------------------
// 体力（服务端权威，Tick 中更新；复制到客户端给 UI）
// ---------------------------------------------------------------------------

void AFPSCharacter::UpdateStamina(float DeltaTime)
{
	// 仅服务端调用（Tick 中已 HasAuthority 守卫）。
	const float OldStamina = Stamina;

	if (MovementState == EFPSMovementState::Running)
	{
		// 真正在向前奔跑（MovementState 已是权威判定）→ 消耗体力。
		Stamina = FMath::Max(0.0f, Stamina - StaminaDrainRate * DeltaTime);

		// 只有体力彻底耗尽（到 0）才强制停跑：长按 Shift 会一直跑到 0。
		// 起跑阈值是"准入门槛"，不是"退出线"—— 跌破阈值但 >0 时仍可继续跑（见 ServerSetWantsToRun）。
		if (Stamina <= 0.0f)
		{
			ServerStopRunForced();
		}
	}
	else
	{
		// 非奔跑 → 回复体力。Idle 与 Walking 用不同速率。
		const float RegenRate = (MovementState == EFPSMovementState::Idle)
			? StaminaRegenRateIdle
			: StaminaRegenRateWalking;
		Stamina = FMath::Min(MaxStamina, Stamina + RegenRate * DeltaTime);
	}

	// 体力变化才广播（避免满体力/空体力时每帧空播）。服务端本机靠此刷 UI，
	// 客户端靠 OnRep_Stamina。UI 更顺滑的做法是 ProgressBar.Percent 绑 GetStamina/GetMaxStamina。
	if (!FMath::IsNearlyEqual(Stamina, OldStamina))
		OnStaminaChanged.Broadcast(Stamina, MaxStamina);
}

void AFPSCharacter::ServerStopRunForced()
{
	// 仅服务端调用。体力不足时由服务端发起的强制停跑 —— 玩家从没调过 StopRun，
	// 所以要同时：① 清服务端 bWantsToRun ② 重算状态/速度 ③ 让所有端动画退出跑步
	// ④ 通知操作者本机清本地 bWantsToRun 并播 OnRunStopped（否则本机动画卡在跑步）。
	bWantsToRun = false;
	UpdateMovementState();   // 重算 → 不再是 Running
	ApplyMovementSpeed();    // 立即切回 WalkSpeed

	// 远端模拟代理（别人客户端上的本角色）靠 multicast 退出跑步动画。
	MulticastOnRunStateChanged(false);

	// 操作者本机：multicast 对 IsLocallyControlled() 会 return（设计上发送方自己跳过），
	// 所以单独发 Client RPC 给操作者，清本地 bWantsToRun + 播 OnRunStopped。
	// Listen server host 自己就是操作者且有 authority，直接本地处理，不走 RPC。
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
	// 操作者客户端本机：清本地输入意图 + 退出跑步动画。
	// 清 bWantsToRun 后，UpdateMovementState（每帧 Tick）就不会再判为 Running，
	// 且玩家就算还按着 Shift，也需松开重按才会重新 StartRun（届时服务端按锁判断是否允许）。
	bWantsToRun = false;
	OnRunStopped.Broadcast();
}

void AFPSCharacter::OnRep_Stamina()
{
	// 客户端收到服务端权威体力 → 广播给 UI / 蓝图。
	OnStaminaChanged.Broadcast(Stamina, MaxStamina);
}

// ---------------------------------------------------------------------------
// 背包 / 道具系统
// ---------------------------------------------------------------------------

void AFPSCharacter::ServerApplyHeal(float Amount)
{
	if (!HasAuthority() || bIsDead || Amount <= 0.0f)
		return;

	Health = FMath::Clamp(Health + Amount, 0.0f, MaxHealth);

	// 服务端本机刷血条（客户端靠 OnRep_Health 复制后广播）。
	OnHealthChanged.Broadcast(Health, MaxHealth);
}

void AFPSCharacter::UseInventoryItem(int32 Index)
{
	// 本地 UI 调用 → 转发服务端权威执行。Host 自己就有 authority，RPC 会本地直接跑。
	ServerUseInventoryItem(Index);
}

void AFPSCharacter::ServerUseInventoryItem_Implementation(int32 Index)
{
	if (Inventory)
		Inventory->ServerUseItem(Index);
}

void AFPSCharacter::DropInventoryItem(int32 Index)
{
	// 本地 UI 调用 → 转发服务端权威执行。
	ServerDropInventoryItem(Index);
}

void AFPSCharacter::ServerDropInventoryItem_Implementation(int32 Index)
{
	if (Inventory)
		Inventory->ServerDropItem(Index);
}

void AFPSCharacter::Interact()
{
	// F 键：发起拾取。本地有 overlap 目标才发（避免空发 RPC），
	// 但服务端不信任客户端的指针 —— 它自己重新查范围（见 ServerTryPickup_Implementation）。
	if (CurrentPickupTarget)
	{
		ServerTryPickup();
	}
}

AFPSPickup* AFPSCharacter::FindBestPickupInRange() const
{
	// 服务端权威：用球形重叠查角色周围的 Pickup，选最近的。
	// 半径取「角色身位 + 各 Pickup 自己的 PickupRadius」—— 用一个够大的查询球先捞，
	// 再按各 Pickup 的真实 PickupRadius 精确判定。
	UWorld* World = GetWorld();
	if (!World)
		return nullptr;

	const FVector Origin = GetActorLocation();
	// 查询球半径用一个保守上限（覆盖常见 PickupRadius，默认 120）。
	const float QueryRadius = 300.0f;

	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FindPickup), false, this);
	World->OverlapMultiByObjectType(
		Overlaps,
		Origin,
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_WorldDynamic),
		FCollisionShape::MakeSphere(QueryRadius),
		Params);

	AFPSPickup* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (const FOverlapResult& O : Overlaps)
	{
		AFPSPickup* Pickup = Cast<AFPSPickup>(O.GetActor());
		if (!Pickup)
			continue;

		const float DistSq = FVector::DistSquared(Origin, Pickup->GetActorLocation());
		// 各 Pickup 用自己的 PickupRadius 做最终范围校验（防越界拾取）。
		const float R = Pickup->GetPickupRadius();
		if (DistSq <= R * R && DistSq < BestDistSq)
		{
			Best = Pickup;
			BestDistSq = DistSq;
		}
	}
	return Best;
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
	// E 键：纯本地 UI 切换。只在本地控制端有意义。
	if (!IsLocallyControlled())
		return;

	// 计分板打开期间（Tab 按住）禁止开/关背包 —— 两个全屏 UI 互斥。
	if (const AFPSPlayerController* PC = Cast<AFPSPlayerController>(GetController()))
	{
		if (PC->IsScoreboardOpen())
			return;
	}

	bInventoryOpen = !bInventoryOpen;

	// 打开时锁住移动/射击等输入（E 键本身不锁，仍能退出）。
	bInputLocked = bInventoryOpen;

	if (bInventoryOpen)
	{
		// 打开瞬间：强制停掉可能正按住的动作，避免状态卡死（一直跑/一直开火）。
		if (bWantsToFire)
			StopFire();
		if (bWantsToRun)
			StopRun();
		if (bWantsToAim)
			StopAim();
	}

	OnToggleInventory(bInventoryOpen);   // 蓝图：建/拆 WBP + 切输入模式 + 显示/隐藏鼠标
}

void AFPSCharacter::CloseInventory()
{
	// 仅在打开时执行。复用 ToggleInventory 的关闭路径（解锁输入 + 触发蓝图 OnToggleInventory(false)）。
	if (!IsLocallyControlled() || !bInventoryOpen)
		return;

	ToggleInventory();   // 当前为打开 → 翻转成关闭
}

void AFPSCharacter::ServerTryPickup_Implementation()
{
	// 服务端权威：自己查范围内最近的 Pickup（不信任客户端传来的指针 —— 它跨 RPC 常变 None）。
	AFPSPickup* Pickup = FindBestPickupInRange();
	if (Pickup)
	{
		// Pickup 自身负责"加进背包 + 销毁"，背包满则保留。
		Pickup->ServerTryPickup(this);
	}
}

void AFPSCharacter::SetPickupTarget(AFPSPickup* Pickup)
{
	if (CurrentPickupTarget == Pickup)
		return;

	CurrentPickupTarget = Pickup;
	OnPickupTargetChanged.Broadcast();   // 蓝图据此显示"按F拾取"提示
}

void AFPSCharacter::ClearPickupTarget(AFPSPickup* Pickup)
{
	// 只有当前目标正是离开/销毁的那个才清 —— 避免范围内有多个 Pickup 时误清。
	if (CurrentPickupTarget != Pickup)
		return;

	CurrentPickupTarget = nullptr;
	OnPickupTargetChanged.Broadcast();   // 蓝图据此隐藏提示
}
