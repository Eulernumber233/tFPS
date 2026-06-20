#include "FPSPickupSpawner.h"
#include "FPSPickup.h"
#include "FPSItemDef.h"
#include "Components/BillboardComponent.h"
#include "Engine/World.h"
#include "WorldCollision.h"
#include "Engine/OverlapResult.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

// ============================================================================
//  Construction
// ============================================================================

AFPSPickupSpawner::AFPSPickupSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;   // Spawner 自身不复制——它只在服务端运行，生成带复制的 AFPSPickup

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Billboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
	Billboard->SetupAttachment(Root);
	Billboard->SetHiddenInGame(true);
	Billboard->SetRelativeScale3D(FVector(0.5f));

	// 尝试加载拾取物风格精灵图（路径不存在时保持默认，不影响运行）
	static ConstructorHelpers::FObjectFinder<UTexture2D> SpriteFinder(
		TEXT("/Engine/EditorResources/S_TargetPoint.S_TargetPoint"));
	if (SpriteFinder.Succeeded())
	{
		Billboard->SetSprite(SpriteFinder.Object);
	}
}

// ============================================================================
//  BeginPlay
// ============================================================================

void AFPSPickupSpawner::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && bAutoStart)
	{
		StartSpawning();
	}
}

// ============================================================================
//  生成循环控制
// ============================================================================

void AFPSPickupSpawner::StartSpawning()
{
	if (!HasAuthority() || bSpawning)
		return;

	bSpawning = true;
	SpawnStartWorldTime = GetWorld()->GetTimeSeconds();

	const float FirstDelay = (InitialSpawnDelay >= 0.0f)
		? InitialSpawnDelay
		: BaseSpawnInterval + FMath::FRandRange(-SpawnIntervalRandomDeviation, SpawnIntervalRandomDeviation);

	GetWorld()->GetTimerManager().SetTimer(
		SpawnTimerHandle,
		this,
		&AFPSPickupSpawner::TrySpawn,
		FMath::Max(FirstDelay, 0.1f),
		false);
}

void AFPSPickupSpawner::StopSpawning()
{
	if (SpawnTimerHandle.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(SpawnTimerHandle);
	}
	bSpawning = false;

	// 解绑旧 Pickup 的销毁委托，避免悬空回调
	if (CurrentSpawnedPickup.IsValid())
	{
		CurrentSpawnedPickup->OnDestroyed.RemoveDynamic(this, &AFPSPickupSpawner::OnSpawnedPickupDestroyed);
		CurrentSpawnedPickup.Reset();
	}
}

void AFPSPickupSpawner::ScheduleNextSpawn()
{
	if (!bSpawning)
		return;

	const float Interval = FMath::Max(
		BaseSpawnInterval + FMath::FRandRange(-SpawnIntervalRandomDeviation, SpawnIntervalRandomDeviation),
		1.0f);   // 下限 1 秒防止高频生成

	GetWorld()->GetTimerManager().SetTimer(
		SpawnTimerHandle,
		this,
		&AFPSPickupSpawner::TrySpawn,
		Interval,
		false);
}

void AFPSPickupSpawner::ForceSpawn()
{
	if (!HasAuthority())
		return;

	TrySpawn();
}

void AFPSPickupSpawner::OnSpawnedPickupDestroyed(AActor* DestroyedActor)
{
	// 仅当销毁的是我们跟踪的 Pickup 时，才触发后续流程
	if (DestroyedActor != CurrentSpawnedPickup.Get())
		return;

	CurrentSpawnedPickup.Reset();

	// 通知蓝图：进入等待生成状态（倒计时开始）
	OnWaitingForRespawn();

	// 安排下一次生成
	ScheduleNextSpawn();
}

// ============================================================================
//  单次生成
// ============================================================================

void AFPSPickupSpawner::TrySpawn()
{
	if (!HasAuthority())
		return;

	// —— 已有 Pickup 未被拾取，等待其销毁后再生成 ——
	if (CurrentSpawnedPickup.IsValid())
	{
		// 不安排定时器：等 OnSpawnedPickupDestroyed 回调触发后再 ScheduleNextSpawn
		return;
	}

	// 清理可能残留的无效弱引用
	CurrentSpawnedPickup.Reset();

	// —— 防堆叠：当前位置已有其他 Pickup 则跳过本周期 ——
	if (IsPickupAlreadyAtSpawnPoint())
	{
		ScheduleNextSpawn();
		return;
	}

	// —— 从生成表抽选 ——
	UFPSItemDef* SelectedItem = SelectItemToSpawn();
	if (!SelectedItem)
	{
		ScheduleNextSpawn();
		return;
	}

	// —— 找到对应条目以获得 Pickup 蓝图 ——
	TSubclassOf<AFPSPickup> PickupClass = DefaultPickupClass;
	for (const FSpawnTableEntry& Entry : SpawnTable)
	{
		if (Entry.ItemDef == SelectedItem)
		{
			PickupClass = GetPickupClassForEntry(Entry);
			break;
		}
	}

	// 最后兜底：ItemDef 自身的 DropPickupClass
	if (!PickupClass)
	{
		PickupClass = SelectedItem->DropPickupClass;
	}

	if (!PickupClass)
	{
		ScheduleNextSpawn();
		return;
	}

	// —— 延迟生成：先设 ItemDef，再 FinishSpawning 触发 Construction Script ——
	const FVector SpawnLoc = GetActorLocation();
	const FRotator SpawnRot = GetActorRotation();

	AFPSPickup* Pickup = GetWorld()->SpawnActorDeferred<AFPSPickup>(
		PickupClass,
		FTransform(SpawnRot, SpawnLoc));
	if (!Pickup)
	{
		ScheduleNextSpawn();
		return;
	}

	// 关键：Construction Script 运行前覆写 ItemDef，蓝图才能读到正确的 PickupMesh/DisplayName
	Pickup->ItemDef = SelectedItem;

	Pickup->FinishSpawning(FTransform(SpawnRot, SpawnLoc));

	// —— 跟踪 + 绑定销毁事件 ——
	CurrentSpawnedPickup = Pickup;
	Pickup->OnDestroyed.AddDynamic(this, &AFPSPickupSpawner::OnSpawnedPickupDestroyed);

	// —— 通知蓝图：生成成功 ——
	OnSpawnSucceeded(Pickup);

	// 不安排下一轮 timer：等待 OnSpawnedPickupDestroyed 回调触发后再 ScheduleNextSpawn
}

// ============================================================================
//  抽选逻辑
// ============================================================================

UFPSItemDef* AFPSPickupSpawner::SelectItemToSpawn() const
{
	if (SpawnTable.Num() == 0)
		return nullptr;

	const int32 CurrentMaxTier = GetCurrentMaxTier();

	// 收集候选：(ItemDef, EffectiveWeight)
	TArray<TPair<UFPSItemDef*, float>> Candidates;

	for (const FSpawnTableEntry& Entry : SpawnTable)
	{
		if (!Entry.ItemDef)
			continue;
		if (Entry.ItemDef->Tier > CurrentMaxTier)
			continue;
		if (Entry.BaseWeight <= 0.0f)
			continue;

		float EffectiveWeight = Entry.BaseWeight;

		// 偏向：匹配 BiasItemClass 的条目权重 * BiasDegree
		if (BiasItemClass && Entry.ItemDef->IsA(BiasItemClass))
		{
			EffectiveWeight *= BiasDegree;
		}

		Candidates.Add({ Entry.ItemDef, EffectiveWeight });
	}

	if (Candidates.Num() == 0)
		return nullptr;

	// 计算总权重
	float TotalWeight = 0.0f;
	for (const auto& Pair : Candidates)
	{
		TotalWeight += Pair.Value;
	}

	if (TotalWeight <= 0.0f)
		return nullptr;

	// 按权重随机抽选
	const float Random = FMath::FRand() * TotalWeight;
	float Accum = 0.0f;
	for (const auto& Pair : Candidates)
	{
		Accum += Pair.Value;
		if (Random <= Accum)
		{
			return Pair.Key;
		}
	}

	// 浮点精度兜底
	return Candidates.Last().Key;
}

// ============================================================================
//  Tier 递进
// ============================================================================

int32 AFPSPickupSpawner::GetCurrentMaxTier() const
{
	if (TierRampTime <= 0.0f || !GetWorld())
		return MaxSpawnTier;

	const float Elapsed = GetWorld()->GetTimeSeconds() - SpawnStartWorldTime;
	const float Ratio = FMath::Clamp(Elapsed / TierRampTime, 0.0f, 1.0f);
	const float TierFloat = FMath::Lerp(
		static_cast<float>(BaseSpawnTier),
		static_cast<float>(MaxSpawnTier),
		Ratio);

	return FMath::FloorToInt(TierFloat);
}

// ============================================================================
//  防堆叠检查
// ============================================================================

bool AFPSPickupSpawner::IsPickupAlreadyAtSpawnPoint() const
{
	const FCollisionShape Sphere = FCollisionShape::MakeSphere(SpawnCheckRadius);

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(
		Overlaps,
		GetActorLocation(),
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_WorldDynamic),   // Pickup 的 PickupSphere 是 WorldDynamic
		Sphere,
		QueryParams);

	for (const FOverlapResult& Result : Overlaps)
	{
		if (Cast<AFPSPickup>(Result.GetActor()))
			return true;
	}

	return false;
}

// ============================================================================
//  Pickup 蓝图解析
// ============================================================================

TSubclassOf<AFPSPickup> AFPSPickupSpawner::GetPickupClassForEntry(const FSpawnTableEntry& Entry) const
{
	if (Entry.OverridePickupClass)
		return Entry.OverridePickupClass;

	if (DefaultPickupClass)
		return DefaultPickupClass;

	if (Entry.ItemDef && Entry.ItemDef->DropPickupClass)
		return Entry.ItemDef->DropPickupClass;

	return nullptr;
}

// ============================================================================
//  编辑器：Billboard 缩放跟随 SpawnCheckRadius
// ============================================================================

#if WITH_EDITOR
void AFPSPickupSpawner::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!PropertyChangedEvent.Property || !Billboard)
		return;

	const FName PropName = PropertyChangedEvent.Property->GetFName();
	if (PropName == GET_MEMBER_NAME_CHECKED(AFPSPickupSpawner, SpawnCheckRadius))
	{
		const float Scale = FMath::Max(SpawnCheckRadius / 64.0f, 0.2f);
		Billboard->SetRelativeScale3D(FVector(Scale));
	}
}
#endif
