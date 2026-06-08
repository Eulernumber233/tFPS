#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Engine/Texture2D.h"
#include "FPSPlayerState.generated.h"

UCLASS()
class TFPS_C_API AFPSPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Add kills to this player's score (server only) */
	UFUNCTION(BlueprintCallable, Category = "Score")
	void AddKill(int32 Amount = 1);

	/** Add deaths to this player's stats (server only) */
	UFUNCTION(BlueprintCallable, Category = "Score")
	void AddDeath(int32 Amount = 1);

	/**
	 * 累加该玩家造成的有效伤害（服务端权威）。由 AFPSCharacter::TakeDamage 在扣血后
	 * 对攻击者的 PlayerState 调用，传入服务端权威 ActualDamage。
	 */
	UFUNCTION(BlueprintCallable, Category = "Score")
	void AddDamage(float Amount);

	/**
	 * 累加该玩家成功带出的物品价值（服务端权威，预留）。未来随机开放带出点、
	 * 玩家在带出点带出物品时调用累加。当前无人调用，恒为 0。
	 */
	UFUNCTION(BlueprintCallable, Category = "Score")
	void AddCarryValue(float Amount);

	/** Called on clients when Kills value replicates */
	UFUNCTION()
	void OnRep_Kills();

	/** Called on clients when Deaths value replicates */
	UFUNCTION()
	void OnRep_Deaths();

	/** Called on clients when TotalDamage value replicates */
	UFUNCTION()
	void OnRep_TotalDamage();

	/** Called on clients when CarryValue value replicates */
	UFUNCTION()
	void OnRep_CarryValue();

	UPROPERTY(ReplicatedUsing = OnRep_Kills, BlueprintReadOnly, Category = "Score")
	int32 Kills = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Deaths, BlueprintReadOnly, Category = "Score")
	int32 Deaths = 0;

	/** 造成的有效伤害累加（服务端权威，复制到所有端给计分板显示）。 */
	UPROPERTY(ReplicatedUsing = OnRep_TotalDamage, BlueprintReadOnly, Category = "Score")
	float TotalDamage = 0.0f;

	/**
	 * 带出物品总价值（预留字段，服务端权威，复制）。未来玩法：游戏中不定期随机开放
	 * 带出点，玩家在带出点带出可拾取 Actor 时按其价值累加。当前恒为 0。
	 */
	UPROPERTY(ReplicatedUsing = OnRep_CarryValue, BlueprintReadOnly, Category = "Score")
	float CarryValue = 0.0f;

	/**
	 * 玩家头像（预留字段，复制）。未来登录界面设置头像后从那里赋值。
	 * 用 TSoftObjectPtr 避免计分板未打开时也强引用加载所有玩家头像。当前为空。
	 */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Score")
	TSoftObjectPtr<UTexture2D> PlayerIcon;
};
