#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "FPSItemDef.h"
#include "FPSCharacter.generated.h"

class AFPSWeapon;
class AFPSPlayerController;
class AFPSPickup;
class AFPSSubmissionPoint;
class UFPSInventoryComponent;
class UFPSInteractionComponent;
class UInputMappingContext;
class UInputAction;

/** 可被进度条显示的「持续动作」类型。同一时刻只有一个进度活跃，
 *  新进度开始会打断旧进度（UI 单进度条模型）。新增动作类型在此追加。 */
UENUM(BlueprintType)
enum class EFPSProgressType : uint8
{
	None	UMETA(DisplayName = "None"),
	Reload	UMETA(DisplayName = "Reload"),
	UseItem	UMETA(DisplayName = "UseItem")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnActionEvent);

/** 持续动作进度开始：动作类型 / 总时长（秒）。每端本机广播。
 *  HUD 订阅后设置进度条标题并本地用 Duration 插值算 0~1 进度（进度值不走网络）。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnProgressBegin, EFPSProgressType, Type, float, Duration);

/** 持续动作进度结束：动作类型 / 是否正常完成（false=被打断/取消）。每端本机广播。
 *  HUD 订阅后清理进度条。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnProgressEnd, EFPSProgressType, Type, bool, bCompleted);

/** 血量变化事件：当前血量 / 最大血量。每端本机广播（服务端改血后 OnRep_Health 在客户端触发）。
 *  UI 订阅此事件即可在血量变化时刷新血条，无需每帧轮询。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, float, Health, float, MaxHealth);

/** 体力变化事件：当前体力 / 最大体力。体力每帧连续变化，UI 用 GetStamina/GetMaxStamina
 *  做 ProgressBar 的 Percent binding 更顺滑；此事件用于"耗尽/恢复"等离散时刻的反馈。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStaminaChanged, float, Stamina, float, MaxStamina);

/** 命中特效事件载荷：服务端权威命中信息（命中点/法线为占位，伤害为权威值，Victim 命中环境时为空） */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnHitEvent,
	FVector, ImpactPoint, FVector, ImpactNormal, float, Damage, AFPSCharacter*, Victim);

/** HoT 持续回血 buff 状态（复制给本人，驱动 buff UI 的独立进度条）。 */
USTRUCT(BlueprintType)
struct FHoTState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bActive = false;

	UPROPERTY(BlueprintReadOnly)
	float RemainingDuration = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float MaxDuration = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float HealPerSecond = 0.0f;
};

/** HoT buff 状态变化 —— HUD 订阅显示/更新/隐藏 buff 独立进度条。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnHoTChanged, bool, bActive, float, RemainingDuration, float, MaxDuration);

// ---- 武器系统信号（蓝图 UI 订阅） ----

/** 拾取武器到槽位（空槽放入时触发）。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWeaponPickedUp, AFPSWeapon*, Weapon, int32, Slot);

/** 交换武器（丢弃手持 + 拾取新武器放同槽时触发）。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWeaponSwapped, AFPSWeapon*, OldWeapon, AFPSWeapon*, NewWeapon);

/** 切换活跃槽位（按 1/2 键时触发）。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeaponSlotSwitched, int32, NewSlot);

UENUM(BlueprintType)
enum class EFPSMovementState : uint8
{
	Idle	UMETA(DisplayName = "Idle"),
	Walking	UMETA(DisplayName = "Walking"),
	Running	UMETA(DisplayName = "Running"),
	Aiming	UMETA(DisplayName = "Aiming")
};

UENUM(BlueprintType)
enum class EFPSWeaponFireState : uint8
{
	Idle	UMETA(DisplayName = "Idle"),
	Firing	UMETA(DisplayName = "Firing")
};

UCLASS()
class TFPS_C_API AFPSCharacter : public ACharacter
{
	GENERATED_BODY()

	friend class AFPSPlayerController;

public:
	AFPSCharacter();

	virtual void Tick(float DeltaTime) override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual float TakeDamage(float Damage, FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Health changed on server — replicate to clients */
	UFUNCTION()
	void OnRep_Health(float OldHealth);

	/** Death state changed — replicate to clients */
	UFUNCTION()
	void OnRep_bIsDead();

	UFUNCTION(BlueprintCallable, Category = "Health")
	bool IsDead() const { return bIsDead; }

	UFUNCTION(BlueprintCallable, Category = "Health")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintCallable, Category = "Health")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintCallable, Category = "Health")
	float GetRespawnDelay() const { return RespawnDelay; }

	// ---- 体力（服务端权威，复制到客户端给 UI） ----

	/** 当前体力值（运行时，蓝图/UI 读取） */
	UFUNCTION(BlueprintCallable, Category = "Stamina")
	float GetStamina() const { return Stamina; }

	/** 总体力值（最大值，蓝图/UI 读取） */
	UFUNCTION(BlueprintCallable, Category = "Stamina")
	float GetMaxStamina() const { return MaxStamina; }

	/** 起跑体力线（运行时读取）：体力必须 > 此值才能从其他状态进入奔跑（准入门槛，非退出线） */
	UFUNCTION(BlueprintCallable, Category = "Stamina")
	float GetRunStartStaminaThreshold() const { return RunStartStaminaThreshold; }

	/** 当前能否起跑（实时计算，所有端可用）：体力 > 起跑线即可。
	 *  注意这只判"能否起跑"，不代表正在跑——已经在跑的角色体力低于阈值仍会继续跑到 0。
	 *  UI 可用它在体力 <= 阈值时提示"无法奔跑"。 */
	UFUNCTION(BlueprintCallable, Category = "Stamina")
	bool CanStartRun() const { return Stamina > RunStartStaminaThreshold; }

	/** 服务端权威复活：传送到出生点、复位血量/碰撞/移动/武器、清死亡状态。
	 *  由 GameMode 在死亡延迟结束后调用（也可被其它复活逻辑复用）。public 供 GameMode 访问。 */
	void Respawn(const FVector& SpawnLocation, const FRotator& SpawnRotation);

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	AFPSWeapon* GetPrimaryWeapon() const { return PrimaryWeapon; }

	/** 当前活跃武器（0=主武器，1=副武器）。蓝图层读取用于开火/换弹等操作。 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	AFPSWeapon* GetActiveWeapon() const;

	/** 副武器（第二把枪）。服务端权威，复制到所有端。 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	AFPSWeapon* GetSecondaryWeapon() const { return SecondaryWeapon; }

	/** 当前活跃武器槽位（0=主武器，1=副武器）。 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	int32 GetActiveWeaponSlot() const { return ActiveWeaponSlot; }

	/** 获取指定槽位的武器（0或1）。越界返回 nullptr。 */
	AFPSWeapon* GetWeaponInSlot(int32 Slot) const;

	// ---- 武器拾取系统 ----

	/** 设置当前可拾取武器目标（武器 Pickup 进入范围时调用，纯本地）。 */
	void SetWeaponPickupTarget(AFPSWeapon* Weapon);

	/** 清除可拾取武器目标。仅当 Pickup 是当前目标才清。 */
	void ClearWeaponPickupTarget(AFPSWeapon* Weapon);

	/** 当前可拾取武器目标（从交互管理器查询，蓝图读取）。 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	AFPSWeapon* GetWeaponPickupTarget() const;

	// ---- 背包 / 道具系统 ----

	/** 背包组件（所有端可读，蓝图 UI 用它遍历格子 / 订阅 OnInventoryChanged）。 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	UFPSInventoryComponent* GetInventory() const { return Inventory; }

	/**
	 * 服务端权威加血（道具治疗调用）。Clamp 到 MaxHealth，复制到所有端并广播血条事件。
	 * public 供 UFPSHealItemDef::ServerUseItem 调用。
	 */
	void ServerApplyHeal(float Amount);

	/**
	 * 通过 UI 使用背包第 Index 格道具（本地调用）→ 转发服务端权威执行。
	 * WBP_Inventory 点击格子时调这个（BlueprintCallable）。
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void UseInventoryItem(int32 Index);

	/**
	 * 通过 UI 丢弃背包第 Index 格道具（本地调用）→ 转发服务端权威执行。
	 * WBP 右键菜单"丢弃"调这个。服务端从背包移除并在脚下生成 Pickup。
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void DropInventoryItem(int32 Index);

	/**
	 * 拖拽移动背包道具（本地调用 → 服务端权威重排）。
	 * WBP 拖拽释放后调这个。FromIndex=被拖拽道具索引，ToGridX/ToGridY=吸附到的网格坐标。
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void MoveInventoryItem(int32 FromIndex, int32 ToGridX, int32 ToGridY);

	/** 设置当前可拾取目标（Pickup 进入范围时调用，纯本地，给蓝图显示"按F"提示）。 */
	void SetPickupTarget(AFPSPickup* Pickup);

	/** 清除可拾取目标（Pickup 离开范围 / 被销毁时调用）。仅当 Pickup 是当前目标才清。 */
	void ClearPickupTarget(AFPSPickup* Pickup);

	/** 当前可拾取目标（从交互管理器查询，蓝图读取）。 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	AFPSPickup* GetPickupTarget() const;

	// ---- 交互管理系统 ----

	/** 交互管理器（管理所有 F 键交互目标的列表 + 滚轮切换）。 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	UFPSInteractionComponent* GetInteractionManager() const { return InteractionManager; }

	/** 切换到下一个交互目标（滚轮下滚调用）。 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void CycleInteractionNext();

	/** 切换到上一个交互目标（滚轮上滚调用）。 */
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void CycleInteractionPrev();

	// ---- 物品提交系统 ----

	/** 设置当前范围内提交点（进入范围 / 提交点开放时调用，纯本地）。 */
	void SetSubmissionTarget(AFPSSubmissionPoint* Point);

	/** 清除当前提交点（离开范围 / 提交点关闭时调用）。 */
	void ClearSubmissionTarget(AFPSSubmissionPoint* Point);

	/** 当前可提交目标（从交互管理器查询，蓝图读取）。 */
	UFUNCTION(BlueprintCallable, Category = "Submission")
	AFPSSubmissionPoint* GetSubmissionTarget() const;

	/** 是否可以在当前提交点提交物品（附近有开放提交点 + 背包有贵重品）。 */
	UFUNCTION(BlueprintCallable, Category = "Submission")
	bool CanSubmitItems() const;

	/**
	 * 单独提交背包第 Index 格的道具（右键菜单"提交"调用 → 转发服务端）。
	 * 需先通过 CanSubmitItems() 判可提交。
	 */
	UFUNCTION(BlueprintCallable, Category = "Submission")
	void SubmitInventoryItem(int32 Index);

	/**
	 * 切换背包面板（按 E，本地表现）。C++ 只负责绑 E 键并转发到这里，
	 * 由蓝图实现：Create/移除 WBP_Inventory + SetInputModeGameAndUI / 显示鼠标。
	 * 纯本地 UI，不走网络。bShown=当前应显示还是隐藏。
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory")
	void OnToggleInventory(bool bShown);

	/** 背包当前是否打开（本地状态，给蓝图/输入逻辑读）。 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool IsInventoryOpen() const { return bInventoryOpen; }

	/**
	 * 关闭背包（供蓝图调用：点面板外空白处 / 点关闭按钮）。
	 * 走和 E 键相同的关闭路径，确保 bInputLocked 一并解锁、OnToggleInventory(false) 触发。
	 * 已关闭时调用无副作用。
	 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void CloseInventory();

	/**
	 * 强制停止开火与瞄准（本地）。供 Controller 在计分板锁鼠标瞬间调用，
	 * 兜底清掉"锁定那一刻可能正按住的开火/瞄准"，避免状态卡死。已停则无副作用。
	 */
	void ForceStopFireAndAim();

	/**
	 * 角色是否处于"动作锁定"状态（死亡/背包打开/使用道具中/换弹中）。
	 * 锁定时禁用开火/瞄准/奔跑/换弹/拾取，但不影响 WASD 移动和鼠标视角。
	 */
	UFUNCTION(BlueprintCallable, Category = "Input")
	bool IsActionLocked() const;

	/** 是否正在使用道具（引导治疗/HoT 使用中）。蓝图可读，用于动画/输入判断。 */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool IsUsingItem() const { return bIsUsingItem; }

	/** HoT buff 当前状态（复制给本人，UI 读此画 buff 进度条）。 */
	UFUNCTION(BlueprintCallable, Category = "Buff")
	const FHoTState& GetHoTState() const { return HoTState; }

	/** 当前移动状态 — 蓝图层读取用于动画/视觉 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	EFPSMovementState MovementState = EFPSMovementState::Idle;

	/** 当前武器开火状态 — 蓝图层读取用于动画/视觉 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
	EFPSWeaponFireState WeaponFireState = EFPSWeaponFireState::Idle;

	/** 当前活跃的进度动作（本地状态，不复制）— 用于打断判断与 UI 显隐 */
	EFPSProgressType ActiveProgress = EFPSProgressType::None;

	/** 每次开火时触发 — 蓝图层实现枪口火焰/音效 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Weapon")
	void OnFire();

	/** 停止开火时触发 — 蓝图层实现特效复位 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Weapon")
	void OnStopFire();

	/** 武器就绪（服务端生成后 / 客户端收到复制后）— 蓝图实现挂载到手臂骨骼 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Weapon")
	void OnPrimaryWeaponEquipped();

	/** 副武器就绪 — 蓝图实现挂载到手臂骨骼（同 OnPrimaryWeaponEquipped，但拿 GetSecondaryWeapon()）。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Weapon")
	void OnSecondaryWeaponEquipped();

	/**
	 * 服务端：将武器安装到指定槽位（0=主，1=副）。
	 * 如果槽位已有武器，旧武器会被替换（但不掉落——调用方负责先处理旧武器）。
	 * 新武器设为活跃武器，旧活跃武器隐藏。
	 */
	void EquipWeapon(AFPSWeapon* Weapon, int32 Slot);

	/**
	 * 切换活跃武器槽（本地调用 → 服务端权威切换）。
	 * 如果目标槽位无武器则忽略。
	 */
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void SwitchWeapon(int32 NewSlot);

	// ---- 持续动作进度条接口（每端本地广播，纯表现）----
	// 换弹/用药等"需要一段时间完成"的动作统一走这两个委托，HUD 只维护一个进度条。
	// 用 BlueprintAssignable 委托而非 BlueprintImplementableEvent：HUD WBP 由 Controller
	// 创建、Character 不持有其引用，委托让 Character 只管 Broadcast，WBP 自行订阅，彻底解耦
	// （与项目里 OnHealthChanged / OnAmmoChanged 的事件驱动模式一致）。
	// 进度值不走网络：只广播"开始(带总时长)/结束"两个离散信号，WBP 本地用 Duration 插值。

	/** 进度动作开始 — HUD 订阅后设进度条标题、本地起 timer 用 Duration 插值。
	 *  若已有进度活跃，C++ 会先广播一次 OnProgressEnd(旧Type, false) 再广播本事件。 */
	UPROPERTY(BlueprintAssignable, Category = "Progress")
	FOnProgressBegin OnProgressBegin;

	/** 进度动作结束 — HUD 订阅后清理进度条。
	 *  bCompleted=true 正常完成；false 被打断/取消（如换弹途中复活、被新动作顶替）。 */
	UPROPERTY(BlueprintAssignable, Category = "Progress")
	FOnProgressEnd OnProgressEnd;

	/** 当前活跃的进度类型（None=无）。蓝图可读，用于判断要不要显示进度条。 */
	UFUNCTION(BlueprintPure, Category = "Progress")
	EFPSProgressType GetActiveProgress() const { return ActiveProgress; }

	/** 开始一个进度动作（本地表现入口）：自动打断当前活跃进度后开新进度。
	 *  由各动作的本地信号调用（换弹的 OnRep_IsReloading→Weapon→此处）。 */
	void BeginProgress(EFPSProgressType Type, float Duration);

	/** 结束当前进度动作。仅当 Type 与当前活跃进度匹配时才结束，避免旧动作的
	 *  结束信号误关掉已被打断后新开的进度。 */
	void EndProgress(EFPSProgressType Type, bool bCompleted);

	// ---- 死亡/复活表现接口（所有端本地触发，纯表现，不做权威逻辑） ----

	/** 角色死亡瞬间触发（每端本地）— 蓝图实现死亡相机轨道/FOV 变换/死亡动画。
	 *  C++ 已自动隐藏武器与网格、关闭碰撞、禁用输入，蓝图只需做相机表现。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Health")
	void OnDeathStart();

	/** 角色复活瞬间触发（每端本地）— 蓝图复位死亡相机/FOV，恢复常态视角。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Health")
	void OnRespawn();

	// ---- 鼠标灵敏度 ----

	/** 基础鼠标灵敏度（Turn + LookUp 共用） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouse")
	float MouseSensitivity = 0.07f;

	/** 开镜时灵敏度倍率（0.5 → 瞄准时降为 50%） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouse")
	float AimSensitivityMultiplier = 0.5f;

	// ---- 蓝图可绑定事件 ----

	/** 开始瞄准 — 蓝图绑定此事件播放瞄准动画 */
	UPROPERTY(BlueprintAssignable, Category = "Aim")
	FOnActionEvent OnAimStarted;

	/** 结束瞄准 — 蓝图绑定此事件恢复常态 */
	UPROPERTY(BlueprintAssignable, Category = "Aim")
	FOnActionEvent OnAimStopped;

	// ---- 输入状态蓝图可读 ----

	/** 原始输入移动轴 — 蓝图层读取用于动画混合 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
	FVector2D MoveInputAxis;

	// ---- 蓝图事件：跑步 ----

	UPROPERTY(BlueprintAssignable, Category = "Movement")
	FOnActionEvent OnRunStarted;

	UPROPERTY(BlueprintAssignable, Category = "Movement")
	FOnActionEvent OnRunStopped;

	// ---- 蓝图事件：开火状态 ----

	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnActionEvent OnFireStarted;

	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnActionEvent OnFireStopped;

	// ---- 蓝图事件：命中特效（由武器的 MulticastHitConfirmed 在每端本地分发） ----

	/** 我（攻击者本机）命中了目标 —— 仅在本地控制的攻击者上广播。用途：hitmarker UI / 命中音 / 准星打勾 */
	UPROPERTY(BlueprintAssignable, Category = "Weapon|Hit")
	FOnHitEvent OnDealtHit;

	/** 我（被命中角色本机）被击中 —— 仅在本地控制的受害者上广播。用途：受伤镜头模糊 / 受击抖动 */
	UPROPERTY(BlueprintAssignable, Category = "Weapon|Hit")
	FOnHitEvent OnReceivedHit;

	/** 世界共享命中表现 —— 所有端无条件广播。用途：命中音效 / 子弹冲击特效 / 权威弹道 */
	UPROPERTY(BlueprintAssignable, Category = "Weapon|Hit")
	FOnHitEvent OnHitWorld;

	/** 血量变化 —— 每端本机广播。用途：血条 UI / 受伤屏幕特效 */
	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnHealthChanged OnHealthChanged;

	/** 体力变化 —— 每端本机广播（OnRep_Stamina 驱动）。用途：体力条离散反馈 / 耗尽提示 */
	UPROPERTY(BlueprintAssignable, Category = "Stamina")
	FOnStaminaChanged OnStaminaChanged;

	/** HoT buff 状态变化 —— HUD 订阅显示/更新/隐藏 buff 独立进度条。 */
	UPROPERTY(BlueprintAssignable, Category = "Buff")
	FOnHoTChanged OnHoTChanged;

	// ---- 武器系统信号 ----

	/** 拾取武器到空槽位时广播（Pick Up）。 */
	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnWeaponPickedUp OnWeaponPickedUp;

	/** 交换武器时广播（丢弃手持 + 新武器放入同槽，Swap）。 */
	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnWeaponSwapped OnWeaponSwapped;

	/** 切换活跃槽位时广播（Switch）。 */
	UPROPERTY(BlueprintAssignable, Category = "Weapon")
	FOnWeaponSlotSwitched OnWeaponSlotSwitched;

	/** 武器在每端收到 MulticastHitConfirmed 后调用攻击者角色的此函数，本地判断角色身份后分发三种事件。
	 *  纯本地表现，不做任何权威逻辑。Victim 为命中的角色（命中环境时为 nullptr）。 */
	void DispatchHitFX(const FVector& ImpactPoint, const FVector& ImpactNormal,
		float Damage, AFPSCharacter* Victim);

	// ---- 蓝图 getter ----

	UFUNCTION(BlueprintCallable, Category = "Movement")
	bool IsRunning() const { return bWantsToRun; }

	UFUNCTION(BlueprintCallable, Category = "Movement")
	bool IsAiming() const { return bWantsToAim; }

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	bool IsFiring() const;

	/** 手臂瞄准俯仰角，复制到所有端 → AnimBP AimOffset */
	UFUNCTION(BlueprintCallable, Category = "Arm")
	float GetArmPitch() const { return ArmPitch; }

protected:
	/** Called on clients when server's authoritative weapon replicates — clean up local weapon */
	UFUNCTION()
	void OnRep_PrimaryWeapon(AFPSWeapon* OldWeapon);

	UPROPERTY(ReplicatedUsing = OnRep_PrimaryWeapon, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<AFPSWeapon> PrimaryWeapon;

	/** Called on clients when secondary weapon replicates */
	UFUNCTION()
	void OnRep_SecondaryWeapon(AFPSWeapon* OldWeapon);

	UPROPERTY(ReplicatedUsing = OnRep_SecondaryWeapon, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<AFPSWeapon> SecondaryWeapon;

	/** 当前活跃武器槽位（0=主武器，1=副武器）。服务端权威，复制到所有端。 */
	UPROPERTY(ReplicatedUsing = OnRep_ActiveWeaponSlot)
	int32 ActiveWeaponSlot = 0;

	/** 切换活跃武器 */
	UFUNCTION()
	void OnRep_ActiveWeaponSlot();

	/** Weapon subclass to spawn at BeginPlay */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TSubclassOf<AFPSWeapon> WeaponClass;

	// 最大血量：仅蓝图默认值可编辑
	UPROPERTY(EditDefaultsOnly, Category = "Health")
	float MaxHealth = 100.0f;

	/** HoT 持续回血 buff：每秒回血量。蓝图可调（不同角色可能有不同体质）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float HoTHealPerSecond = 5.0f;

	/** HoT 持续回血 buff：叠加时间上限（秒）。蓝图可调。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float HoTMaxBuffDuration = 30.0f;

	/** 死亡后到复活的延迟（秒）。死亡相机/动画在这段时间播放。蓝图可调。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float RespawnDelay = 2.0f;

	// ReplicatedUsing：变量在服务器改变后，自动同步到客户端，并执行指定函数
	UPROPERTY(ReplicatedUsing = OnRep_Health, BlueprintReadOnly, Category = "Health")
	float Health;

	UPROPERTY(ReplicatedUsing = OnRep_bIsDead, BlueprintReadOnly, Category = "Health")
	uint8 bIsDead : 1;

	// ---- 道具使用状态（COND_OwnerOnly 复制给本人，驱动进度条） ----

	/** 是否正在使用道具（引导治疗/HoT 使用中）。复制给本人驱动进度条 + 锁动作。 */
	UPROPERTY(ReplicatedUsing = OnRep_bIsUsingItem)
	uint8 bIsUsingItem : 1;

	/** 当前道具使用的总时长（秒），复制给本人——客户端 OnRep 靠它起本地进度插值。 */
	UPROPERTY(Replicated)
	float ItemUseTotalDuration = 0.0f;

	/** 最近一次道具使用是否正常完成（vs 被打断/取消）。复制给本人驱动 EndProgress bCompleted。 */
	UPROPERTY(Replicated)
	uint8 bItemUseCompleted : 1;

	UFUNCTION()
	void OnRep_bIsUsingItem();

	// ---- HoT 持续回血 buff（COND_OwnerOnly 复制给本人，驱动 buff UI） ----

	UPROPERTY(ReplicatedUsing = OnRep_HoTState)
	FHoTState HoTState;

	UFUNCTION()
	void OnRep_HoTState();

	// ---- 体力配置（蓝图可编辑，每个实例可不同） ----

	/** 总体力值（最大值） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina")
	float MaxStamina = 100.0f;

	/** 奔跑时体力消耗速率（每秒消耗多少） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina")
	float StaminaDrainRate = 25.0f;

	/** Idle（静止）时体力回复速率（每秒回复多少） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina")
	float StaminaRegenRateIdle = 20.0f;

	/** Walking（移动但不奔跑）时体力回复速率（每秒回复多少） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina")
	float StaminaRegenRateWalking = 10.0f;

	/** 起跑体力线：体力低于此值会停跑并上锁，必须回复到 >= 此值才能再次奔跑。
	 *  设为 0 则只在彻底耗尽（0）时停跑、回复一点即可再跑（无"歇口气"惩罚）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stamina")
	float RunStartStaminaThreshold = 20.0f;

	/** 当前体力值（服务端权威，复制给客户端）。RepNotify → 客户端广播 OnStaminaChanged */
	UPROPERTY(ReplicatedUsing = OnRep_Stamina, BlueprintReadOnly, Category = "Stamina")
	float Stamina;

	/** 体力复制到客户端时广播 OnStaminaChanged（UI 用 getter binding 更顺滑，此事件用于离散反馈） */
	UFUNCTION()
	void OnRep_Stamina();

	/** 每帧更新体力（仅服务端）：奔跑扣、其余回；低于阈值上锁停跑、回到阈值解锁。在 Tick 中调用。 */
	void UpdateStamina(float DeltaTime);

	/** 服务端强制停跑（体力不足时）：清服务端 bWantsToRun + 重算 + 通知所有端动画 + 通知操作者本机清状态。 */
	void ServerStopRunForced();

	/** 通知操作者客户端：清本地 bWantsToRun 并播 OnRunStopped（体力锁跑由服务端发起，客户端没调过 StopRun）。 */
	UFUNCTION(Client, Reliable)
	void ClientForceStopRun();

	/** Aim pitch replicated to all clients for AnimBP spine rotation */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aim")
	float ArmPitch = 0.0f;

	// ---- 移动速度配置（蓝图可编辑，每个实例可不同） ----

	/** 走路速度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float WalkSpeed = 300.0f;

	/** 跑步速度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float RunSpeed = 600.0f;

	/** 瞄准走路速度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float AimSpeed = 200.0f;

	/** 跳跃高度 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float JumpZVelocity = 420.0f;

	// ---- 输入状态标志（服务端+客户端各存一份） ----

	uint8 bWantsToRun : 1;
	uint8 bWantsToAim : 1;
	uint8 bWantsToFire : 1;

	/** 奔跑打断开火后，松开 Shift 时若 LMB 仍按住是否恢复开火 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	bool bAutoResumeFireAfterRun = true;

	// ---- Enhanced Input（蓝图层可赋值覆盖） ----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputMappingContext* InputMappingContext = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* InputMove = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* InputLook = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* InputJump = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* InputRun = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* InputAim = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* InputFire = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* InputReload = nullptr;

	/** F 键：拾取当前范围内的 Pickup */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* InputInteract = nullptr;

	/** E 键：切换背包面板 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* InputInventory = nullptr;

	/** 1 键：切换主武器 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* InputSwitchWeapon1 = nullptr;

	/** 2 键：切换副武器 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* InputSwitchWeapon2 = nullptr;

	/** 鼠标滚轮：切换 F 键交互目标（Axis1D，滚上=+1, 滚下=-1）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* InputCycleInteraction = nullptr;

	// ---- 背包系统 ----

	/** 背包组件（构造时创建，服务端权威，复制给本人）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UFPSInventoryComponent> Inventory;

	/** 交互管理器（构造时创建，不复制——每端本地管理自己的交互目标列表）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UFPSInteractionComponent> InteractionManager;

	/** 背包面板是否打开（本地状态，不复制）。 */
	bool bInventoryOpen = false;

	/**
	 * 输入锁（本地状态，不复制）：true 时禁掉移动/视角/开火/瞄准/跑步/换弹，
	 * 但**不锁 E 键（ToggleInventory）** —— 这样打开背包仍能按 E 退出。
	 * 由 ToggleInventory 在开关背包时翻转。各输入函数入口检查它。
	 */
	bool bInputLocked = false;

	/**
	 * 鼠标是否已锁定到计分板面板（查 Controller 状态）。true 时左/右键点击进 UI（点排序按钮），
	 * 开火/瞄准应被屏蔽。仅本地控制端有意义（非本地端无 Controller UI 状态）。
	 */
	bool IsScoreboardMouseLocked() const;

	/** F 键处理：对当前可拾取目标发起服务端拾取请求。 */
	void Interact();

	/** E 键处理：翻转 bInventoryOpen 并触发蓝图 OnToggleInventory（纯本地 UI）。 */
	void ToggleInventory();

	/**
	 * 客户端请求拾取（不传 Pickup 指针 —— Actor 引用跨 Server RPC 在客户端常解析失败）。
	 * 服务端收到后自己用球形重叠查角色周围的 AFPSPickup，选最近的一个拾取（顺便防作弊）。
	 */
	UFUNCTION(Server, Reliable)
	void ServerTryPickup();

	/** 客户端请求使用背包道具 → 服务端权威执行。 */
	UFUNCTION(Server, Reliable)
	void ServerUseInventoryItem(int32 Index);

	/** 客户端请求丢弃背包道具 → 服务端权威执行（移除 + 脚下生成 Pickup）。 */
	UFUNCTION(Server, Reliable)
	void ServerDropInventoryItem(int32 Index);

	/** 客户端请求拖动重排背包道具 → 服务端权威执行。 */
	UFUNCTION(Server, Reliable)
	void ServerMoveInventoryItem(int32 FromIndex, int32 ToGridX, int32 ToGridY);

	/** Server-side death handling */
	void Die(AController* Killer);

	/** 死亡/复活时切换尸体表现（隐藏武器+网格、关碰撞）。各端本地执行，纯表现。 */
	void SetDeathPresentation(bool bDead);

	/** Enhanced Input handler：移动 (Axis2D, X=Forward, Y=Right) */
	void Move(const FInputActionValue& Value);
	void MoveCompleted(const FInputActionValue& Value);

	/** Enhanced Input handler：视角 (Axis2D, X=Yaw, Y=Pitch) */
	void Look(const FInputActionValue& Value);

	void StartRun();
	void StopRun();
	void StartAim();
	void StopAim();

	void StartFire();
	void StopFire();

	/** R 键换弹：本地预判 + 转发服务端权威换弹 */
	void Reload();

	/** 根据当前输入和速度重新计算 MovementState（所有端都执行） */
	void UpdateMovementState();

	/** 将 WalkSpeed/RunSpeed/AimSpeed 应用到 CharacterMovement（仅服务端） */
	void ApplyMovementSpeed();

	UFUNCTION(Server, Reliable)
	void ServerSetWantsToRun(bool bNewWantsToRun);

	UFUNCTION(Server, Reliable)
	void ServerSetWantsToAim(bool bNewWantsToAim);

	/** 通知所有客户端瞄准状态变化 — 远程客户端靠此触发 OnAimStarted/Stopped */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastOnAimStateChanged(bool bNewWantsToAim);

	/** 通知所有客户端跑步状态变化 */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastOnRunStateChanged(bool bNewWantsToRun);

	/** 通知所有客户端开火状态变化 */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastOnFireStateChanged(bool bNewFiring);

	UFUNCTION(Server, Reliable)
	void ServerStartFire();

	UFUNCTION(Server, Reliable)
	void ServerStopFire();

	/** 客户端请求换弹 → 服务端权威校验 + 执行 */
	UFUNCTION(Server, Reliable)
	void ServerReload();

	/** 客户端请求取消当前道具使用（按 F 或其它取消操作）→ 服务端执行取消。 */
	UFUNCTION(Server, Reliable)
	void ServerCancelUseItem();

	/** 客户端每帧将本地瞄准 Pitch 发送到服务端，服务端赋值后复制给所有客户端 */
	UFUNCTION(Server, Reliable)
	void ServerUpdateAimPitch(float Pitch);

	// ---- 武器切换 & 拾取 ----

	/** 切换主武器（按 1 键） */
	void SwitchToPrimaryWeapon();

	/** 切换副武器（按 2 键） */
	void SwitchToSecondaryWeapon();

	/** 服务端切换武器槽位 */
	UFUNCTION(Server, Reliable)
	void ServerSwitchWeapon(int32 NewSlot);

	/** 客户端请求拾取武器 Pickup → 服务端权威执行（交换/装备/丢弃）。 */
	UFUNCTION(Server, Reliable)
	void ServerTryPickupWeapon();

	/** 客户端请求一键提交所有贵重品 → 服务端找最近开放提交点执行。 */
	UFUNCTION(Server, Reliable)
	void ServerSubmitAllValuables();

	/** 客户端请求单独提交第 Index 格 → 服务端找最近开放提交点执行。 */
	UFUNCTION(Server, Reliable)
	void ServerSubmitSingleItem(int32 Index);

	/** 滚轮输入处理：正值=下一项，负值=上一项。 */
	void CycleInteractionInput(const FInputActionValue& Value);

	/** 服务端：将指定武器作为 Pickup 掉落在角色脚下（死亡/交换时调用）。 */
	void DropWeaponAsPickup(AFPSWeapon* Weapon);

	/** 根据 ActiveWeaponSlot 刷新两把武器的 Show/Hide。服务端直接调，客户端 OnRep 调。 */
	void UpdateWeaponVisibility();

	// ---- 道具使用状态机（仅服务端，不复制） ----

	/** 取消当前道具使用（F 键 / 死亡时调用）。已扣耐久不退还。 */
	void CancelItemUse();

	/** 启动异步道具使用流程（Character 状态机入口），由 ServerUseInventoryItem_Implementation 调用。 */
	void StartItemUseProcess(const struct FItemUseResult& Result);

	/** ChanneledHeal：前摇结束 → 开始逐跳回血 */
	void OnChanneledHealWindUpComplete();

	/** ChanneledHeal：单跳回血 */
	void OnChanneledHealTick();

	/** HoTApply：使用时间结束 → 激活 HoT buff */
	void OnHoTApplicationComplete();

	/** 道具使用正常完成（所有跳血交付完毕 / HoT buff 已激活） */
	void CompleteItemUse();

	/** 清空所有道具使用相关的 timer 和临时状态 */
	void ClearItemUseState();

	// ---- HoT Buff 系统（仅服务端，最终状态复制到 HoTState） ----

	/** 激活/叠加 HoT 持续回血 buff。每秒回血速度和上限取自 Character 自身配置。 */
	void ActivateHoT(float AddedDuration);

	/** HoT 每 tick 回血 */
	void OnHoTTick();

	/** 移除 HoT buff（到期/死亡） */
	void DeactivateHoT();

	// ---- 道具使用内部状态（仅服务端） ----

	EFPSItemUseType ActiveItemUseType = EFPSItemUseType::None;
	int32 RemainingHealAmount = 0;
	int32 ActiveHealPerTick = 0;
	float ActiveHealInterval = 0.0f;

	FTimerHandle ItemUsePhaseTimerHandle;
	FTimerHandle HealTickTimerHandle;

	// ---- HoT buff 内部状态（仅服务端） ----

	float HoTRemainingTime = 0.0f;
	float HoTHealPerTick = 0.0f;
	FTimerHandle HoTTimerHandle;
	static constexpr float HoTTickInterval = 1.0f;

	// HoT 待激活参数（UseTime 期间暂存）
	float PendingHoTBaseDuration = 0.0f;
};
