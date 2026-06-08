#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FPSWeapon.generated.h"

class AFPSCharacter;

/** 弹药变化事件：弹夹数 / 备弹数。每端本机广播（服务端权威值复制到客户端后由 OnRep 触发）。
 *  UI / 蓝图订阅此事件即可在弹药变化时刷新弹药 HUD，无需每帧轮询。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAmmoChanged, int32, CurrentAmmo, int32, ReserveAmmo);

UCLASS()
class TFPS_C_API AFPSWeapon : public AActor
{
	GENERATED_BODY()

public:
	AFPSWeapon();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Set the character that owns this weapon */
	void SetOwningCharacter(AFPSCharacter* InOwnerCharacter);

	/** Override relevancy: weapon is relevant to anyone who can see the owning character */
	virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget,
		const FVector& SrcLocation) const override;

	/** Start continuous fire (server only — 服务端权威开火 + LineTrace + 伤害) */
	void StartFire();

	/** Stop continuous fire (server only) */
	void StopFire();

	/** Is currently firing? */
	bool IsFiring() const { return bIsFiring; }

	/** 蓝图层获取武器持有者（所有端可用，OwningCharacter 是 Replicated） */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	AFPSCharacter* GetOwningCharacter() const { return OwningCharacter; }

	/** 武器射程（蓝图特效层可用作 Niagara Beam 终点默认距离） */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	float GetFireRangeEnd() const { return FireRangeEnd; }

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	float GetFireRangeStart() const { return FireRangeStart; }

	/** 单发间隔（蓝图特效层可用作本地 timer 间隔） */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	float GetFireRate() const { return FireRate; }

	// ---- 弹药系统（服务端权威，所有端可读，UI 用 OnAmmoChanged 刷新） ----

	/** 当前弹夹内子弹数 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
	int32 GetCurrentAmmo() const { return CurrentAmmo; }

	/** 当前备弹数（弹夹外剩余） */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
	int32 GetReserveAmmo() const { return ReserveAmmo; }

	/** 弹夹容量（蓝图子类配置的最大值） */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
	int32 GetMagSize() const { return MagSize; }

	/** 是否正在换弹（换弹期间不能开火） */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
	bool IsReloading() const { return bIsReloading; }

	/** 能否换弹：未满夹 且 有备弹 且 未在换弹中（服务端 / 本地预判都可调） */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
	bool CanReload() const;

	/** 开始换弹（server only）——校验后启动换弹计时，结束时从备弹补满弹夹 */
	void ServerBeginReload();

	/** 重置弹药到满夹 + 初始备弹（server only）——角色复活时由 Character 调用 */
	void ServerResetAmmo();

	/** 弹药变化时广播（弹夹/备弹任一变化）——UI 订阅刷新弹药 HUD */
	UPROPERTY(BlueprintAssignable, Category = "Weapon|Ammo")
	FOnAmmoChanged OnAmmoChanged;

	/** 换弹开始（每端本地）——蓝图实现换弹动画/音效 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Weapon|Ammo")
	void OnReloadStart();

	/** 换弹完成（每端本地）——蓝图复位换弹动画状态 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Weapon|Ammo")
	void OnReloadFinish();

	/** 蓝图层射线/特效的统一起点和方向 —— 与 C++ HandleFire 用同一数据源（PlayerCameraManager）。
	 *  这样蓝图 LineTrace 命中点 = 服务端权威命中点 = 屏幕准心位置，三者完全一致。
	 *  禁止蓝图用 Camera 组件的 GetWorldLocation/GetForwardVector，因为 Arm 上的 Camera
	 *  会跟随 FPS 手臂动画晃动，导致视线偏移。 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Aim")
	void GetAimRay(FVector& OutStart, FVector& OutDirection) const;

protected:
	/** 解析瞄准起点和方向：本地控制端用 PlayerViewPoint（相机），远端模拟代理回退到 ArmPitch + Actor Yaw */
	void GetAimViewPoint(FVector& OutLocation, FRotator& OutRotation) const;

	/** Time (seconds) between shots */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Config")
	float FireRate = 0.1f;

	/** Damage per hit */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Config")
	float DamageAmount = 34.0f;

	/** Max trace distance */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Config")
	float FireRangeEnd = 5000.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Config")
	float FireRangeStart = 0.5f;

	// ---- 弹药配置（蓝图子类填数值） ----

	/** 弹夹容量 */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Ammo")
	int32 MagSize = 30;

	/** 初始备弹数（开局/复活时的弹夹外子弹） */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Ammo")
	int32 InitialReserveAmmo = 120;

	/** 换弹耗时（秒） */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Ammo")
	float ReloadTime = 2.0f;

	/** 是否在本机绘制服务端权威射线的 debug line。
	 *  - 所有端独立判断（服务端把命中点 Multicast 给所有端，每端按本机开关决定画不画）
	 *  - 蓝图可读写：测试时打开对比 Niagara Beam 与权威射线，正式使用时关闭
	 *  - 默认 true 方便调试 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Debug")
	bool bDrawAuthoritativeTrace = true;

	UPROPERTY(Replicated)
	uint8 bIsFiring : 1;

	UPROPERTY(Replicated)
	TObjectPtr<AFPSCharacter> OwningCharacter;

	// ---- 弹药运行时状态（服务端权威，复制到客户端） ----

	/** 当前弹夹内子弹数。RepNotify → 客户端广播 OnAmmoChanged 刷 UI */
	UPROPERTY(ReplicatedUsing = OnRep_Ammo)
	int32 CurrentAmmo = 0;

	/** 当前备弹数。RepNotify → 客户端广播 OnAmmoChanged 刷 UI */
	UPROPERTY(ReplicatedUsing = OnRep_Ammo)
	int32 ReserveAmmo = 0;

	/** 是否换弹中（复制，远端动画/本端禁火都要读） */
	UPROPERTY(ReplicatedUsing = OnRep_IsReloading)
	uint8 bIsReloading : 1;

	/** 上一次换弹是否正常完成（true=补弹完成，false=被打断/取消）。
	 *  与 bIsReloading 一起复制：bIsReloading 变 false 时，此标记告诉各端的
	 *  进度条接口该次换弹是走完了还是被打断了（决定 OnProgressEnd 的 bCompleted）。 */
	UPROPERTY(Replicated)
	uint8 bReloadCompleted : 1;

	/** 弹药复制到客户端时广播 OnAmmoChanged */
	UFUNCTION()
	void OnRep_Ammo();

	/** 换弹状态复制到客户端时触发本地换弹动画事件 */
	UFUNCTION()
	void OnRep_IsReloading();

	/** 服务端换弹计时结束：从备弹补满弹夹 */
	void FinishReload();

	FTimerHandle FireTimerHandle;
	FTimerHandle ReloadTimerHandle;

	/** Perform one shot — line trace + damage on server */
	void HandleFire();

	/** 服务端 LineTrace 完成后广播：各端按 bDrawAuthoritativeTrace 选项画权威 debug line。
	 *  不触发任何蓝图特效事件 —— 特效完全由蓝图层听角色的 OnFireStarted/OnFireStopped 自行驱动 */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastDrawAuthTrace(FVector AuthTraceStart, FVector AuthTraceEnd);

	/** 服务端权威命中后广播给所有端的一份命中信息。
	 *  所有端收到完全相同的数据，由各端本机自行判断角色并分发成三种蓝图特效通知：
	 *    - 攻击者本机 → OnDealtHit（hitmarker UI）
	 *    - 被命中角色本机 → OnReceivedHit（受伤反馈）
	 *    - 所有端 → OnHitWorld（音效/冲击/弹道，世界共享表现）
	 *  Victim 命中环境时为 nullptr。Unreliable：纯特效，高频开火下丢一两发无感。 */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastHitConfirmed(FVector ImpactPoint, FVector ImpactNormal,
		float Damage, AFPSCharacter* Victim);
};
