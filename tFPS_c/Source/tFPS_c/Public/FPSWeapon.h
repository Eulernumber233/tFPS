#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FPSWeapon.generated.h"

class AFPSCharacter;
class UFPSAmmoItemDef;
class USphereComponent;

/** 武器状态：推导值，不存字段。
 *  Dropped   = bIsOnGround==true（在地上可被拾取）
 *  Holstered = 在槽位中但不是当前手持（visible=false）
 *  Active    = 在当前活跃槽位，手中持有 */
UENUM(BlueprintType)
enum class EWeaponState : uint8
{
	Dropped   UMETA(DisplayName = "掉落中"),
	Holstered UMETA(DisplayName = "挂载中"),
	Active    UMETA(DisplayName = "手持中")
};

UCLASS()
class TFPS_C_API AFPSWeapon : public AActor
{
	GENERATED_BODY()

public:
	AFPSWeapon();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void BeginPlay() override;

	/** Set the character that owns this weapon (also disables pickup collision). */
	void SetOwningCharacter(AFPSCharacter* InOwnerCharacter);

	/** Override relevancy: always relevant when on ground, otherwise follows owning character. */
	virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget,
		const FVector& SrcLocation) const override;

	/** Start continuous fire (server only) */
	void StartFire();
	void StopFire();
	bool IsFiring() const { return bIsFiring; }

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	AFPSCharacter* GetOwningCharacter() const { return OwningCharacter; }

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	float GetFireRangeEnd() const { return FireRangeEnd; }

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	float GetFireRangeStart() const { return FireRangeStart; }

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	float GetFireRate() const { return FireRate; }

	// ---- 弹药系统（弹夹 = 弹药链，索引0=顶部先发射，末尾=最后装入） ----

	/** 弹夹内子弹总数（遍历弹药链合计）。 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
	int32 GetCurrentAmmo() const;

	/** 备弹总数：背包中所有匹配本武器口径的子弹条目 Count 合计。 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
	int32 GetComputedReserveAmmo() const;

	UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
	int32 GetMagSize() const { return MagSize; }

	UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
	FText GetWeaponName () const { return WeaponName; }

	UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
	bool IsReloading() const { return bIsReloading; }

	UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
	bool CanReload() const;

	void ServerBeginReload();
	void ServerResetAmmo();

	/** 设置弹夹弹药链（拾取掉落武器时恢复）。 */
	void SetLoadedAmmoState(const TArray<struct FWeaponAmmoEntry>& InLoadedAmmo);

	/** 当前弹夹顶部弹药类型（用于伤害加成）。空弹夹返回 nullptr。 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
	UFPSAmmoItemDef* GetLoadedAmmoDef() const;

	/** 弹夹弹药链（只读，掉落时保存弹药状态）。 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
	const TArray<FWeaponAmmoEntry>& GetLoadedAmmo() const { return LoadedAmmo; }

	UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
	FName GetAcceptedCaliber() const { return AcceptedCaliber; }

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	int32 GetWeaponValue() const { return WeaponValue; }

	//UPROPERTY(BlueprintAssignable, Category = "Weapon|Ammo")
	//FOnAmmoChanged OnAmmoChanged;

	UFUNCTION(BlueprintImplementableEvent, Category = "Weapon|Ammo")
	void OnReloadStart();

	UFUNCTION(BlueprintImplementableEvent, Category = "Weapon|Ammo")
	void OnReloadFinish();

	UFUNCTION(BlueprintCallable, Category = "Weapon|Aim")
	void GetAimRay(FVector& OutStart, FVector& OutDirection) const;

	// ---- 拾取/掉落状态 ----

	/** 武器是否在地上（可被拾取），Replicated。 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Pickup")
	bool IsOnGround() const { return bIsOnGround; }

	/** 当前武器状态（蓝图可读）。从 bIsOnGround + Owner 的活跃武器推导。 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	EWeaponState GetWeaponState() const;

	/** 拾取范围半径。 */
	float GetPickupRadius() const { return PickupRadius; }

	/** 服务端：将武器放入世界（从角色身上分离，开启拾取碰撞）。*/
	void PlaceInWorld(const FVector& Location);

	/** 服务端：将武器从世界移除（装备到角色身上，关闭拾取碰撞）。*/
	void RemoveFromWorld();

protected:
	void GetAimViewPoint(FVector& OutLocation, FRotator& OutRotation) const;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Config")
	FText WeaponName;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Config")
	float FireRate = 0.1f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Config")
	float DamageAmount = 34.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Config")
	float FireRangeEnd = 5000.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Config")
	float FireRangeStart = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Ammo")
	int32 MagSize = 30;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Ammo")
	FName AcceptedCaliber = NAME_None;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Ammo")
	TObjectPtr<UFPSAmmoItemDef> InitialAmmoDef = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon|Ammo")
	float ReloadTime = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	int32 WeaponValue = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Debug")
	bool bDrawAuthoritativeTrace = true;

	// ---- 拾取组件 ----

	UPROPERTY(VisibleAnywhere, Category = "Weapon|Pickup")
	TObjectPtr<USphereComponent> PickupSphere;

	/** 生成时即作为场景掉落物（不在玩家手上）。仅在 HasAuthority 时生效。 */
	UPROPERTY(EditAnywhere, Category = "Weapon|Pickup")
	uint8 bStartAsGroundItem : 1 = false;

	UPROPERTY(EditAnywhere, Category = "Weapon|Pickup")
	float PickupRadius = 120.0f;

	UPROPERTY(ReplicatedUsing = OnRep_IsOnGround)
	uint8 bIsOnGround : 1;

	UFUNCTION()
	void OnRep_IsOnGround();

	// ---- 复制属性 ----

	UPROPERTY(Replicated)
	uint8 bIsFiring : 1;

	UPROPERTY(Replicated)
	TObjectPtr<AFPSCharacter> OwningCharacter;

	/** 弹夹弹药链（索引 0=顶部先发射，末尾=底部最后装入）。服务端权威，复制到客户端。 */
	UPROPERTY(ReplicatedUsing = OnRep_Ammo)
	TArray<FWeaponAmmoEntry> LoadedAmmo;

	UPROPERTY(ReplicatedUsing = OnRep_IsReloading)
	uint8 bIsReloading : 1;

	UPROPERTY(Replicated)
	uint8 bReloadCompleted : 1;

	UFUNCTION()
	void OnRep_Ammo();

	UFUNCTION()
	void OnRep_IsReloading();

	void FinishReload();

	FTimerHandle FireTimerHandle;
	FTimerHandle ReloadTimerHandle;

	void HandleFire();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastDrawAuthTrace(FVector AuthTraceStart, FVector AuthTraceEnd);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastHitConfirmed(FVector ImpactPoint, FVector ImpactNormal,
		float Damage, AFPSCharacter* Victim);

	// ---- 拾取 overlap ----

	UFUNCTION()
	void OnPickupSphereBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);

	UFUNCTION()
	void OnPickupSphereEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
