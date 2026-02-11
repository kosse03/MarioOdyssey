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

	UFUNCTION(BlueprintCallable, Category="Boss|IceShard")
	void StartIceRainByBothFists();

	// Fist 콜백(캡쳐 시작/해제)
	void NotifyFistCaptured(class AAttrenashinFist* CapturedFist);
	void NotifyFistReleased(class AAttrenashinFist* ReleasedFist);

	// 카운터 샤드가 캡쳐된 주먹에 적중했을 때
	void NotifyBarrageShardHitCapturedFist(class AAttrenashinFist* HitFist, const FVector& ShardVelocity);

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

	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase1", meta=(ClampMin="0.1"))
	float Phase1CaptureWindowSeconds = 8.0f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase1", meta=(ClampMin="0.0"))
	float Phase1PostFailCooldownSeconds = 0.8f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase1|Retreat", meta=(ClampMin="0.0"))
	float Phase1RetreatDistance = 2000.f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase1|Retreat", meta=(ClampMin="0.01"))
	float Phase1RetreatSeconds = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Head")
	bool bRequireCapturedFistForHeadHit = true;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Head")
	float HeadContactDamage = 1.f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Head")
	float HeadContactDamageCooldown = 0.4f;

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

	// 캡쳐 중 반대손 카운터 샤드
	UPROPERTY(EditDefaultsOnly, Category="Boss|CaptureBarrage", meta=(ClampMin="0.1"))
	float CaptureBarrageIntervalSeconds = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|CaptureBarrage", meta=(ClampMin="1.0"))
	float CaptureBarrageShardSpeed = 2600.f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|CaptureBarrage", meta=(ClampMin="1"))
	int32 CaptureBarrageHitsToForceRelease = 3;

	UPROPERTY(EditDefaultsOnly, Category="Boss|CaptureBarrage")
	float CaptureBarrageSpawnForwardOffset = 0.f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|CaptureBarrage", meta=(ClampMin="0.0"))
	float CaptureBarrageKnockbackStrength = 320.f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|CaptureBarrage", meta=(ClampMin="0.0"))
	float CaptureBarrageKnockbackUp = 90.f;


private:
	EAttrenashinPhase Phase = EAttrenashinPhase::Phase1;

	enum class EPhase1CaptureResult : uint8
	{
		None,
		Success,
		Fail
	};

	TWeakObjectPtr<class AAttrenashinFist> LeftFist;
	TWeakObjectPtr<class AAttrenashinFist> RightFist;

	float SlamAttemptTimer = 0.f;
	bool bNextLeft = true;
	int32 HeadHitCount = 0;
	float LastHeadContactDamageTime = -1000.f;

	FTimerHandle Phase1CaptureWindowTimerHandle;
	bool bPhase1CaptureWindowActive = false;
	EPhase1CaptureResult Phase1CaptureResult = EPhase1CaptureResult::None;
	float Phase1FailCooldownRemain = 0.f;

	bool bRetreating = false;
	FVector RetreatStart = FVector::ZeroVector;
	FVector RetreatTarget = FVector::ZeroVector;
	float RetreatT = 0.f;

	// 캡쳐 카운터 샤드 상태
	FTimerHandle CaptureBarrageTimerHandle;
	TWeakObjectPtr<class AAttrenashinFist> BarrageCapturedFist;
	TWeakObjectPtr<class AAttrenashinFist> BarrageThrowingFist;
	int32 BarrageHitCount = 0;
	bool bCaptureBarrageActive = false;

	UFUNCTION()
	void OnHeadBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	                        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	                        bool bFromSweep, const FHitResult& SweepResult);

	AActor* GetPlayerTarget() const;
	void TryStartPhase1Slam();
	void SpawnIceShardsAt(const FVector& Center);

	void EnterPhase(EAttrenashinPhase NewPhase);
	void BeginPhase1CaptureWindow(class AAttrenashinFist* CapturedFist);
	void EndPhase1CaptureWindow(bool bSuccess);
	void OnPhase1CaptureWindowTimeout();
	void StartRetreatMotion();
	void UpdateRetreat(float DeltaSeconds);

	void StartCaptureBarrage(class AAttrenashinFist* CapturedFist);
	void StopCaptureBarrage();
	void HandleCaptureBarrageTick();

	FVector GetAnchorLocationBySide(EFistSide Side) const;
};
