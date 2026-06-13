#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FPSItemDef.h"
#include "FPSPickup.generated.h"

class UFPSItemDef;
class USphereComponent;
class USceneComponent;
class UStaticMeshComponent;
class AFPSCharacter;

/**
 * 地上的拾取物 Actor（复制）。
 *
 * C++ 提供完整组件层级：
 *   - DefaultRoot（USceneComponent）
 *   - PickupMeshComponent（UStaticMeshComponent）：OnConstruction/BeginPlay 从 ItemDef->PickupMesh 自动赋值
 *   - PickupSphere（USphereComponent）：拾取范围触发器
 *
 * 通用用法：建一个 BP_Pickup_Generic 继承本类，不需要加任何组件。
 *   ItemDef 的 PickupMesh 填什么模型，地上就显示什么模型。
 * 特殊用法：需要专属粒子/动画的道具，单独建 BP_Pickup_xxx 子类，
 *   DA 的 PickupMesh 留空、DropPickupClass 指向专用子类。
 *
 * 交互（走近按 F）：
 *   玩家进入 Sphere → 角色端记录"当前可拾取目标"（蓝图据此显示"按F"提示）
 *   按 F → 角色发 ServerTryPickup → 这里把 ItemDef 塞进玩家背包 → 成功则 Destroy
 *
 * ItemDef 由蓝图子类或在场景中手摆时指定（指向某个 DA_xxx 数据资产）。
 */
UCLASS()
class TFPS_C_API AFPSPickup : public AActor
{
	GENERATED_BODY()

public:
	AFPSPickup();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 这个 Pickup 代表哪种道具（蓝图子类指定，指向 DA_xxx 资产）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup")
	TObjectPtr<UFPSItemDef> ItemDef = nullptr;

	/**
	 * 服务端：把"丢弃出来的道具"的运行时状态写进本 Pickup。
	 * 让捡回来时还原当时的耐久/数量（而不是又变成满耐久的 DefaultValue）。
	 * 地图手摆的 Pickup 不调此函数 → bHasOverride=false → 拾取走 DefaultValue。
	 */
	void SetDroppedState(const FInventoryEntry& Entry);

	/**
	 * 服务端：尝试把本道具交给 Picker。
	 * 成功（背包未满）→ 加进背包并 Destroy；失败（背包满）→ 不销毁，触发蓝图提示。
	 * 由角色按 F 时调用（角色端走 ServerTryPickup RPC 后到这里）。
	 */
	void ServerTryPickup(AFPSCharacter* Picker);

	/** 蓝图读取：用于"按F拾取"提示文字。 */
	UFUNCTION(BlueprintCallable, Category = "Pickup")
	UFPSItemDef* GetItemDef() const { return ItemDef; }

	/** 拾取范围半径（服务端做范围校验时用）。 */
	float GetPickupRadius() const { return PickupRadius; }

	/** 背包满拾取失败时触发（仅服务端 Picker 本机有意义，这里简单做成所有端可绑） */
	UFUNCTION(BlueprintImplementableEvent, Category = "Pickup")
	void OnPickupBlocked();

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	/** C++ 默认根：保证蓝图子类无论是否重设 root，attach 都成功（同 AFPSWeapon）。 */
	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<USceneComponent> DefaultRoot;

	/** 地上显示的网格体。OnConstruction 从 ItemDef->PickupMesh 自动赋值。专用子类可不设 DA 的 PickupMesh，改在蓝图层替换此组件。 */
	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<UStaticMeshComponent> PickupMeshComponent;

	/** 拾取范围触发器。 */
	UPROPERTY(VisibleAnywhere, Category = "Pickup")
	TObjectPtr<USphereComponent> PickupSphere;

	/** 拾取范围半径（蓝图可调）。 */
	UPROPERTY(EditAnywhere, Category = "Pickup")
	float PickupRadius = 120.0f;

	/** 是否携带丢弃时的运行时状态（true=用 OverrideEntry 的耐久/数量，false=用 ItemDef->DefaultValue）。 */
	UPROPERTY(Replicated, BlueprintReadWrite, EditDefaultsOnly, Category = "Pickup")
	bool bHasOverride = false;

	/** 丢弃时携带的运行时状态（耐久/数量），bHasOverride 为 true 时拾取按它入背包。 */
	UPROPERTY(Replicated)
	FInventoryEntry OverrideEntry;

	/** 从 ItemDef->PickupMesh 读取并赋给 PickupMeshComponent。OnConstruction + BeginPlay 各调一次。 */
	void ApplyPickupMeshFromItemDef();

	UFUNCTION()
	void OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);

	UFUNCTION()
	void OnSphereEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
