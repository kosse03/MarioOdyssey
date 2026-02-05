#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "Character/Boss/AttrenashinTypes.h"
#include "AttrenashinBoss.generated.h"

UCLASS()
class MARIOODYSSEY_API AAttrenashinBoss : public AActor
{
	GENERATED_BODY()

public:
	AAttrenashinBoss();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category="Boss|Anim")
	EAttrenashinPhase GetPhase() const { return Phase; }

	UFUNCTION(BlueprintPure, Category="Boss|Anim")
	int32 GetHeadHitCount() const { return HeadHitCount; }

	UFUNCTION(BlueprintPure, Category="Boss|Anim")
	class AAttrenashinFist* GetLeftFist() const { return LeftFist.Get(); }

	UFUNCTION(BlueprintPure, Category="Boss|Anim")
	class AAttrenashinFist* GetRightFist() const { return RightFist.Get(); }

	UFUNCTION(BlueprintCallable, Category="Boss|IceShard")
	void PerformGroundSlam_IceShardRain();

	UFUNCTION(BlueprintCallable, Category="Boss|IceShard")
	void StartIceRainByFist(bool bUseLeftFist);

protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root = nullptr;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> FistAnchorL = nullptr;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> FistAnchorR = nullptr;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<class USphereComponent> HeadHitSphere = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Spawn")
	TSubclassOf<class AAttrenashinFist> FistClass;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase1")
	float SlamAttemptInterval = 6.0f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Head")
	bool bRequireCapturedFistForHeadHit = true;

	UPROPERTY(EditDefaultsOnly, Category="Boss|IceShard")
	TSubclassOf<class AIceShardActor> IceShardClass;

	UPROPERTY(EditDefaultsOnly, Category="Boss|IceShard")
	TSubclassOf<class AIceTileActor> IceTileClass;

	UPROPERTY(EditDefaultsOnly, Category="Boss|IceShard", meta=(ClampMin="1"))
	int32 IceShardCount = 12;

	UPROPERTY(EditDefaultsOnly, Category="Boss|IceShard", meta=(ClampMin="0.0"))
	float IceShardRadius = 1000.f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|IceShard")
	float IceShardSpawnHeight = 900.f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|IceShard")
	float IceShardSpawnHeightJitter = 220.f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|IceShard")
	float IceShardDamage = 1.f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|IceShard")
	bool bIceShardCenterOnPlayer = true;

	UPROPERTY(EditDefaultsOnly, Category="Boss|IceShard")
	float IceTileSpawnZOffset = 0.f;

private:
	EAttrenashinPhase Phase = EAttrenashinPhase::Phase1;

	TWeakObjectPtr<class AAttrenashinFist> LeftFist;
	TWeakObjectPtr<class AAttrenashinFist> RightFist;

	float SlamAttemptTimer = 0.f;
	bool bNextLeft = true;
	int32 HeadHitCount = 0;

	UFUNCTION()
	void OnHeadBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	                        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	                        bool bFromSweep, const FHitResult& SweepResult);

	AActor* GetPlayerTarget() const;
	void TryStartPhase1Slam();
	void SpawnIceShardsAt(const FVector& Center);
};
