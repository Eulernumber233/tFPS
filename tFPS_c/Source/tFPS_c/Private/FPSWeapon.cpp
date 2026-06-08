#include "FPSWeapon.h"
#include "FPSCharacter.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h"
#include "DrawDebugHelpers.h"
#include "Net/UnrealNetwork.h"

// 子弹/射击专用 Trace 通道，对应 DefaultEngine.ini 里 Name="Weapon" 的 GameTraceChannel1。
// 用专属通道而非 ECC_Visibility，避免与 AI 视线/鼠标拾取等通用 Visibility 用途互相干扰。
#define COLLISION_WEAPON ECC_GameTraceChannel1

AFPSWeapon::AFPSWeapon()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bNetLoadOnClient = true;

	bIsReloading = false;
	bReloadCompleted = false;

	// 提供一个默认 SceneComponent 作为 RootComponent，保证 AttachToActor / AttachToComponent
	// 一定能成功；蓝图子类的 SkeletalMesh 通常应作为子组件挂在它下面（或被蓝图重设为新 root）。
	USceneComponent* DefaultRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultRoot"));
	RootComponent = DefaultRoot;
}

void AFPSWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AFPSWeapon, bIsFiring, COND_None);
	DOREPLIFETIME_CONDITION(AFPSWeapon, OwningCharacter, COND_None);
	DOREPLIFETIME_CONDITION(AFPSWeapon, CurrentAmmo, COND_None);
	DOREPLIFETIME_CONDITION(AFPSWeapon, ReserveAmmo, COND_None);
	DOREPLIFETIME_CONDITION(AFPSWeapon, bIsReloading, COND_None);
	DOREPLIFETIME_CONDITION(AFPSWeapon, bReloadCompleted, COND_None);
}

void AFPSWeapon::SetOwningCharacter(AFPSCharacter* InOwnerCharacter)
{
	OwningCharacter = InOwnerCharacter;
	SetOwner(InOwnerCharacter);
}

// 只对"能看到攻击者角色"的客户端相关，其他客户端（比如完全被墙遮挡的）不相关，节省网络带宽。
bool AFPSWeapon::IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget,
	const FVector& SrcLocation) const
{
	if (OwningCharacter)
		return OwningCharacter->IsNetRelevantFor(RealViewer, ViewTarget, SrcLocation);
	return Super::IsNetRelevantFor(RealViewer, ViewTarget, SrcLocation);
}

// ===================================================================
// Firing —— C++ 只做服务端权威逻辑 + 权威 debug line。
// 蓝图特效完全由 AFPSCharacter::OnFireStarted / OnFireStopped 事件驱动。
// ===================================================================

void AFPSWeapon::StartFire()
{
	if (!HasAuthority())
		return;

	if (!OwningCharacter || OwningCharacter->IsDead())
		return;

	// 换弹中不能开火；弹夹空时不开火（也不空响计时），由换弹补给。
	if (bIsReloading || CurrentAmmo <= 0)
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

	// 弹夹空 → 停火（不自动换弹，由玩家按 R）。OnRep_Ammo 已在上一发归零时刷过 UI。
	if (CurrentAmmo <= 0)
	{
		StopFire();
		return;
	}

	// 消耗一发（服务端权威）。OnRep_Ammo 复制到客户端刷 UI；服务端本机手动广播。
	--CurrentAmmo;
	OnRep_Ammo();

	FVector ViewLocation;
	FRotator ViewRotation;
	GetAimViewPoint(ViewLocation, ViewRotation);

	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * FireRangeEnd;
	const FVector TraceStart = ViewLocation + ViewRotation.Vector() * FireRangeStart;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(OwningCharacter);
	QueryParams.AddIgnoredActor(this);
	// bTraceComplex=false：命中走 Physics Asset 的简化碰撞体（角色 Mesh 需配 Physics Asset），
	// 比逐三角面便宜得多，且足够精准。bReturnPhysicalMaterial 为日后部位判定/表面特效预留。
	QueryParams.bTraceComplex = false;
	QueryParams.bReturnPhysicalMaterial = true;

	FHitResult Hit;
	const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
		Hit, TraceStart, TraceEnd, COLLISION_WEAPON, QueryParams);
	// Hit.GetActor() 可能是任意 Actor（墙/地板/道具/角色）。先权威结算伤害，
	// 再把命中信息广播给所有端做特效。Victim 只有命中角色时才非空。
	AFPSCharacter* Victim = nullptr;
	if (bBlocked)
	{
		if (AActor* HitActor = Hit.GetActor())
		{
			Victim = Cast<AFPSCharacter>(HitActor);

			AController* InstigatorController = OwningCharacter->GetController();
			FPointDamageEvent DamageEvent(DamageAmount, Hit, -ViewRotation.Vector(), nullptr);
			HitActor->TakeDamage(DamageAmount, DamageEvent, InstigatorController, OwningCharacter);
		}
	}

	const FVector AuthEnd = bBlocked ? Hit.ImpactPoint : TraceEnd;
	MulticastDrawAuthTrace(TraceStart, AuthEnd);

	// 命中信息广播给所有端，由各端本机分发成三种蓝图特效通知。
	// 未命中任何东西（bBlocked=false）则不发 —— 没有命中点也无受击/世界冲击可言。
	// 对于无论是否命中都需要的特效（比如音效、子弹路径）移到fpscharacter里的OnFireStarted.Broadcast()，在蓝图里循环播放
	if (bBlocked)
		MulticastHitConfirmed(Hit.ImpactPoint, Hit.ImpactNormal, DamageAmount, Victim);
}

void AFPSWeapon::GetAimViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	if (!OwningCharacter)
		return;

	// 起点：用 PlayerCameraManager 的相机世界位置（= 玩家眼睛 = 屏幕渲染原点）。
	// 方向：用 ControlRotation —— 纯鼠标输入累积，不受 Arm 动画 Sway/Lean 扰动。
	//
	// 为什么分两路：Camera 挂在 Arm 下，Arm 跑 FPS 移动动画时会晃。
	// 如果方向跟着 Camera 朝向取，移动时方向就会跟着动画晃 → 本机看着"射线偏屏幕中心"。
	// ControlRotation 是引擎专门设计的纯输入瞄准方向。
	if (AController* C = OwningCharacter->GetController())
	{
		FRotator UnusedRot;
		C->GetPlayerViewPoint(OutLocation, UnusedRot);  // 只取相机位置
		OutRotation = C->GetControlRotation();          // 方向单独用 ControlRotation
		return;
	}

	// 远端模拟代理（其他客户端上的别人角色）没有 Controller，
	// 回退到 Pawn 位置 + ArmPitch（已 Replicated） + Actor Yaw
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

	// 起点向前偏移 50cm，避免遮挡第一人称视野
	const FVector DrawStart = AuthTraceStart
		+ (AuthTraceEnd - AuthTraceStart).GetSafeNormal() * 50.0f;
	DrawDebugLine(GetWorld(), DrawStart, AuthTraceEnd,
		FColor::Yellow, false, 0.1f, 0, 0.3f);
}

void AFPSWeapon::MulticastHitConfirmed_Implementation(FVector ImpactPoint, FVector ImpactNormal,
	float Damage, AFPSCharacter* Victim)
{
	// 所有端收到同一份命中信息。交给攻击者角色做本机角色身份判断 + 三种事件分发。
	if (OwningCharacter)
		OwningCharacter->DispatchHitFX(ImpactPoint, ImpactNormal, Damage, Victim);
}

// ===================================================================
// 弹药系统 —— 服务端权威，复制到客户端，OnRep 广播 OnAmmoChanged 刷 UI。
// 蓝图子类只填 MagSize / InitialReserveAmmo / ReloadTime 数值。
// ===================================================================

void AFPSWeapon::ServerResetAmmo()
{
	if (!HasAuthority())
		return;

	GetWorldTimerManager().ClearTimer(ReloadTimerHandle);
	bReloadCompleted = false;  // 复活时若正在换弹 → 视为被打断（非正常完成）
	bIsReloading = false;
	CurrentAmmo = MagSize;
	ReserveAmmo = InitialReserveAmmo;

	// 服务端本机手动触发 OnRep（客户端靠复制自动触发）。
	OnRep_Ammo();
	OnRep_IsReloading();
}

bool AFPSWeapon::CanReload() const
{
	return !bIsReloading && CurrentAmmo < MagSize && ReserveAmmo > 0;
}

void AFPSWeapon::ServerBeginReload()
{
	if (!HasAuthority())
		return;

	if (!OwningCharacter || OwningCharacter->IsDead())
		return;

	if (!CanReload())
		return;

	// 换弹打断开火（权威停火 timer），换弹中 StartFire 会被 bIsReloading 挡掉。
	StopFire();

	bReloadCompleted = false;  // 标记"进行中"，须在 bIsReloading 之前写（OnRep 顺序保证）
	bIsReloading = true;
	OnRep_IsReloading();  // 服务端本机触发换弹动画事件 + 进度条开始（客户端靠复制）

	GetWorldTimerManager().SetTimer(ReloadTimerHandle, this,
		&AFPSWeapon::FinishReload, ReloadTime, false);
}

void AFPSWeapon::FinishReload()
{
	if (!HasAuthority())
		return;

	// 从备弹补到满夹，不超过备弹存量。
	const int32 Needed = MagSize - CurrentAmmo;
	const int32 ToLoad = FMath::Min(Needed, ReserveAmmo);
	CurrentAmmo += ToLoad;
	ReserveAmmo -= ToLoad;

	bReloadCompleted = true;  // 正常补弹完成（区别于被打断）
	bIsReloading = false;

	OnRep_Ammo();        // 服务端本机刷 UI（客户端靠复制）
	OnRep_IsReloading(); // 服务端本机触发 OnReloadFinish + 进度条完成（客户端靠复制）
}

void AFPSWeapon::OnRep_Ammo()
{
	OnAmmoChanged.Broadcast(CurrentAmmo, ReserveAmmo);
}

void AFPSWeapon::OnRep_IsReloading()
{
	// 武器自身的换弹动画事件（蓝图播换弹蒙太奇/音效），保持原行为。
	if (bIsReloading)
		OnReloadStart();
	else
		OnReloadFinish();

	// 转发到角色的统一进度条接口（换弹只是众多"持续动作"中的一种）。
	// 进度值不走网络：只发开始(带 ReloadTime)/结束(带是否正常完成)，WBP 本地插值。
	if (OwningCharacter)
	{
		if (bIsReloading)
			OwningCharacter->BeginProgress(EFPSProgressType::Reload, ReloadTime);
		else
			OwningCharacter->EndProgress(EFPSProgressType::Reload, bReloadCompleted);
	}
}
