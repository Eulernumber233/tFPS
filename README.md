# tFPS — 多人混战 FPS

基于 **Unreal Engine 5.7** 的多人第一人称射击游戏 Demo，采用 **Listen Server** 架构，支持局域网联机。

## 游戏规则

- **模式**：Deathmatch 混战（FFA，无队伍），每人各自为战
- **人数**：上限 10 人
- **时长**：5 分钟一局
- **死亡**：血量归零后 3 秒随机复活
- **胜利条件**：时间结束时，击杀数最高的玩家获胜

## 当前功能

### 战斗系统
- **主副武器**：最多同时持有两把枪，按 `1`/`2` 切换
- **开镜瞄准**：按住右键降低移速、缩小 FOV
- **弹药系统**：弹夹 + 备弹分离，换弹时从背包取匹配口径的子弹装填
- **命中反馈**：权威射线检测，命中/被命中/命中环境三类事件驱动 UI 和特效

### 背包 & 道具
- **网格背包**（默认 3×5 格），Tab 开背包、右键使用/丢弃
- **血包**：即时治疗、引导治疗、持续回血（HoT）三种
- **子弹**：不同口径 (5.56/7.62/9mm) × 等级 (T1-T5)，武器口径匹配
- **贵重品**：金条/钻石等，可在提交点提交累加带出价值
- **套娃背包**（设计预留，下一轮实现）

### 交互系统
- 走近物品按 `F` 拾取/换枪
- 鼠标滚轮切换多个交互目标
- 武器拾取优先于道具，满两把时交换当前武器

### 物品提交点
- 随机开放/关闭，开放时走近按 `F` 一键提交背包贵重品
- 提交价值计入战绩面板的 CarryValue

### 物品生成器
- 服务端权威周期生成掉落物，Tier 递进 + 权重抽选 + 防堆叠
- 事件驱动：生成 → 等待被拾取 → 倒计时 → 下一轮

### HUD
- 准星、血条、弹药显示
- 背包面板（数据驱动，自动铺格）
- 交互提示、提交提示
- Tab 战绩面板（击杀/死亡/伤害/带出价值，可排序）

### 登录
- 基础登录界面，支持设置玩家名

## 操作说明

| 按键 | 操作 |
|---|---|
| WASD | 移动 |
| 鼠标 | 视角 |
| 左键 | 开火 |
| 右键 | 瞄准（开镜） |
| R | 换弹 |
| F | 交互（拾取/换枪/提交） |
| 鼠标滚轮 | 切换交互目标 |
| Shift | 奔跑 |
| 空格 | 跳跃 |
| E | 打开/关闭背包 |
| Tab | 战绩面板（按住查看，松开关闭） |
| 1 | 切主武器 |
| 2 | 切副武器 |

## 编译与运行

### 环境要求

- Unreal Engine 5.7
- Visual Studio 2022

### 编译

1. 右键 `tFPS_c/tFPS_c.uproject` → **Generate Visual Studio project files**
2. 打开生成的 `.sln`，选择 `Development_Editor` 编译
3. 在 UE 编辑器中打开项目

### Listen Server 联机测试

**编辑器内**：Play 按钮旁箭头 → Advanced Settings → Play Mode: **Play As Listen Server**

**独立运行**：
```
# 服务端（同时也是玩家）
tFPS_c.exe TestMap -game -Port=7777

# 客户端连接
tFPS_c.exe 127.0.0.1:7777 -game
```

### Dedicated Server（未来）
```
tFPS_cServer.exe TestMap -log
tFPS_c.exe 127.0.0.1:7777 -game
```

## 架构

```
AFPSGameMode          [仅服务端]   比赛流程控制、计时、生成玩家
AFPSGameState         [复制到所有]  比赛时间、比赛阶段同步
AFPSPlayerState       [复制到所有]  每个玩家的击杀/死亡/伤害/带出价值
AFPSPlayerController  [服务器+所属] 重生 RPC、计分板、HUD 创建
AFPSCharacter         [复制]       血量/体力、移动、死亡/复活、主副武器、背包组件、交互组件
AFPSWeapon            [复制 Actor]  武器射击、服务端权威碰撞检测、弹药系统
AFPSPickup            [复制 Actor]  地上道具拾取物
AFPSWeaponPickup      [复制 Actor]  地上武器拾取物
AFPSSubmissionPoint   [复制 Actor]  物品提交点
AFPSPickupSpawner     [仅服务端]    掉落物生成器
UFPSInventoryComponent[挂 Character] 背包组件（COND_OwnerOnly 复制）
UFPSItemDef           [DataAsset]   道具数据定义（多态子类：Heal/ChanneledHeal/HoT/Valuable/Ammo）
UFPSInteractionComponent[挂 Character] 交互管理器
```

## 项目结构

```
tFPS_c/
├── Source/tFPS_c/       C++ 源码
├── Content/
│   ├── Map/             地图
│   ├── source/Player/   角色/武器/UI 蓝图
│   └── ...
└── Config/              引擎配置
```
