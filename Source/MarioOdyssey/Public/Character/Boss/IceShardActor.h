#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IceShardActor.generated.h"

UENUM(BlueprintType)
enum class EIceShardMode : uint8
{
	RainTile UMETA(DisplayName="RainTile"),
	CaptureBarrage UMETA(DisplayName="CaptureBarrage")
};

UCLASS()
class MARIOODYSSEY_API AIceShardActor : public AActor
{
	GENERATED_BODY()

public:
	AIceShardActor();

	// 기본 얼음비 샤드(바닥 충돌 시 타일 생성)
	void InitShard(AActor* InOwnerBoss, float InDamage, TSubclassOf<class AIceTileActor> InIceTileClass, float InIceTileZOffset);

	// 캡쳐 카운터 샤드(플레이어 데미지 없음, 캡쳐된 주먹 타격 시 카운트)
	void InitBarrageShard(AActor* InOwnerBoss, class AAttrenashinFist* InCapturedFist, const FVector& InVelocity);

protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root = nullptr;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<class USphereComponent> Sphere = nullptr;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<class UStaticMeshComponent> Mesh = nullptr;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<class UProjectileMovementComponent> ProjectileMove = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="IceShard")
	float LifeSeconds = 6.0f;

	UPROPERTY(EditDefaultsOnly, Category="IceShard")
	float InitialDownSpeed = 100.f;

	UPROPERTY(EditDefaultsOnly, Category="IceShard")
	float GravityScale = 1.0f;

private:
	TWeakObjectPtr<AActor> OwnerBoss;
	TWeakObjectPtr<class AAttrenashinFist> BarrageCapturedFist;

	float Damage = 1.f;
	TSubclassOf<class AIceTileActor> IceTileClass;
	float IceTileZOffset = 0.f;

	EIceShardMode Mode = EIceShardMode::RainTile;
	bool bDamageMarioEnabled = true;
	bool bSpawnTileOnStop = true;

	bool bDamagedMario = false;
	bool bStopped = false;

	UFUNCTION()
	void OnBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	                    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	                    bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnProjectileStop(const FHitResult& ImpactResult);
};
