#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IceShardActor.generated.h"

UCLASS()
class MARIOODYSSEY_API AIceShardActor : public AActor
{
	GENERATED_BODY()

public:
	AIceShardActor();

	// Boss가 스폰 직후 설정
	void InitShard(AActor* InOwnerBoss, float InDamage, TSubclassOf<class AIceTileActor> InIceTileClass, float InIceTileZOffset);

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
	float InitialDownSpeed = 1200.f;

	UPROPERTY(EditDefaultsOnly, Category="IceShard")
	float GravityScale = 1.0f;

private:
	TWeakObjectPtr<AActor> OwnerBoss;
	float Damage = 1.f;

	TSubclassOf<class AIceTileActor> IceTileClass;
	float IceTileZOffset = 0.f;

	bool bDamagedMario = false;
	bool bStopped = false;

	UFUNCTION()
	void OnBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	                    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	                    bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnProjectileStop(const FHitResult& ImpactResult);
};
