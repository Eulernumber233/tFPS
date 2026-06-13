# tFPS — 多人混战 FPS 项目

## 项目概述
UE 5.7 多人 FPS 游戏 Demo，Listen Server 架构，局域网联机。

## 游戏规则
- **模式**：Deathmatch 混战（FFA，无队伍）
- **人数**：上限 10 人
- **时长**：5 分钟一局
- **死亡**：血量归零后 3 秒在随机出生点复活
- **胜利条件**：时间结束时，击杀数最高的玩家获胜

## 架构方案
当前使用 **方案一（Listen Server）**：
- Host 启动游戏（同时也是玩家）
- 其他客户端输入 IP:Port 连接加入
- 未来可无缝迁移到方案二（Dedicated Server）

## 类结构

```
AFPSGameMode          [仅服务端]   比赛流程控制、计时、生成玩家
AFPSGameState         [复制到所有]  比赛时间、比赛阶段同步
AFPSPlayerState       [复制到所有]  每个玩家的击杀数/死亡数
AFPSPlayerController  [服务器+所属] 重生 RPC（输入已下放到 Character）
AFPSCharacter         [复制]       血量/体力、移动、死亡/复活、主副武器、移动状态、背包组件
AFPSWeapon            [复制 Actor]  武器射击、服务端权威碰撞检测、弹药（仅弹夹）。网格体由蓝图子类创建
AFPSWeaponPickup      [复制 Actor]  地上的武器拾取物（不进背包），拾取时与玩家手持武器交换
UFPSInventoryComponent[挂 Character] 背包：服务端权威增删/使用，COND_OwnerOnly 复制给本人
UFPSItemDef           [DataAsset]   道具数据定义（多态子类包括：Heal/ChanneledHeal/HoT/Valuable/Ammo）
UFPSAmmoItemDef       [DataAsset]   子弹数据定义：口径/等级/基础伤害，背包中按 (Caliber,Tier) 分开堆叠
AFPSPickup            [复制 Actor]  地上拾取物，Root + StaticMesh(自动从DA读) + Sphere 触发器。通用 BP_Pickup_Generic 覆盖大多数道具
UFPSInteractionComponent[挂 Character] 交互管理器：管理所有 F 键交互目标的列表，滚轮切换选中项
AFPSSubmissionPoint   [复制 Actor]  物品提交点：随机开放/关闭，开放时玩家提交贵重品累加 CarryValue
```

## 蓝图可读状态（所有端）

| 属性 | 类型 | 来源 |
|---|---|---|
| `MovementState` | `EFPSMovementState` | `Tick` 中根据速度+输入计算 |
| `WeaponFireState` | `EFPSWeaponFireState` | `Tick` 中根据武器状态设置 |
| `MoveInputAxis` | `FVector2D` | `Move` 中每帧写入 X/Y，`Completed` 时清零 |
| `bIsDead`（IsDead()） | `bool` | 服务端权威复制 |
| `Health` | `float` | 服务端权威复制 |
| `ArmPitch` | `float` | 本地控制端写入，复制到所有端，给 AnimBP 做 AimOffset |

| Getter 函数 | 返回 | 备注 |
|---|---|---|
| `IsRunning()` | `bWantsToRun` | 仅在按 Shift 的客户端为 true |
| `IsAiming()` | `bWantsToAim` | 仅在按 RMB 的客户端为 true |
| `IsFiring()` | `GetActiveWeapon()->IsFiring()` | 服务端复制到所有端 |
| `GetWeapon()` | `AFPSWeapon*` | 主武器（槽 0），兼容旧蓝图 |
| `GetActiveWeapon()` | `AFPSWeapon*` | 当前活跃槽位的武器（开火/换弹操作的目标） |
| `GetSecondaryWeapon()` | `AFPSWeapon*` | 副武器（槽 1，可能为 nullptr） |
| `GetActiveWeaponSlot()` | `int32` | 0=主武器活跃，1=副武器活跃 |
| `GetWeaponPickupTarget()` | `AFPSWeaponPickup*` | 当前可拾取武器（非空显示"按F换枪"提示） |
| `GetArmPitch()` | `float` | 复制到所有端的瞄准俯仰角 |

## 输入系统（纯 Enhanced Input，Axis2D 架构）

C++ 不再动态创建 InputAction / IMC / Modifier。所有资产由蓝图填到 `BP_FPSPlayer` 的 Input 分类下的 UPROPERTY：`InputMappingContext / InputMove / InputLook / InputJump / InputRun / InputAim / InputFire / InputReload / InputInteract / InputInventory / InputSwitchWeapon1 / InputSwitchWeapon2 / InputCycleInteraction`。

| 输入 | IA | 值类型 | C++ 处理函数 | 说明 |
|---|---|---|---|---|
| WASD | InputMove | Axis2D | `Move(FVector2D)` | X=前后 (W=+1,S=-1)，Y=左右 (D=+1,A=-1) |
| Mouse | InputLook | Axis2D | `Look(FVector2D)` | X=Yaw，Y=Pitch。开镜自动降灵敏度 |
| Shift | InputRun | Boolean | `StartRun/StopRun` | Started→StartRun, Completed→StopRun |
| RMB | InputAim | Boolean | `StartAim/StopAim` | Started→StartAim, Completed→StopAim |
| Space | InputJump | Boolean | `ACharacter::Jump/StopJumping` | |
| LMB | InputFire | Boolean | `StartFire/StopFire` | |
| R | InputReload | Boolean | `Reload` | Started→Reload（本地预判 CanReload 后转发 ServerReload） |
| F | InputInteract | Boolean | `Interact` | Started→根据交互管理器选中目标路由：提交/捡枪/拾取（转发对应 RPC） |
| Mouse Wheel | InputCycleInteraction | Axis1D | `CycleInteractionInput` | Triggered→正值=下一项, 负值=上一项（切换 F 键交互目标） |
| Tab | InputInventory | Boolean | `ToggleInventory` | Started→翻转背包开关，触发蓝图 OnToggleInventory（纯本地 UI） |
| 1 | InputSwitchWeapon1 | Boolean | `SwitchToPrimaryWeapon` | Started→切主武器（若已有且非活跃） |
| 2 | InputSwitchWeapon2 | Boolean | `SwitchToSecondaryWeapon` | Started→切副武器（若已有且非活跃） |

### 蓝图侧 IA 资产配置要点

- **每个 IA 必须挂 Trigger**（如 `Down` 或 `Pressed`），否则 Enhanced Input 静默丢事件
- **IA_Move (Axis2D)**：W=(+1,0), S=(-1,0)（Negate 修饰符）, D=(0,+1), A=(0,-1)（Negate 修饰符）。或用 Swizzle Input Axis Values 修饰符把 X/Y 重排
- **IA_Look (Axis2D)**：MouseX→X 直连，MouseY→Y 加 Negate 修饰符（让上推鼠标 = 抬头）
- **MouseX/Y 的 IA 需把 Trigger 的 Actuation Threshold 调到 ~0.0001**，否则微小鼠标移动被忽略

### 关键设计决策

- **IMC 仅在 BeginPlay 的 SetTimerForNextTick 注册一次**：BeginPlay 时 Controller 可能尚未 Possess，下一帧 LocalPlayer/Subsystem 才就绪
- **不调用 `RequestRebuildControlMappings`**：会丢弃动态创建的 UInputModifier（Listen Server 专属 bug 历史教训）
- **资产由蓝图提供而非 C++ NewObject**：动态创建的 Modifier 在 Listen Server 上跨帧可能被 GC，蓝图序列化的 uasset 不会
- **无 Raw Input fallback**：Enhanced Input 在所有端（包括 Listen Server host）上工作
- **`PlayerController` 已精简**：只保留 `ClientRespawn` RPC，无输入处理代码

## 移动状态系统（C++ + 蓝图分工）

| 层级 | 负责内容 | 网络同步？ |
|---|---|---|
| C++ | WASD/Shift/RMB/LMB/Mouse输入绑定，移动执行，速度切换，跳跃，开火，视角旋转 | ✅ 服务端权威 |
| C++ | `MovementState` 枚举每帧更新 | 本地（给蓝图读） |
| 蓝图 | 读 MovementState 做动画、武器姿态、相机效果 | ❌ 纯本地表现 |

### MovementState 枚举
- `Idle` — 静止
- `Walking` — 移动中
- `Running` — Shift 按住 + 移动
- `Aiming` — RMB 按住（优先于跑步）

### 速度切换逻辑（C++ 服务端权威）
| 状态 | 速度 | 触发 |
|---|---|---|
| Walking | WalkSpeed (300) | 默认 |
| Running | RunSpeed (600) | 按住 Shift |
| Aiming | AimSpeed (200) | 按住 RMB 右键 |

速度通过 `CharacterMovementComponent.MaxWalkSpeed` 复制到所有客户端。

## 交互管理系统（UFPSInteractionComponent）

组件挂在 AFPSCharacter 上，管理所有 F 键交互目标的列表。当多个交互源同时存在时，玩家用**鼠标滚轮**切换当前选中项，F 键只对选中项生效。

### 交互类型与优先级

| 类型 | 优先级 | 提示文字 | 源 Actor |
|---|---|---|---|
| `WeaponPickup` | 100（最高） | "Swap weapon" | AFPSWeapon（IsOnGround） |
| `SubmissionPoint` | 50 | "Submit valuables" | AFPSSubmissionPoint（IsOpen） |
| `Pickup` | 0 | "Pick up {ItemName}" | AFPSPickup |

优先级越高越靠前显示。同优先级按注册顺序排列。

### 生命周期

- **注册**：交互源进入角色范围（overlap 触发）→ 对应 SetXxxTarget 函数调用 `InteractionManager->RegisterInteraction(Source, Type, Prompt, Priority)`
- **注销**：角色离开范围 / 源被销毁 → ClearXxxTarget 调用 `UnregisterInteraction(Source)`
- **选中**：第一个条目注册时自动选中；删除条目时自动调整选中索引
- **不复制**：每个端本地维护自己的交互列表（overlap 在各端独立触发）

### 蓝图接口

| 函数 | 用途 |
|---|---|
| `GetInteractionManager()` | 拿组件，遍历 `GetEntries()` / 读 `GetActivePrompt()` |
| `CycleInteractionNext()` | 选中下一个（滚轮下滚），BlueprintCallable |
| `CycleInteractionPrev()` | 选中上一个（滚轮上滚），BlueprintCallable |
| `OnInteractionsChanged`（委托） | 条目列表变化 → UI 重建提示框 |

### C++ 输入绑定

- `InputCycleInteraction`（Axis1D）：滚轮 → `CycleInteractionInput` → 正值调 Next、负值调 Prev
- 蓝图层在 `BP_FPSPlayer` Class Defaults 的 Input 分类填 IA（Mouse Wheel Axis）

### UI 实现指南

`WBP_InteractionPrompt`（提示框列表）：
- 容器：`VerticalBox`，动态生成每个提示行
- 每行：图标（按类型）+ 文字（`PromptText`）+ 高亮边框（`SelectedIndex == 当前行` 时显示）
- 订阅 `OnInteractionsChanged` → 遍历 `GetEntries()` → `Create Widget(WBP_InteractionPromptRow)` → 填数据
- 订阅角色死亡事件 → IsDead 时隐藏整个提示框

## 物品提交系统（AFPSSubmissionPoint）

场景手摆的提交点 Actor，随机进入"开放"状态。开放期间玩家走到范围内：
- **按 F 键**一键提交背包中所有 `UFPSValuableItemDef` 道具
- **打开背包右键物品**菜单出现"提交"选项（需在开放提交点范围内）

提交的物品按其 `GetCurrentValue()` 累加到 `AFPSPlayerState::CarryValue`（战绩面板显示）。

### 状态机（仅服务端）

```
Closed ──(Random[MinClosedTime, MaxClosedTime])──→ Open
Open   ──(Random[MinOpenTime, MaxOpenTime])──────→ Closed
```

- `BeginPlay` 随机排第一个状态变更
- 每次状态切换后立即排下一次（用 `FTimerHandle`）
- `SubmissionState` 通过 OnRep 复制到所有端 → 触发蓝图 `OnSubmissionStateChanged(bIsOpen)`

### 关键字段

| 字段 | 默认值 | 说明 |
|---|---|---|
| `InteractionRadius` | 200cm | 玩家检测范围 |
| `MinClosedTime` / `MaxClosedTime` | 30s / 120s | 关闭时长随机区间 |
| `MinOpenTime` / `MaxOpenTime` | 15s / 45s | 开放时长随机区间 |

### 提交流程

```
══ 一键提交（按 F 键） ══
本地：Interact() → InteractionManager 选中 SubmissionPoint
  → ServerSubmitAllValuables RPC
服务端：FindBestSubmissionPointInRange → Point->SubmitAllValuables
  → 遍历背包：找所有 UFPSValuableItemDef 条目
  → 累加 GetCurrentValue 到 CarryValue
  → Inv->ServerRemoveItemAt 逐格删除
  → OnInventoryChanged → UI 刷新

══ 单件提交（右键菜单） ══
WBP_SlotContextMenu：CanSubmitItems() 为 true 时多显示"提交"按钮
  → 点提交 → Character::SubmitInventoryItem(Index)
  → ServerSubmitSingleItem RPC
服务端：FindBestSubmissionPointInRange → Point->SubmitSingleItem
  → 校验 IsA<UFPSValuableItemDef> → GetCurrentValue → 删格 → AddCarryValue
```

### 蓝图接口

| 函数 | 用途 |
|---|---|
| `GetSubmissionTarget()` | 当前范围内开放提交点（null=不在范围或已关闭） |
| `CanSubmitItems()` | 是否可提交（范围内 + 开放）——右键菜单据此显示/隐藏"提交" |
| `SubmitInventoryItem(Index)` | 单独提交第 Index 格（BlueprintCallable，右键菜单调） |
| `OnSubmissionTargetChanged`（委托） | 提交目标变化 → UI 显示/隐藏提交提示 |
| `OnSubmissionStateChanged(bIsOpen)`（BlueprintImplementableEvent） | 状态变化 → 蓝图切换灯光/粒子/音效 |

### 蓝图接入步骤

1. 创建继承 `AFPSSubmissionPoint` 的蓝图子类（如 `BP_SubmissionPoint`）
2. 在 `DefaultRoot` 下加 StaticMesh（提交点模型，**不要替换 root**）
3. 实现 `OnSubmissionStateChanged`：bIsOpen 为 true 时播放激活特效、灯光；false 时关闭
4. 实现 `OnSubmissionStateChanged` 关联的粒子/音效
5. 拖几个 `BP_SubmissionPoint` 到 TestMap → 启动测试 → 等待随机开放 → 走近按 F 提交

### 背包右键菜单适配

在 `WBP_SlotContextMenu` 中（参考 CLAUDE.md "背包面板设计" 章节）：

```
Event Construct（或 Pre Construct）：
  → Get Owning Player Pawn → Cast BP_FPSPlayer → 存 OwnerCharacter 引用

构建菜单按钮前：
  → Branch(OwnerCharacter.CanSubmitItems())  ← ★新增判断
    True → 在 Use/Drop 之外多显示一个"Submit"按钮
    False → 只显示 Use/Drop（原逻辑）

Submit 按钮 OnClicked：
  → OwnerCharacter.SubmitInventoryItem(TargetSlotIndex)
  → Remove from Parent
```

## 武器生成与挂载

- **仅服务端生成**：`BeginPlay` 中 `HasAuthority()` 时才 `SpawnActorDeferred`，客户端不生成本地武器
- **服务端权威**：服务端的武器是唯一版本（`bReplicates = true`），负责开火/伤害
- **客户端接收**：服务端 `CurrentWeapon` 复制到客户端，`OnRep_CurrentWeapon` 中调用 `OnWeaponEquipped()`（每人仅一次）
- **网络相关性**：`IsNetRelevantFor` 重写为跟随拥有者角色 → 所有玩家都能看到彼此的武器
- **第一人称可见性**：`SetupPlayerInputComponent` 中 `GetMesh()->SetOwnerNoSee(true)`。武器自身的 OwnerNoSee 由蓝图层在 `OnWeaponEquipped` 中设置
- **`OnWeaponEquipped` 触发**：
  - 服务端：`BeginPlay` 中生成武器后用 `SetTimerForNextTick` 延迟一帧触发（确保武器蓝图子类的组件构造完成）
  - 客户端：`OnRep_CurrentWeapon` 中触发（武器初始复制完成后）
  - `bWeaponEquipped` 守卫保证每个角色仅触发一次
- **`AFPSWeapon` 默认 RootComponent**：C++ 构造函数创建一个空 `USceneComponent` 作为 RootComponent，保证蓝图子类无论是否在 ConstructionScript 重设 root，`AttachToActor/Component` 都能成功

## 主副武器系统

角色最多同时持有两把武器：`CurrentWeapon`（主，槽 0）+ `SecondaryWeapon`（副，槽 1）。两者都 Replicated，所有端可见。

- **活跃槽位**：`ActiveWeaponSlot`（0 或 1，Replicated）。`GetActiveWeapon()` 返回当前持有和使用的武器。
- **切换**：按 1 键切主武器，按 2 键切副武器（`InputSwitchWeapon1/2`）。服务端权威执行 `ServerSwitchWeapon`，切换时自动停火/停瞄，隐藏旧武器显示新武器。
- **开火/换弹**：永远操作活跃武器（`GetActiveWeapon()`），而非固定 `CurrentWeapon`。
- **蓝图挂载**：`OnWeaponEquipped()` 挂载主武器到 Arm；`OnSecondaryWeaponEquipped()` 挂载副武器（默认隐藏）。切换时 C++ 通过 `SetActorHiddenInGame` 控制可见性，蓝图无需额外处理。
- **装备接口**：`EquipWeapon(Weapon, Slot)` — 服务端将武器安装到指定槽。不改变 `ActiveWeaponSlot`（调用方决定是否切槽）。

## 武器拾取与掉落

### AFPSWeaponPickup（地上的武器 Actor）

独立于 AFPSPickup（道具拾取），武器不进背包。`DefaultRoot + PickupSphere` 结构同 AFPSPickup。

| 属性 | 类型 | 说明 |
|---|---|---|
| `WeaponClass` | TSubclassOf\<AFPSWeapon\> | 拾取时生成的武器蓝图类 |
| `DroppedCurrentAmmo` | int32 | 掉落时弹夹剩余弹数 |
| `DroppedAmmoDef` | UFPSAmmoItemDef* | 掉落时弹夹内弹药类型 |
| `Value` | int32 | 武器价值（死亡掉落排序用） |

### 拾取流程（F 键，武器优先于道具）

```
走近武器 Pickup → 角色端记录 CurrentWeaponPickupTarget → 蓝图提示"按 F 换枪"
按 F → Interact()：优先检查 CurrentWeaponPickupTarget
  → ServerTryPickupWeapon RPC → FindBestWeaponPickupInRange
    → AFPSWeaponPickup::ServerTryPickup(Picker)：
      ├─ 0 枪 → EquipWeapon(New, 0)      ← 装备为主武器
      ├─ 1 枪 → EquipWeapon(New, 1)      ← 装备为副武器（不切槽）
      └─ 2 枪 → 交换当前活跃槽位的武器
          ├─ 旧武器 → DropWeaponAsPickup → SpawnActor(Weapon→WeaponPickupClass)
          └─ EquipWeapon(New, ActiveSlot)
      → 应用 DroppedCurrentAmmo/DroppedAmmoDef 到新武器
      → Destroy 地上的 WeaponPickup
```

### 死亡掉落

`Die()` 中比较 `CurrentWeapon→GetWeaponValue()` 与 `SecondaryWeapon→GetWeaponValue()`，掉落价值更高的。若仅有 1 把则掉落那一把。

- 掉落：`DropWeaponAsPickup(Weapon)` → Spawn 该武器的 `WeaponPickupClass`
- 掉了活跃武器且另一把存活 → 自动 `ActiveWeaponSlot` 切到存活武器
- 唯一武器掉落 → `Respawn` 时从 `WeaponClass` 重新生成默认武器

### AFPSWeapon 新增字段

| 字段 | 类型 | 说明 |
|---|---|---|
| `WeaponValue` | int32 | 武器价值（蓝图子类填，死亡掉落比较用） |
| `WeaponPickupClass` | TSubclassOf\<AFPSWeaponPickup\> | 掉落时生成的 Pickup 蓝图类（必须填） |
| `AcceptedCaliber` | FName | 本武器接受的口径名（换弹时匹配背包子弹） |

## 武器状态系统（C++ + 蓝图分工）

**职责清晰分离**：C++ 只管权威逻辑，所有特效（粒子/音效/动画/连发计时）由蓝图听角色事件自行驱动。

| 层级 | 负责内容 | 网络同步？ |
|---|---|---|
| C++ 角色 | LMB 输入绑定，`OnFireStarted/OnFireStopped` 广播 | ⚡ 本地零延迟 + Multicast 同步远端 |
| C++ 武器 | 服务端连发计时、LineTrace、伤害判定 | ✅ 服务端权威 |
| C++ 武器 | 服务端 debug line multicast（仅按 `bDrawAuthoritativeTrace` 开关绘制） | ✅ Multicast Unreliable |
| 蓝图角色 | 听 `OnFireStarted/Stopped` → 启动本地 timer → 连发期间调武器蓝图函数播特效 | ❌ 纯本地表现 |
| 蓝图武器 | 实现自己的特效播放函数（粒子/音效/抛壳/Niagara Beam） | ❌ 纯本地表现 |

### 开火事件流（极简版）

```
本地玩家按 LMB
├─ AFPSCharacter::StartFire
│   ├─ OnFireStarted.Broadcast()             ← 本地零延迟广播
│   │   └─ 蓝图: BP_FPSPlayer 监听 → 启动本地连发 timer → 每发调武器 BP 播粒子
│   ├─ MulticastOnFireStateChanged(true)     ← 远端玩家收到后也广播 OnFireStarted
│   │   └─ 远端蓝图同样的逻辑（延迟 ~1 RTT）
│   └─ ServerStartFire RPC
│       └─ 服务端 CurrentWeapon->StartFire()
│           ├─ FireTimer 每 FireRate 调 HandleFire（LineTrace + 伤害）
│           └─ MulticastDrawAuthTrace（每端按本机开关画 debug line，无蓝图特效）
└─ 释放 LMB 走 OnFireStopped 对称流程
```

### 关键 C++ 接口

| 类 | 函数 | 用途 |
|---|---|---|
| AFPSCharacter | `OnFireStarted/Stopped` (BlueprintAssignable) | **蓝图特效层唯一入口**：本地零延迟 + Multicast 同步远端 |
| AFPSWeapon | `StartFire/StopFire` (server only) | 启停服务端权威开火 timer |
| AFPSWeapon | `HandleFire` (server only) | 单发：LineTrace + 伤害 + MulticastDrawAuthTrace |
| AFPSWeapon | `GetFireRange()/GetFireRate()` (BlueprintCallable) | 蓝图层查询常数（用于 timer 间隔、Niagara Beam 终点距离） |
| AFPSWeapon | `GetOwningCharacter()` (BlueprintCallable) | 蓝图层拿持有者（替代不可靠的 `GetOwner()`） |
| AFPSWeapon | `bDrawAuthoritativeTrace` (EditAnywhere, BlueprintReadWrite) | 每端独立开关，对比本地 Niagara Beam vs 权威射线 |

### 蓝图侧推荐实现

**`BP_FPSPlayer` EventGraph**：
```
On Fire Started → Set Timer by Event(WeaponFireTimer, Time=Weapon.GetFireRate, Looping=true)
                  └ 触发 Custom Event: FireOnce → 调用 Weapon 蓝图函数 BP_PlayFireFX

On Fire Stopped → Clear Timer(WeaponFireTimer)
```

**`BP_WeaponBase` 暴露蓝图函数 `BP_PlayFireFX`**（普通蓝图函数，不是 RPC）：
```
BP_PlayFireFX → Spawn Niagara Beam at Muzzle Socket
              → Play Sound at Location
              → Spawn 抛壳
```

### 一致性说明

- 本地 timer（蓝图）和服务端 timer（C++）节奏独立 —— 本地按 `GetFireRate` 自己 tick，零延迟
- 如果服务端因延迟某发没打出（如玩家中途死亡），本地可能多播 1-2 发特效，**对玩家感知无影响**
- LineTrace + 伤害**永远**在服务端权威，本地特效不影响命中结果
- 服务端 `MulticastDrawAuthTrace` 只画 debug line，**不触发任何蓝图事件** —— 避免特效双轨重复

### WeaponFireState 枚举
- `Idle` — 未开火
- `Firing` — 按住 LMB 连发中

### 蓝图接入步骤
1. 创建继承自 `AFPSWeapon` 的蓝图子类（如 `BP_WeaponBase` → `BP_VIRTUS`），添加 Skeletal Mesh 组件并挂在 DefaultRoot 下（**不要替换 root，否则 attach 行为不可控**）
2. 在武器蓝图 EventGraph 中覆盖 `OnFireBP`（枪口火焰/音效）和 `OnStopFireBP`（特效复位）
3. 创建继承自 `AFPSCharacter` 的角色蓝图（如 `BP_FPSPlayer`），在 Class Defaults → Input 分类下指派 `InputMappingContext / InputMove / InputLook / InputJump / InputRun / InputAim / InputFire`（详见"输入系统"章节）
4. 角色蓝图中 **删除** WASD/Look/Jump/Run/Aim/Fire 等所有输入事件节点（C++ 已绑定）
5. **保留** 所有动画、状态机、武器姿态、相机逻辑
6. **`OnWeaponEquipped` 事件实现**：
   - `GetWeapon()` → `IsValid` → `Attach Actor To Component`
   - **Target**：武器 actor（GetWeapon 输出）
   - **Parent**：角色身上的手臂 SkeletalMeshComponent（如 `Arm`）
   - **Socket Name**：留 `None`。Arm 和武器使用同一套骨架/动画对位，武器 attach 到 Arm 根节点时本地变换为零即与手部正确贴合。**不要填 socket**，否则会被骨骼额外变换偏移
   - **Location/Rotation/Scale Rule**：`Snap to Target`
7. 从 `MovementState` 引脚读取枚举，驱动移动动画；从 `WeaponFireState` 引脚读取枚举，驱动射击动画
8. 将角色蓝图的 `WeaponClass` 设置为步骤 1 创建的 Weapon 蓝图

## 蓝图可绑定事件（BlueprintAssignable）

| 事件 | 触发时机 | 网络同步 |
|---|---|---|
| `OnAimStarted` | 按下 RMB | 本地即时 + NetMulticast 通知所有远程客户端 |
| `OnAimStopped` | 松开 RMB | 同上 |
| `OnRunStarted` | 按住 Shift | 同上 |
| `OnRunStopped` | 松开 Shift | 同上 |
| `OnFireStarted` | 按下 LMB | 同上（开火状态变化，非每发） |
| `OnFireStopped` | 松开 LMB | 同上 |
| `OnDealtHit` | 服务端权威命中确认 | 仅攻击者本机广播（hitmarker UI/命中音/准星打勾） |
| `OnReceivedHit` | 服务端权威命中确认 | 仅被命中角色本机广播（受伤镜头模糊/受击抖动） |
| `OnHitWorld` | 服务端权威命中确认 | 所有端无条件广播（命中音效/冲击特效/权威弹道） |

蓝图中直接从「我的蓝图」面板拖出事件节点绑定动画即可。发送方（操作者本人）收到的是零延迟本地广播，远程客户端通过 NetMulticast RPC 同步触发，延迟约 1 RTT。

### 命中通知系统（OnDealtHit / OnReceivedHit / OnHitWorld）

**设计原则**：客户端可保留自己的本地预测射线（弹孔/枪口火焰等廉价可丢弃特效），服务端只对权威命中发**一条** Multicast，由各端本机分发成三种蓝图通知。任何影响游戏结果或对方表现的东西（伤害/血雾/击杀）只信服务器，本地预测不参与命中结果。

**数据流**：
```
服务端 AFPSWeapon::HandleFire 命中
└─ MulticastHitConfirmed(ImpactPoint, ImpactNormal, Damage, Victim)   ← 所有端收到同一份数据
      └─ 每端 AFPSWeapon 调 OwningCharacter->DispatchHitFX(...)
            ├─ if 本机控制攻击者              → OnDealtHit.Broadcast
            ├─ if Victim 且本机控制 Victim    → Victim->OnReceivedHit.Broadcast
            └─ 无条件                          → OnHitWorld.Broadcast
```

**载荷**（`FOnHitEvent` 四参数委托）：`ImpactPoint` / `ImpactNormal`（占位，客户端已自行预测可忽略） / `Damage`（服务端权威，未来可加距离衰减） / `Victim`（命中角色，命中环境时为 nullptr —— 蓝图据此区分血雾 vs 弹孔）。

**网络注意点**：`MulticastHitConfirmed` 发在武器上，武器 `IsNetRelevantFor` 跟随攻击者角色。因此通知只到达"攻击者相关"的客户端。实践上够用（看到子弹冲击的前提通常是看得到开枪者）；极端背身偷袭可能丢失受害者的 `OnReceivedHit` 特效，但 `Health` 复制的权威伤害永不丢。如未来需保证背身受击反馈，可把受击通知改为在受害者角色上发（受害者对自己永远相关）。

### EventGraph 使用示例
```
OnAimStarted → Set FOV 60 → Play AimMontage → Set AnimBP bIsAiming = true
OnAimStopped → Set FOV 90 → Stop AimMontage → Set AnimBP bIsAiming = false
OnFireStarted → Play FireHold Montage → Set AnimBP bIsFiring = true
OnFireStopped → Stop FireHold Montage → Set AnimBP bIsFiring = false
```

## 射击碰撞通道（Weapon Trace Channel）

子弹用**专属自定义 Trace 通道 `Weapon`**（`DefaultEngine.ini` 里 `ECC_GameTraceChannel1`，C++ 宏 `COLLISION_WEAPON`），不用通用 `ECC_Visibility` —— 避免与 AI 视线、鼠标拾取等用途互相干扰。通道默认响应 `Ignore`，只有显式 `Block` 的物体才挡子弹。

**命中体设计（C++ 构造函数 `AFPSCharacter()` 中配置）**：

| 组件 | 对 Weapon 通道 | 说明 |
|---|---|---|
| 骨骼网格体 `GetMesh()` | `Block`（QueryOnly） | 子弹打真实身体，贴合模型，可做部位判定。**必须配 Physics Asset**，否则射线打不中 |
| 胶囊体 `GetCapsuleComponent()` | `Ignore` | 圆柱判定粗糙且会与网格双重命中。胶囊继续负责移动/落地碰撞 |
| 武器/世界物体 mesh | `Block`（蓝图设） | 作为世界场景物体挡弹，**不扣伤害**（`Cast<AFPSCharacter>` 失败为 null，只发 OnHitWorld 特效） |

**LineTrace 参数**：`bTraceComplex=false`（走 Physics Asset 简化体，比逐三角面便宜），`bReturnPhysicalMaterial=true`（为日后部位判定/表面特效预留，物理材质 `SurfaceType4="Enemy"` 已在配置里）。`QueryParams` 忽略持枪者和武器自身。

**⚠️ 蓝图侧必做**：
1. 角色 Mesh（如 `Arm` / `CharacterMesh0`）**指定 Physics Asset** —— 否则 Mesh 的 query 碰撞没有形状，子弹穿人而过
2. 武器 Skeletal Mesh 若要作为世界挡弹物，在蓝图里 `Set Collision Response to Channel(Weapon, Block)`
3. 本地玩家 `GetMesh()->SetOwnerNoSee(true)` 只隐藏渲染不影响碰撞；玩家不会打到自己（trace 已 `AddIgnoredActor(OwningCharacter)`）

> 已知后续点：`Die()` 调 `SetActorEnableCollision(false)`，尸体不接子弹。若日后做 ragdoll 尸体，需注意死亡网格是否会挡活人子弹。

## 弹药系统（弹夹归武器 + 备弹归背包 + 口径匹配）

弹夹（`CurrentAmmo`）归 `AFPSWeapon` 管理；备弹作为 `UFPSAmmoItemDef` 条目存放在玩家背包中。换弹时从背包逐格取匹配口径的子弹补入弹夹。

### UFPSAmmoItemDef（DataAsset，继承 UFPSItemDef）

子弹是独立的 ItemDef 子类（Type 5），多了口径和子弹伤害字段：

| 字段 | 类型 | 说明 |
|---|---|---|
| `Caliber` | FName | 弹药口径（"5.56"、"7.62"、"9mm"等），武器用它匹配 |
| `BaseDamage` | float | 此弹药提供的单发额外伤害（加到武器 DamageAmount） |
| `Tier` | int32 | 继承自基类。不同 (Caliber, Tier) = 不同 DataAsset → 天然不堆叠 |

子弹不可直接"使用"（`GetItemUseType()=None`）；消耗由换弹时 `ConsumeAmmo` 执行。

### 武器配置（蓝图子类 EditDefaultsOnly）

| 属性 | 默认 | 含义 |
|---|---|---|
| `MagSize` | 30 | 弹夹容量 |
| `AcceptedCaliber` | NAME_None | 本武器接受的口径（匹配 `UFPSAmmoItemDef::Caliber`） |
| `ReloadTime` | 2.0 | 换弹耗时（秒） |
| 已移除 | — | `ReserveAmmo` / `InitialReserveAmmo`（备弹改由背包管理） |

### 运行时状态（服务端权威，复制）

| 属性 | 同步 | 说明 |
|---|---|---|
| `CurrentAmmo` | `ReplicatedUsing=OnRep_Ammo` | 弹夹内子弹数 |
| `bIsReloading` | `ReplicatedUsing=OnRep_IsReloading` | 换弹中（禁火 + 触发换弹动画） |
| `LoadedAmmoDef` | 仅服务端 | 当前弹夹内装填的弹药类型（决定单发伤害加成） |

### 蓝图可调用 getter（所有端）
`GetCurrentAmmo()` / `GetMagSize()` / `GetComputedReserveAmmo()` / `IsReloading()` / `CanReload()` / `GetAcceptedCaliber()` / `GetWeaponValue()`

`GetComputedReserveAmmo()` 实时遍历背包中所有匹配 `AcceptedCaliber` 的子弹条目，合计 Count。

### 数据流

```
开火 (HandleFire, 服务端) → --CurrentAmmo
  伤害 = DamageAmount + (LoadedAmmoDef ? LoadedAmmoDef->BaseDamage : 0)
  CurrentAmmo<=0 → StopFire（不空响、不自动换弹）

按 R → Reload(本地 CanReload 预判) → ServerReload RPC
  → ServerBeginReload → StopFire + bIsReloading=true → ReloadTimer(ReloadTime)
    → FinishReload：
      1. 调 Inventory->ConsumeAmmo(AcceptedCaliber, Needed, OutLoadedDef)
         → 从背包末尾向前遍历 UFPSAmmoItemDef 条目，逐格取弹
         → 更新 LoadedAmmoDef = 最后取弹的条目
         → 耗尽条目自动删格
      2. CurrentAmmo += 取到的发数
      3. Inv->MarkItemsDirty() → OnInventoryChanged → UI 刷新

复活 / 首次装备 → ServerResetAmmo()：满夹 + LoadedAmmoDef=nullptr
  伤害加成 0 直到首次换弹装填有 BaseDamage 的子弹
```

### 弹药拾取
子弹 Pickup（如 `BP_Pickup_Ammo556_T1`）走标准 AFPSPickup 流程 → 入背包为 `FInventoryEntry(ItemDef=DA_Ammo556_T1, Count=30)`。同口径同等级的子弹会堆叠；不同口径或不同等级天然分格。

### 关键设计
- **口径匹配**：武器 `AcceptedCaliber` 必须与 `UFPSAmmoItemDef::Caliber` 完全一致才可取弹
- **换弹不丢旧弹类型**：弹夹内旧弹类型在新弹装填时被覆盖（`LoadedAmmoDef` 更新为最后装填的条目）
- **ConsumeAmmo 消耗顺序**：从数组末尾向前（后进先出）取弹，耗尽条目自动 `RemoveAt`
- **mark dirty**：finish reload 后手动调 `MarkItemsDirty()` 保证 listen server host UI 刷新

## 道具 / 背包系统（C++ 服务端权威 + 蓝图填数值/UI）

地图上随机/手摆 Pickup，玩家走近按 F 拾取进背包，Tab 开背包点击使用。**逻辑全在 C++ 服务端权威**，蓝图只负责数据资产（道具数值）、Pickup/物品的网格、以及背包/拾取提示 UI。

### 三层身份分离（核心设计）

一个"道具"拆成三个独立概念，从一开始就分开，避免把整个 Actor 塞进背包：

| 概念 | 类 | 是什么 | 网络形态 |
|---|---|---|---|
| **Pickup（地上拾取物）** | `AFPSPickup` | 地图上能看到、能捡的 Actor（Mesh + Sphere 触发器） | 复制 Actor，服务端生成，捡走即 Destroy |
| **ItemDef（道具定义）** | `UFPSItemDef`(及子类) | "大血包"这个**数据**：名字/图标/数值/使用效果 | `UDataAsset`，所有端本地各持一份，**不复制**（背包只复制指向它的指针） |
| **InventoryEntry（背包格）** | `FInventoryEntry` | 背包里一条记录：ItemDef 指针 + 数量/耐久 | 复制的 USTRUCT，存在背包组件里 |

### 混合存储模型（`bUsesDurability` 决定）

`FInventoryEntry` 字段：`ItemDef` / `Count` / `Durability` / `GridX` / `GridY`。由 `ItemDef->bUsesDurability` 决定走 Count 还是 Durability：

| 类型 | 标志 | 字段 | 拾取行为 | 例子 |
|---|---|---|---|---|
| **可堆叠** | `bUsesDurability=false` | `Count`（≤MaxStack） | 并入现有同类格子 `Count+=` | 子弹、一次性药品 |
| **带耐久** | `bUsesDurability=true` | `Durability` | 每个新建一格，各管各的耐久，不堆叠 | 血包（治疗多少扣多少耐久，可多次用到耗尽） |

### 网格容量 + 空间占格升级预留

背包是**网格**：`UFPSInventoryComponent` 持有 `GridColumns × GridRows`（MVP 默认 3×5=15 格，设计上限 8×5）。UI 第 Index 个道具 → `Row=Index/Columns, Col=Index%Columns` 铺格。

**当前是"计数网格"**：每个道具占 1 格（`ItemDef->GridWidth=GridHeight=1`），`FInventoryEntry.GridX/Y` 填 -1（未定位），UI 按数组顺序铺。

**升级到"空间占格网格"（塔科夫式，已留好接口）**：道具有 `GridWidth×GridHeight`（大狙 1×5、医疗箱 2×2），放置时装箱算法找空位。**唯一替换点 = `UFPSInventoryComponent::FindSlotFor()`**——把里面的"判总数没满"换成装箱算法（建占用位图、扫描找空矩形、算左上角坐标），其余 `ServerAddItem/ServerAddEntry`/复制/数据流**一行都不用改**。代价：UI 要从 `UniformGridPanel` 自动铺改成 `CanvasPanel` 按 `(GridX,GridY)` 绝对定位 + 拖拽（表现层重做，不碰逻辑）。这是为"带出物品价值 / 战绩综合衡量"玩法预留。

### 道具效果：多态 ItemDef（方案 A，预留升级到方案 B）

`UFPSItemDef` 基类 virtual：`GetItemUseType()` / `GetCurrentValue(Entry)` / `ServerCanUse(User, Entry)` / `ServerUseItem(User, Entry)`。每种道具一个子类 override。背包组件只调 `ItemDef->ServerUseItem`，**不关心细节**。

`ServerUseItem` 返回 `int32`（消耗的耐久/数量），背包组件据此判删格。异步类道具（ChanneledHeal/HoTApply）的后续由 Character 状态机驱动。

#### 已有五种道具子类

| 子类 | 枚举值 | 存储 | 行为 | 例子 |
|---|---|---|---|---|
| `UFPSHealItemDef` | `Instant` | 耐久 | 点即回血，回血=扣耐久=min(缺血,耐久) | 急救包 |
| `UFPSChanneledHealItemDef` | `ChanneledHeal` | 耐久 | 按下后进入前摇→逐跳回血至承诺量交付完毕。期间锁奔跑/射击/瞄准/换弹/拾取；F 取消（已扣耐久不退还）。总时长=UseTime+ceil(承诺量/HealPerTick)×HealInterval | 三角洲大血包 |
| `UFPSHoTItemDef` | `HoTApply` | Count（可堆叠） | 使用耗时 UseTime 秒（锁动作），结束后激活 HoT buff。回血速度和时长上限由 Character 决定，ItemDef 仅提供持续时间（叠加延长至上限）。 | PUBG 止疼药 |
| `UFPSValuableItemDef` | `None` | Count | 不可使用，只用于带出计分。Value 字段填价值。 | 金条/钻石 |
| `UFPSAmmoItemDef` | `None` | Count（可堆叠） | 子弹：Caliber 口径 + BaseDamage 单发伤害加成 + Tier 等级。不同 (Caliber,Tier) 为不同 DataAsset，天然不堆叠。换弹时武器从背包取弹消耗。 | 5.56 T1/T2/T3 子弹 |

#### 基类通用字段（新增）

| 字段 | 类型 | 说明 |
|---|---|---|
| `Tier` | int32 | 道具等级（1=普通, 2=绿, 3=蓝, 4=紫, 5=金）。不同等级不堆叠 |
| `Value` | int32 | 基础价值。`GetCurrentValue(Entry)` 对耐久道具按剩余比例折算 |
| `UseTime` | float | 使用耗时（秒）。0=即时；>0=使用期间锁动作 |

#### 异步使用流程（ChanneledHeal / HoTApply）

```
Client UseInventoryItem → CloseInventory → ServerUseInventoryItem RPC
  → ServerUseInventoryItem_Implementation
    → Inventory->BeginItemUse(Index)  ← 扣耐久/数量，返回 FItemUseResult
    → if ChanneledHeal or HoTApply → Character->StartItemUseProcess(Result)
        ├─ ChanneledHeal: UseTime 前摇 → 逐跳回血(HealInterval) → CompleteItemUse
        ├─ HoTApply: UseTime → CompleteItemUse → ActivateHoT(buff 独立 timer)
        └─ F 取消 → CancelItemUse（已扣耐久不退还）
```

#### 动作锁定（IsActionLocked）

统一入口：`AFPSCharacter::IsActionLocked()` — 死亡/背包打开/使用道具中/换弹中 → 禁开火/瞄准/奔跑/换弹/拾取。不锁 WASD 移动和鼠标视角。

`Interact()`（F 键）：bIsUsingItem 时变为取消道具使用；否则受 IsActionLocked 检查并拾取。

#### HoT Buff 系统

- `AFPSCharacter` 配置：`HoTHealPerSecond`（每秒回血量）/ `HoTMaxBuffDuration`（时长上限）— 蓝图可调
- `FHoTState` 结构体（Replicated, COND_OwnerOnly）：`bActive / RemainingDuration / MaxDuration / HealPerSecond`
- `OnHoTChanged` 委托 — HUD 订阅驱动独立 buff 进度条
- 服务端每秒 tick 回血 + 递减剩余时间；客户端收到初始值后本地 countdown
- 叠加时 OnRep 推送新 RemainingDuration，客户端重置 countdown
- 死亡时 DeactivateHoT 清 buff

### 背包组件 `UFPSInventoryComponent`（挂在 Character 上）

- **复制**：`Items` 数组用 `COND_OwnerOnly` —— 背包只复制给本人，别人看不到你背包内容。
- **服务端权威接口**：
  - `ServerAddItem(Def)`：拾取全新道具，按 `DefaultValue` 给初值。
  - `ServerAddEntry(Entry)`：加一条已有状态的格（丢弃后捡回走这条，保留原耐久/数量）。`ServerAddItem` 内部也是造默认 Entry 后调它。
  - `ServerUseItem(Index)`：UI 点格使用。
  - `ServerDropItem(Index)`：丢弃——移除格 + 在玩家脚下 `SpawnActor(ItemDef->DropPickupClass)`，并 `Pickup->SetDroppedState(Entry)` 把耐久带到地上。
  - `ServerClear()`：预留复活清包。
- **listen server host 即时刷新**：同弹药系统约定，每处改 `Items` 后手动调 `OnRep_Items()`（host 不走复制），保证 host 自己 UI 即时更新。
- **事件**：`OnInventoryChanged`（BlueprintAssignable）—— 背包 UI 订阅刷新。
- **读取**：`GetItems()` / `GetMaxSlots()`（=列×行）/ `GetGridColumns()` / `GetGridRows()`。

### 拾取流程（走近按 F）

```
Pickup 的 Sphere overlap（所有端各自触发）
  → 每端本地角色 SetPickupTarget(Pickup) → OnPickupTargetChanged（蓝图显示"按F拾取 XXX"）
玩家按 F → AFPSCharacter::Interact → 有 CurrentPickupTarget 才发 ServerTryPickup RPC
  → 服务端 Pickup->ServerTryPickup(Picker)
      → Inventory->ServerAddItem(ItemDef)
          ├─ 成功 → Pickup->Destroy()（复制销毁，所有端消失）
          └─ 满背包 → 保留，触发 Pickup 蓝图 OnPickupBlocked（"背包已满"提示）
```

`CurrentPickupTarget` 是**本地状态不复制**（overlap 在每端各自维护）。`AFPSPickup` C++ 提供 `PickupMeshComponent`（UStaticMeshComponent），`OnConstruction` / `BeginPlay` 从 `ItemDef->PickupMesh` 自动加载网格体。专用子类可留空 DA 的 PickupMesh，改在蓝图层自行加模型。

### 背包 UI / 使用流程（E 开背包，右键菜单使用/丢弃）

```
按 E → AFPSCharacter::ToggleInventory（仅本地控制端）
  → 翻转 bInventoryOpen → 蓝图 OnToggleInventory(bShown)
      → 蓝图建/拆 WBP_Inventory + SetInputModeGameAndUI + 显示鼠标（打开仍可移动，仅放开鼠标点道具）
WBP_InventorySlot 右键 → 弹小菜单 WBP_SlotContextMenu（使用 / 丢弃）
  ├─ 使用 → Character.UseInventoryItem(Index) → ServerUseInventoryItem RPC → Inventory->ServerUseItem
  │         → ItemDef->ServerCanUse 校验 → ServerUseItem 改 Entry → 耗尽删格 → OnInventoryChanged → UI 刷新
  └─ 丢弃 → Character.DropInventoryItem(Index) → ServerDropInventoryItem RPC → Inventory->ServerDropItem
            → 脚下 SpawnActor(DropPickupClass) + SetDroppedState(保留耐久) → 移除格 → OnInventoryChanged → UI 刷新
```

### 蓝图可调用接口汇总

| 类 | 接口 | 用途 |
|---|---|---|
| AFPSCharacter | `GetInventory()` | 拿背包组件（遍历格子 / 订阅 OnInventoryChanged） |
| AFPSCharacter | `UseInventoryItem(Index)` (BlueprintCallable) | UI 右键菜单"使用" |
| AFPSCharacter | `DropInventoryItem(Index)` (BlueprintCallable) | UI 右键菜单"丢弃" |
| AFPSCharacter | `GetPickupTarget()` / `OnPickupTargetChanged` | 拾取提示 UI |
| AFPSCharacter | `OnToggleInventory(bShown)` (BlueprintImplementableEvent) | 蓝图实现背包面板显隐 + 输入模式切换 |
| AFPSCharacter | `IsInventoryOpen()` | 背包是否打开 |
| AFPSCharacter | `ServerApplyHeal(float)` | 服务端权威加血（供 HealItemDef 调，Health 是 protected） |
| AFPSCharacter | `IsActionLocked()` | 是否锁定战斗动作（死亡/背包/使用道具中/换弹中） |
| AFPSCharacter | `IsUsingItem()` / `GetHoTState()` | 是否正在使用道具 / HoT buff 状态 |
| AFPSCharacter | `OnHoTChanged(bActive, Remaining, Max)` | HoT buff 状态变化 — HUD 订阅驱动独立进度条 |
| UFPSInventoryComponent | `GetItems()` / `GetMaxSlots()` / `GetGridColumns()` / `GetGridRows()` / `OnInventoryChanged` | UI 数据 + 网格行列 + 刷新事件 |
| UFPSItemDef | `DisplayName / Icon / PickupMesh / bUsesDurability / MaxStack / DefaultValue / Value / UseTime / GridWidth / GridHeight / DropPickupClass` | 蓝图数据资产填的字段 |
| UFPSChanneledHealItemDef | `HealPerTick / HealInterval` | Type 2 引导治疗专用字段 |
| UFPSHoTItemDef | `HoTBaseDuration` | Type 3 持续回血：单次使用增加的持续时长 |
| AFPSPickup | `GetItemDef()` / `OnPickupBlocked()` / `PickupMeshComponent` | 拾取提示文字 / 满包提示 / C++ 自动管理的网格体组件 |

### 蓝图接入步骤（血包 MVP）

> C++ 编译通过后在 UE 编辑器中操作。新 C++ 类无需手动"创建"，编译后自动出现在编辑器。

1. **数据资产 `DA_HealLarge`**：右键 Content → Miscellaneous → Data Asset（中文编辑器：**其他 → 数据资产**）→ 弹"选取数据资产类"→ 搜 `FPSHealItemDef` 选中。填 `DisplayName="大血包"` / `Icon` / `DefaultValue=64`（初始耐久）/ `PickupMesh` 选药包 StaticMesh / `DropPickupClass=BP_Pickup_Generic`（下一步建完回填，用于丢弃）。
   > 基类 `UFPSItemDef` 是 `Abstract` 不可选，**只能选子类 `FPSHealItemDef`**——这是预期。若"数据资产"对话框里搜不到，是没编译/热重载没生效，关编辑器全量编译重开。
2. **通用 Pickup 蓝图 `BP_Pickup_Generic`**：继承 `AFPSPickup`，**不需要加任何组件**（C++ 已提供 `PickupMeshComponent`）。`ItemDef` 留空（由 Spawner/手摆时单独指定）。`DropPickupClass` 所有普通道具的 DA 都指向它。需要特殊拾取表现的道具才单独建 `BP_Pickup_xxx` 子类，此时 DA 的 `PickupMesh` 留空、`DropPickupClass` 指向专用子类。
3. **背包 UI（数据驱动，不是一个个拖框）**——见下方"### WBP 背包面板设计"详解。要点：做一个 `WBP_InventorySlot`（单格样式），父面板 `WBP_Inventory` 用 `UniformGridPanel` + 蓝图循环按 `GetItems()` 自动生成 N 个格子，订阅 `OnInventoryChanged` 刷新。
4. **IA 资产** `IA_Interact`(F) / `IA_Inventory`(E)，填到 `BP_FPSPlayer` 的 `InputInteract` / `InputInventory`。
   > **键位约定**：E=开/关背包，F=拾取，Tab=战绩面板（预留，未实现）。绑哪个物理键由蓝图 IA 资产决定，C++ 不关心。
5. **`BP_FPSPlayer` 蓝图事件**：
   - `OnPickupTargetChanged` → `GetPickupTarget` 非空显示"按 F 拾取 [GetItemDef.DisplayName]"，空则隐藏。
   - `OnToggleInventory(bShown)` → bShown 时 `Create Widget(WBP_Inventory) + Add to Viewport + SetInputModeGameAndUI + bShowMouseCursor=true`，否则移除 + `SetInputModeGameOnly + bShowMouseCursor=false`。
6. **手摆测试**：把 `BP_Pickup_Generic` 拖进 TestMap，设 `ItemDef=DA_HealLarge` → 受伤后走近按 F → E 开背包右键格子选使用/丢弃 → 验证回血/扣耐久/耗尽删格、丢弃落地保留耐久全端同步。

### WBP 背包面板设计（数据驱动，UniformGridPanel + 循环生成）

**核心：不手动拖 15 个框，而是设计 1 个格子 WBP + 蓝图循环按数组自动生成 N 个。** 一共 3 个 WBP：`WBP_InventorySlot`（单格样式）、`WBP_Inventory`（面板，循环生成格子）、`WBP_SlotContextMenu`（右键使用/丢弃菜单）。升级到塔科夫式占格背包时这套 UI 要重做（换 CanvasPanel 按坐标定位 + 拖拽），但逻辑层不动。

#### ① 单格 `WBP_InventorySlot`（控件选型）

```
SizeBox（锁死 64×64，保证每格等大）
└── Border（SlotBorder — 背景/边框，选中时改色）
    └── Overlay（让图标和数字叠放）
        ├── Image  SlotIcon（道具图标，Anchor 拉满铺格）
        └── Text   SlotCountText（Alignment=Right/Bottom，"x30" 堆叠数 或 "48" 耐久）
```
- **控件为什么这么选**：`SizeBox` 锁尺寸；`Overlay` 让图标铺满、数字浮右下角；`Border` 做选中高亮。
- **变量**：`SlotIndex`(int) / `OwnerCharacter`(BP_FPSPlayer 引用，菜单回调用)。
- **自定义函数 `InitSlot(Index, ItemDef, Count, Durability)`**（父面板生成时调）：
  - `Set SlotIndex = Index`
  - `SlotIcon.Brush.Texture = ItemDef.Icon`（ItemDef 字段都是 BlueprintReadOnly，直接拖引脚读）
  - `Branch(ItemDef.bUsesDurability)`：True→`SlotCountText = Durability 转字符串`；False→`SlotCountText = "x"+Count`
- **右键菜单**：重写 `On Mouse Button Down`：
  ```
  PointerEvent → Get Effecting Button == Right Mouse Button ?
    True → Create Widget(WBP_SlotContextMenu) → 存 Menu
         → Menu.TargetSlotIndex = SlotIndex
         → Menu.OwnerCharacter  = OwnerCharacter
         → Menu → Add to Viewport
         → Menu → Set Position in Viewport(PointerEvent.Screen Space Position)  ← 弹在鼠标处
         → Return Handled
    False → Return Unhandled
  ```
  > 想要"左键单击直接使用"可省掉菜单：左键 → 直接 `OwnerCharacter.UseInventoryItem(SlotIndex)`。

#### ② 父面板 `WBP_Inventory`（循环生成 + 解决 For 循环取值）

容器层级：`Border（半透明黑底）→ UniformGridPanel`（命名 `SlotGrid`，**Designer 里勾 Is Variable** 否则 Graph 引用不到）。

```
Event Construct：
  Get Owning Player Pawn → Cast BP_FPSPlayer → 存 OwnerCharacter
  → GetInventory → Bind Event to OnInventoryChanged → 自定义事件 RefreshInventory
  → 立刻调一次 RefreshInventory

RefreshInventory（自定义事件）：
  1. SlotGrid → Clear Children
  2. GetInventory → GetItems  →  For Each Loop（带 Index）：
       ├ Array Element ──→ Break FInventoryEntry      ★关键！见下
       │                     输出: ItemDef / Count / Durability / GridX / GridY
       ├ Create Widget(Class=WBP_InventorySlot) → 存 NewSlot
       ├ NewSlot.OwnerCharacter = OwnerCharacter
       ├ NewSlot → InitSlot(Array Index, ItemDef, Count, Durability)   ← 用 Break 出来的字段
       └ SlotGrid → Add Child to Uniform Grid(NewSlot)：
            Row    = Array Index / GetGridColumns   （整数除）
            Column = Array Index % GetGridColumns   （取余 Modulo）
  3.（可选）补铺空灰格到 GetMaxSlots 个，视觉上像满网格
```

> **⚠️ For 循环取不到值的根因 = 漏了 `Break FInventoryEntry`**。`For Each Loop` 的 **Array Element** 引脚给的是整个 `FInventoryEntry` **结构体**，不能直接当值用。必须右键该引脚 → **Split Struct Pin**（或拖出来搜 `Break FInventoryEntry`），才能拿到里面的 `ItemDef`/`Count`/`Durability`。`Array Index` 引脚直接就是 int，给 InitSlot 和算 Row/Col 用。
> **Row/Col 算法**：Columns=5 时第 7 个道具 → Row=7/5=1, Col=7%5=2 → 第 1 行第 2 列。

- 有几个道具就生成几个格子，全自动。服务端改背包→`Items` 复制→`OnInventoryChanged`→`RefreshInventory` 自动重画。
- 列数从 `GetGridColumns()` 读（MVP=5），行数 `GetGridRows()`（MVP=3）。**格数变了 UI 自动适配**——未来"套娃背包扩容"改 GridColumns/Rows，UI 一行不用动。

#### ③ 右键菜单 `WBP_SlotContextMenu`

```
Border → Vertical Box
  ├── Button UseButton  → Text "使用"
  └── Button DropButton → Text "丢弃"
变量：TargetSlotIndex(int) / OwnerCharacter(BP_FPSPlayer 引用)
回调：
  UseButton  OnClicked → OwnerCharacter.UseInventoryItem(TargetSlotIndex)  → Remove from Parent
  DropButton OnClicked → OwnerCharacter.DropInventoryItem(TargetSlotIndex) → Remove from Parent
```

#### ④ WBP 完成后怎么挂（关键：在 BP_FPSPlayer.OnToggleInventory 里建/拆）

C++ 已绑 E 键 → `ToggleInventory()` 翻转 `bInventoryOpen` → 触发蓝图 `OnToggleInventory(bShown)`。在 `BP_FPSPlayer` 实现该事件：
```
Event OnToggleInventory(bShown)：
  Branch(bShown)：
    True（打开）:
      ├ Create Widget(WBP_Inventory) → 存成 BP_FPSPlayer 的变量 InventoryWidgetRef
      ├ InventoryWidgetRef → Add to Viewport
      ├ Get Player Controller → Set Input Mode Game And UI(InMouseLockMode=DoNotLock)
      └ Get Player Controller → Set Show Mouse Cursor = true
    False（关闭）:
      ├ InventoryWidgetRef → Remove from Parent（IsValid 判一下）
      ├ Set Input Mode Game Only
      └ Set Show Mouse Cursor = false
```
- **Set Input Mode Game And UI**：让鼠标能点 UI，**但角色仍能移动**（符合"打开不暂停、放开鼠标点道具"）。
- ⚠️ 用变量 `InventoryWidgetRef` 存住引用，关闭时才能 Remove。别每次新建不删，会叠面板。

### 后续丰富方向（按优先级）

| 道具 | 类型 | 实现要点 |
|---|---|---|
| 小血包/立即治疗药 | 可堆叠或带耐久 | 同 HealItemDef，调 DefaultValue |
| 持续治疗针（HoT） | 升级方案 B | `UDurationHealEffect`：Character/BuffComponent 注册定时器每秒回血，复制剩余时间给 UI |
| 护甲 | 新增 Armor 属性 | Character 加 `Armor`（Replicated）+ 伤害先扣甲；`UArmorItemDef` 加甲 |
| 护甲修补片 | 带耐久 | `UArmorRepairItemDef`：补 Armor，按修补量扣耐久 |
| 各级子弹 | 可堆叠 | `UAmmoItemDef`：`ServerUseItem` 给当前武器 `ReserveAmmo +=`；按弹种匹配武器 |
| 自动刷新点 | Spawner 系统 | `AFPSPickupSpawner` Actor 按周期生成指定 Pickup（类似 CS/Quake），或 GameMode 随机撒点 |
| **套娃背包（装备背包扩容）** | 装备槽 + 扩容 | 用户明确要：角色有固定基础格（如 3×5），装备"背包道具"后扩到该背包的格数（上限 8×5）。见下方设计 |

### 套娃背包升级设计（背包里放背包，预留接口已就位）

**目标**：角色默认有基础网格（`GridColumns/Rows` 蓝图默认值，如 3×5）。有一个独立"背包装备槽"，放入不同等级的背包道具后，把网格扩到该背包定义的格数（设计上限 8×5）。UI 因为本来就按 `GetGridColumns()/GetGridRows()` 动态铺格，**扩容后自动适配，一行不用改**。

**这轮（MVP）先不做**——它引入递归依赖（格数由"装备了哪个背包"决定，而背包道具又占一格）。MVP 用固定格数把全链路跑通。

**升级落地步骤**（下一轮）：
1. 新增 `UFPSBackpackItemDef : UFPSItemDef`，加 `GridColumns / GridRows`（该背包提供的格数，≤8×5）。
2. `UFPSInventoryComponent` 加一个**独立装备槽** `TObjectPtr<UFPSItemDef> EquippedBackpack`（不占普通格子，单独 Replicated）+ `ServerEquipBackpack(Def)`：把组件的 `GridColumns/Rows` 改成该背包的值，重算放置。
3. **缩容处理**：卸下大背包换小背包时，超出新容量的道具——掉到脚下（复用 `ServerDropItem` 逻辑）或拒绝卸下。需明确规则。
4. UI 侧：背包装备槽做成一个特殊格子（只接受背包类道具的拖入）；主网格不变。
5. **替换点仍是 `FindSlotFor()`**：扩容只是改了 `GridColumns/Rows`，放置逻辑本身不动。

## UI / HUD 架构（UMG，蓝图主体 + C++ 数据接口）

**实现机制**：游戏 UI = UMG（Slate 的蓝图封装）。一个"界面"是继承 `UUserWidget` 的 **WBP（Widget Blueprint）**。WBP 在编辑器里拖控件搭建，二进制 uasset，**不能命令行生成**。

**数据交互**：WBP 读 C++ 数据有三条路径——
1. **Getter 调用**：`GetHealth/GetMaxHealth/GetCurrentAmmo/...`（一次性/初始化时读）
2. **Property Binding**：ProgressBar.Percent / Text.Content 绑函数（每帧自动拉）
3. **事件驱动 Set**（最高效）：订阅 `OnHealthChanged` / `OnAmmoChanged`，仅变化时刷新

**谁创建 HUD**：`AFPSPlayerController`（纯本地，`IsLocalController()` 判断只给本机玩家建）。当前 HUD 创建逻辑待加（见下方"待办"）。

### 当前 HUD 数据接口现状
| HUD 元素 | 数据来源 | C++ 现状 |
|---|---|---|
| 准星 | 无需数据，纯 WBP | ✅ 直接画 |
| 血条 | `GetHealth()/GetMaxHealth()` + `OnHealthChanged(Health, MaxHealth)` 事件 | ✅ 已有 |
| 弹药（弹夹/备弹） | `GetCurrentAmmo()/GetReserveAmmo()/GetMagSize()` + `OnAmmoChanged` 事件 | ✅ 已有 |
| 背包面板（E 开） | `GetInventory()->GetItems()` + `OnInventoryChanged` 事件 + `UseInventoryItem()/DropInventoryItem()` | ✅ 已有（详见"道具/背包系统"） |
| 拾取提示（按 F） | `GetPickupTarget()` + `OnPickupTargetChanged` 事件 | ✅ 已有 |
| Tab 计分板：名字/击杀/死亡 | `APlayerState::GetPlayerName()` / `AFPSPlayerState::Kills/Deaths` | ✅ 已有 |
| Tab 计分板：总伤害 | `AFPSPlayerState::TotalDamage`（TakeDamage 中给攻击者累加权威 ActualDamage） | ✅ 已有 |
| Tab 计分板：带出价值 | `AFPSPlayerState::CarryValue`（预留字段，恒 0，未来带出点累加） | ✅ 字段就绪 |
| Tab 计分板：头像 icon | `AFPSPlayerState::PlayerIcon`（TSoftObjectPtr，预留，未来登录系统设置） | ✅ 字段就绪 |
| Tab 计分板：爆头率 | 需爆头数+总命中统计，依赖部位判定 | ❌ 待加 |

### WBP 搭建指南（这轮：准星 / 血条 / 弹药）

> C++ 接口已就绪，以下在 UE 编辑器中操作。

**1. 准星 `WBP_Crosshair`（纯表现，零数据）**
- New → User Interface → Widget Blueprint，命名 `WBP_Crosshair`
- 根换成 `Canvas Panel`，放一个 `Image`（或四条 `Border` 拼十字）
- Anchor 设中心（0.5,0.5），Alignment (0.5,0.5)，Position (0,0) → 正屏幕中心

**2. 血条 `WBP_Health`**
- 放一个 `ProgressBar`（命名 `HealthBar`）+ 可选 `Text`（显示数值）
- **初始化**：`Event Construct` → `Get Owning Player Pawn → Cast to BP_FPSPlayer`：
  - `GetHealth / GetMaxHealth` → `Health/MaxHealth` 设 `HealthBar.Percent`
  - **Bind**（关键）：`OnHealthChanged`（从 Cast 出的角色拖出红色事件节点）→ 自定义事件 `RefreshHealth(Health, MaxHealth)` → 设 `HealthBar.Percent = Health/MaxHealth`
- 角色复活后是同一个 Pawn，事件持续有效；若做"死亡换 Pawn"需在新 Pawn possess 后重绑

**3. 弹药 `WBP_Ammo`**
- 两个 `Text`：`CurrentAmmoText` / `ReserveAmmoText`
- **初始化**：`Event Construct` → `Get Owning Player Pawn → Cast to BP_FPSPlayer → GetWeapon → Cast to BP_WeaponBase`：
  - 先读 `GetCurrentAmmo / GetReserveAmmo` 设初值
  - **Bind** 武器的 `OnAmmoChanged(Current, Reserve)` → 自定义事件 → 设两个 Text
- ⚠️ **时序坑**：客户端 `Event Construct` 时武器可能还没复制过来（`GetWeapon` 返回 None）。推荐：在 **`BP_FPSPlayer.OnWeaponEquipped`** 里再去绑武器的 `OnAmmoChanged`（此时武器已就绪），或 HUD 里加重试。最稳：HUD 创建延后到 OnWeaponEquipped 之后

**4. 挂到屏幕**（二选一）
- **方案 A（推荐，纯蓝图先跑通）**：在 `BP_FPSPlayer` 的 `BeginPlay` → `Is Locally Controlled` 为真 → `Create Widget(WBP_HUD)` → `Add to Viewport`。把准星/血条/弹药放进一个总 `WBP_HUD` 里
- **方案 B（规范，C++ 控制）**：让 `AFPSPlayerController` 持有 `TSubclassOf<UUserWidget> HUDClass`（蓝图填），在 `OnPossess`/`BeginPlayingState` 中 `IsLocalController()` 时创建——待加

## 战绩面板 / 计分板系统（C++ 逻辑全包 + 蓝图摆控件）

Tab 战绩面板。**逻辑全在 C++**：数据收集、排序、Tab 开关、两段式鼠标锁定都在 C++；蓝图只继承 C++ WBP 基类摆控件、调暴露的函数。**挂在 `AFPSPlayerController`（玩家级 UI，与具体 Pawn 无关，死亡换 Pawn 不丢），不挂 Character**——这点与"背包挂 Character（Pawn 能力）"是有意区分。

### 数据字段（`AFPSPlayerState`，服务端权威复制）

| 字段 | 类型 | 同步 | 来源 |
|---|---|---|---|
| `Kills` / `Deaths` | int32 | ReplicatedUsing | 既有 |
| `TotalDamage` | float | ReplicatedUsing=OnRep_TotalDamage | `AFPSCharacter::TakeDamage` 给攻击者 PS 调 `AddDamage(ActualDamage)`，排除自伤 |
| `CarryValue` | float | ReplicatedUsing=OnRep_CarryValue | **预留**，恒 0。未来不定期随机开放带出点，玩家带出可拾取 Actor 时按其价值调 `AddCarryValue` |
| `PlayerIcon` | TSoftObjectPtr\<UTexture2D\> | Replicated | **预留**，空。未来登录界面设置头像后赋值。用软引用避免计分板未开也强加载所有头像 |

`AddDamage/AddCarryValue` 均服务端权威（`HasAuthority()` 守卫，Amount<=0 忽略）。

### 默认玩家名（`AFPSGameMode::PostLogin`）

服务端在 `PostLogin` 用 `++PlayerJoinCount` 给登入玩家设 `SetPlayerName("client1"/"client2"/...)`。后续登录系统可用 `APlayerState::SetPlayerName` 覆盖。

### C++ WBP 基类 `UFPSScoreboardWidget : UUserWidget`

逻辑层，WBP 继承它。模块已加 `UMG/Slate/SlateCore` 依赖。

- **枚举 `EScoreboardSortKey`**：`Kills` / `TotalDamage` / `CarryValue`（新增可排序字段在此追加 + 在 `RowLess` 加分支）。
- **结构体 `FScoreboardRow`（BlueprintReadOnly）**：`PlayerName` / `Icon` / `Kills` / `Deaths` / `TotalDamage` / `CarryValue` / `bIsLocalPlayer`。WBP 只读这个铺行，不直接碰 PlayerState。
- **状态（本地，不复制）**：`SortKey`（默认 Kills）/ `bDescending`（默认 true 降序）。排序是每端本地视图，底层数值由 PlayerState 权威复制，各端一致。
- **BlueprintCallable 接口**：
  - `RefreshRows()`：遍历 `GameState->PlayerArray` 收集 → 按当前键+方向 `Sort` → 触发 `OnRowsUpdated`。显示时/排序变化时调。
  - `SetSortKey(枚举)` / `ToggleSortOrder()` / `SetDescending(bool)`：改排序后内部自动 `RefreshRows`。
  - `GetSortedRows()` / `GetSortKey()` / `IsDescending()`：BlueprintPure 读取。
- **`OnRowsUpdated()`（BlueprintImplementableEvent）**：WBP 实现 = Clear 表格 → 遍历 `GetSortedRows()` 生成行控件。
- **排序稳定性**：主键相等 → 用 Kills 兜底 → 再用 PlayerName 保证确定顺序。跳过 `IsOnlyASpectator()` 玩家。

### Tab 开关 + 两段式鼠标锁定（`AFPSPlayerController` 全包）

**蓝图填的 UPROPERTY**（Controller Class Defaults → Scoreboard 分类）：`ScoreboardClass`（WBP 类，须继承 UFPSScoreboardWidget）/ `ScoreboardMappingContext`（计分板专用 IMC）/ `InputScoreboard`(Tab) / `InputScoreboardClickLeft`(左键) / `InputScoreboardClickRight`(右键)。

- **IMC 注册**：`BeginPlay` 中 `SetTimerForNextTick` 注册 `ScoreboardMappingContext`，**优先级 1**（高于角色 IMC 的 0）。
- **两段式交互**：
  1. **Tab 按下**（`OnScoreboardPressed`）：创建/显示计分板 + `RefreshRows`。`SetInputMode(GameOnly)`、不弹鼠标——**纯查看，仍可移动/开火/瞄准**。若背包开着先 `CloseInventory`（两全屏 UI 互斥）。
  2. **面板开时按左/右键**（`OnScoreboardClick`）：切 `GameAndUI` + 弹鼠标 + `SetWidgetToFocus(计分板)`。此时**左右键进 UI 点排序按钮，不再开火/瞄准**——靠 `AFPSCharacter::StartFire/StartAim` 入口查 `IsScoreboardMouseLocked()` 直接 return；键盘移动仍生效（GameAndUI 不拦键盘）。锁定瞬间调 `Char->ForceStopFireAndAim()` 兜底停掉已按住的开火/瞄准。
  3. **Tab 松开**（`OnScoreboardReleased`）：隐藏面板 + 解除锁定 + 复位 GameOnly。
- **互斥**：计分板打开期间 `AFPSCharacter::ToggleInventory` 查 `PC->IsScoreboardOpen()` 直接 return（Tab 时不能开背包）。
- **widget 复用**：`ScoreboardWidget` 懒创建、复用同一实例，仅切 Visibility。`EndPlay` 移除。

### Character 侧配合（最小改动，复用 bInputLocked 模式）

- `StartFire()` / `StartAim()` 入口加 `if (IsScoreboardMouseLocked()) return;`。
- `IsScoreboardMouseLocked()` 私有 helper：Cast Controller 查 `IsScoreboardMouseLocked()`。
- `ForceStopFireAndAim()`（public）：供 Controller 锁鼠标瞬间兜底停火/停瞄。
- `ToggleInventory()` 入口加计分板互斥判断。

### 蓝图接入步骤（WBP_Scoreboard）

1. **行 WBP `WBP_ScoreboardRow`**：摆 Image(icon) + Text(名字/击杀/死亡/伤害/带出价值)。自定义函数 `InitRow(FScoreboardRow)` 填各控件；`bIsLocalPlayer` 为真时高亮背景。icon 软引用为空时用占位图。
2. **主面板 `WBP_Scoreboard`（继承 `UFPSScoreboardWidget`）**：
   - 表格容器（`ScrollBox`/`VerticalBox`，勾 Is Variable）+ 排序下拉框 `ComboBoxString`（选项 Kills/Damage/CarryValue）+ 升降序按钮。
   - 重写 `OnRowsUpdated`（C++ 事件）：Clear 容器 → `For Each` `GetSortedRows()`（**记得 Split / Break FScoreboardRow**）→ Create `WBP_ScoreboardRow` → `InitRow` → Add。
   - 下拉框 `OnSelectionChanged` → 映射到枚举 → 调 `SetSortKey`；升降序按钮 `OnClicked` → 调 `ToggleSortOrder`（都自动刷新）。
3. **Controller 蓝图（BP_FPSPlayerController，须继承 AFPSPlayerController）**：Class Defaults → Scoreboard 分类填 `ScoreboardClass=WBP_Scoreboard`、`ScoreboardMappingContext`、三个 IA。
4. **IA 资产**：`IA_Scoreboard`(Tab，Trigger 用 Down/Pressed) / `IA_ScoreboardClickLeft`(左键) / `IA_ScoreboardClickRight`(右键)，全放进 `ScoreboardMappingContext`。
   > 注意 GameMode 的 `PlayerControllerClass` 须是这个 BP_FPSPlayerController（或在 C++ GameMode 默认值基础上蓝图覆盖）。

### 后续扩展

- **带出价值**：实现带出点 Actor + 玩家带出时服务端调 `PS->AddCarryValue(物品价值)`；可拾取 Actor 加价值字段。
- **头像**：登录系统设置后赋 `PlayerIcon`；UI 用 `TSoftObjectPtr` 异步加载。
- **爆头率**：`AFPSPlayerState` 加爆头数/总命中，依赖 `HandleFire` 用 `Hit.PhysMaterial`/`Hit.BoneName` 判头。

## 网络数据流

### 开火流程（服务端权威）
```
客户端扣扳机 → ServerStartFire (RPC Reliable)
  → 服务端 CurrentWeapon->StartFire → FireTimer 每 FireRate 调 HandleFire
    → 服务端从 Pawn 视角执行 LineTrace（权威碰撞检测）
      ├─ 命中 → ApplyDamage → 血量复制到所有客户端 → ≤0 死亡
      └─ 未命中 → 无操作（客户端自主播放枪口特效）
```

### 死亡/复活流程
```
Health ≤ 0 → 服务端处理：
  1. 攻击者 PlayerState.Kills++
  2. 受害者 PlayerState.Deaths++
  3. 受害者 Character.Die() → 禁用移动/碰撞，播放死亡动画
  4. 3s 后 → 服务端 ChoosePlayerStart → RestartPlayer
```

### 比赛流程
```
服务器 BeginPlay → StartMatch()
  → 5 分钟倒计时 (GameState.TimeRemaining)
  → 倒计时归零 → EndMatch() → 显示比分
  → 可重新开始
```

## 复制数据

| 属性 | 所属类 | 同步方式 |
|---|---|---|
| Health | FPSCharacter | RepNotify |
| bIsDead | FPSCharacter | Replicated |
| MatchStage | FPSGameState | Replicated |
| TimeRemaining | FPSGameState | Replicated |
| Kills | FPSPlayerState | ReplicatedUsing |
| Deaths | FPSPlayerState | ReplicatedUsing |
| Items | FPSInventoryComponent | ReplicatedUsing（COND_OwnerOnly，仅复制给本人） |

## RPC 接口

| 函数 | 方向 | 可靠性 | 作用 |
|---|---|---|---|
| ServerSetWantsToRun | Client→Server | Reliable | 同步跑步状态到服务端 |
| ServerSetWantsToAim | Client→Server | Reliable | 同步瞄准状态到服务端 |
| ServerStartFire | Client→Server | Reliable | 按住连发 |
| ServerStopFire | Client→Server | Reliable | 松开扳机 |
| ServerReload | Client→Server | Reliable | 换弹（服务端权威校验 + 补弹） |
| MulticastOnAimStateChanged | Server→All | Reliable | 远程客户端触发 OnAimStarted/Stopped |
| MulticastOnRunStateChanged | Server→All | Reliable | 远程客户端触发 OnRunStarted/Stopped |
| MulticastOnFireStateChanged | Server→All | Reliable | 远程客户端触发 OnFireStarted/Stopped |
| MulticastHitConfirmed | Server→All | Unreliable | 权威命中信息（武器上，各端 DispatchHitFX 分发 OnDealtHit/OnReceivedHit/OnHitWorld） |
| ServerTryPickup | Client→Server | Reliable | 拾取当前范围内 Pickup（服务端校验 + 入背包 + 销毁） |
| ServerUseInventoryItem | Client→Server | Reliable | 使用背包第 N 格道具（服务端权威执行 ItemDef 效果；异步类启动状态机） |
| ServerDropInventoryItem | Client→Server | Reliable | 丢弃背包第 N 格道具（移除 + 脚下生成 Pickup，保留耐久） |
| ServerCancelUseItem | Client→Server | Reliable | 取消当前道具使用（F 键，已扣耐久不退还） |
| ServerSwitchWeapon | Client→Server | Reliable | 切换活跃武器槽（0=主，1=副） |
| ServerTryPickupWeapon | Client→Server | Reliable | 拾取当前范围内武器 Pickup（交换/装备/掉落） |
| ServerSubmitAllValuables | Client→Server | Reliable | 一键提交背包所有贵重品到最近开放提交点 |
| ServerSubmitSingleItem | Client→Server | Reliable | 单独提交背包第 N 格到最近开放提交点 |
| ClientRespawn | Server→Client | Reliable | 通知复活 |

## 编译与运行

### 首次设置
1. 右键 `tFPS_c.uproject` → **Generate Visual Studio project files**
2. 打开生成的 `.sln`，选择 `Development_Editor` 编译
3. 在 UE 编辑器中确认 GameMode 设置为 `FPSGameMode`

### Listen Server 测试
**方式一（编辑器）：** 点击 Play 按钮旁的箭头 → Advanced Settings → Play Mode: **Play As Listen Server**

**方式二（独立运行）：**
```
# 服务端（同时也是玩家）
tFPS_c.exe TestMap -game -Port=7777

# 客户端连接
tFPS_c.exe 127.0.0.1:7777 -game
```

### Dedicated Server（未来）
```
# 编译 Server 目标后
tFPS_cServer.exe TestMap -log
tFPS_c.exe 127.0.0.1:7777 -game
```

## 地图准备
当前使用 `TestMap.umap`，需要在其中放置：
- 至少 2-4 个 `APlayerStart` 作为随机出生点
- 玩家需要行走的地面/平台（有碰撞）

## 常见编译错误与注意事项

### UHT 类型解析失败（"无法打开源文件 CoreMinimal.h" 等连锁报错）

**根因**：在 `.h` 的 `UPROPERTY` 里用了 `TSoftObjectPtr<T>` / `TObjectPtr<T>` / `TSubclassOf<T>`，但没有前置声明 `T`。UHT 解析时遇到未知类型 → 生成错误的 `.generated.h` → 所有依赖该头文件的编译单元连锁爆炸，表现是一堆 UE 核心头文件（`CoreMinimal.h`、`UObject/ObjectMacros.h`、`Engine/DataAsset.h`）也找不到。

**症状**：
```
无法打开 源 文件 "UObject/ScriptMacros.h"
无法打开 源 文件 "Engine/DataAsset.h"
无法打开 源 文件 "CoreMinimal.h"
未定义标识符 "uint8"
缺少显式类型(假定"int")
```

**修复**：在头文件顶部（`#include "xxx.generated.h"` 之前）加 `class UXXX;` 前置声明。

**常见需要前置声明的类型**：
- `class UStaticMesh;`
- `class UTexture2D;`
- `class UAnimMontage;`
- `class USkeletalMesh;`
- `class UNiagaraSystem;`
- 任何在 `UPROPERTY` 的模板参数中用到的自定义 UObject 子类

**注意**：向前置声明和 `#include` 之间，头文件里优先用前置声明（减少编译依赖），`.cpp` 里再 `#include` 完整头文件。

## 编码规范
- All public/private/protected 显式标记
- 网络相关函数加 `/* Server */` / `/* Client */` 注释
- 使用 UE5 标准命名规范（类前缀，类型后缀）

## 蓝图结构记录

> 此章节由 Claude 根据用户提供的截图维护，与代码同步更新。

### `BP_FPSPlayer` (继承 AFPSCharacter)

**组件层级**：
```
BP_FPSPlayer (Self)
└── CollisionCylinder (胶囊体, 继承自 Character)
    └── Arm (SkeletalMeshComponent — 第一人称手臂)
        ├── Camera
        ├── Arrow
        └── CharacterMesh0 (继承的 Mesh 组件, 通常隐藏)
```

**关键事件/函数**：
- `OnWeaponEquipped`（C++ BlueprintImplementableEvent）：`GetWeapon → IsValid → Cast To BP_WeaponBase → Attach Actor To Component(Parent=Arm, Socket=None)`。Arm 和武器使用同一套骨架/动画对位，attach 到 Arm 根节点（无 socket）武器即正确贴合到手部
- `OnFireStarted_Function / OnFireStopped_Function`：开火状态变化时触发动画/音效
- `OnAimStarted_Function / OnAimStopped_Function`：瞄准状态变化时切 FOV/动画
- `BecomeRunningCauseStopFire`：跑步打断开火处理
- `ApplyRemoteArmRotation`：远端玩家用 ArmPitch 调整手臂俯仰
- `UpdateDefaultFOV / UpdateScopeSensitivity`：FOV 与开镜灵敏度更新

**变量**：
- `InAimFOVHandle / OutAimFOVHandle`（定时器柄）：FOV 平滑过渡
- `AimFOV`（浮点）：瞄准时的 FOV 目标值

### `BP_WeaponBase` / `BP_VIRTUS` (继承 AFPSWeapon)

`BP_WeaponBase` 是抽象基类，不直接作为武器使用；`BP_VIRTUS` 等子类继承它并填具体的 Skeletal Mesh、蒙太奇资产。

**组件层级**（应为）：
```
BP_WeaponBase
└── DefaultRoot (USceneComponent, C++ 提供)
    └── SkeletalMesh (蓝图添加 — 枪体)
        └── Muzzle Socket (用于枪口特效)
```

**EventGraph 实现要点**：
- C++ 已不再提供 `OnFireBP/OnStopFireBP`。所有开火特效完全由角色端蓝图 `OnFireStarted/Stopped` 事件驱动
- `Event StartWeaponFire`（BI_Fire 接口）+ `FireInternal` + 内部 timer 仍可作为蓝图工具函数被角色端蓝图调用，传入 `Player` 参数
- `Play Montage` 用 `Player.MovementState` 选 Idle/Walk/Run/Aim 对应的蒙太奇

**角色端调用链**（[BP_FPSPlayer](file:///d:/CPP/UE/tFPS) `OnFireStarted_Function`）：
```
OnFireStarted_Function → Branch(!IsFiring) → Branch(!IsDead) → Branch(MovementState != Running)
  → Cast to BP_WeaponBase(GetWeapon)
  → StartWeaponFire(Target=武器, Player=Self)   ← 触发武器内连发 timer + Play Montage 链
```
`OnFireStopped_Function` 对称调用 `EndWeaponFire`。

**辅助 C++ getter**：
- `GetOwningCharacter()` —— 蓝图可调用，所有端可用（OwningCharacter 是 Replicated）。可用于不经角色调用的场景（如 ABP_VIRTUS）
- `GetFireRange()/GetFireRate()` —— 蓝图查询常数，用于本地 timer 间隔和 Niagara Beam 终点距离

### `ABP_VIRTUS` (武器 SkeletalMesh 的 AnimBP)

**意图**：从角色读取 MovementState/Move X/Y/Character Movement 等数据，驱动武器自身骨骼动画（如握把、扳机指）。

**当前实现要点**：
- `Event Blueprint Initialize Animation`：尝试 `Cast To BP_VIRTUS`（拿武器自身）→ `Get Owner` → `Cast To BP_FPSPlayer`，缓存为 `As ABP FPSPlayer`
- `Event Blueprint Update Animation`：从 `As ABP FPSPlayer` 读取 `Move X / Move Y / Movement State / Character Movement`，赋给 AnimBP 局部变量

**已知问题**：`Initialize Animation` 触发时机早于 `OnWeaponEquipped`，武器尚未 attach 到角色 Arm，`GetOwner()` 在客户端可能为 None（COND_OwnerOnly），导致 Cast 失败 → Update 每帧报 "尝试读取 As ABP FPSPlayer 时结果为无"。**修复方案**：见下方"蓝图调整指南"。

### 蓝图调整指南（修复当前 bug）

#### 1. 修复 `ABP_VIRTUS` 的 Owner Cast 报错
- **删除** `Event Blueprint Initialize Animation` 里的所有 Cast 逻辑（保留事件节点本身）
- **改为在 `Event Blueprint Update Animation` 里每帧重新解析**：
  ```
  Try Get Pawn Owner  ──┐
  (失败时回退到)         ├──> Cast to BP_FPSPlayer ──> Set As ABP FPSPlayer ──> 读取 Move X/Y/MovementState...
  Get Owning Actor      │
   └─ Get Attach Parent Actor (循环到顶层 Character)
  ```
- 推荐做法：定义一个本地纯函数 `ResolveOwningCharacter`，逻辑为：
  1. `Get Owning Component → Get Attach Parent → Get Owner`（武器 attach 到 Arm，Arm 的 Owner 就是 Character）
  2. Cast 失败再回退到 `Try Get Pawn Owner`
- 这样在武器还没 attach 的前几帧 Cast 失败，但因为是 Update 每帧重试，attach 完成后立即恢复正常

#### 2. `BP_FPSPlayer.OnWeaponEquipped` 的 Attach 设置
当前节点设置实际是正确的（**Socket=None 是预期行为** —— Arm 和武器骨架对位，不需要 socket）：
- **Location/Rotation/Scale Rule** 建议全部 `Snap to Target`
- **Weld Simulated Bodies** 保持勾选即可
- 失败分支的 `Print String "WeaponEquipped_Failed_!!!"` 可保留作诊断
- 武器不跟随的真正原因是问题 1（ABP Cast 失败导致 AnimBP 没在驱动骨骼），修好 ABP 后位置即正常
