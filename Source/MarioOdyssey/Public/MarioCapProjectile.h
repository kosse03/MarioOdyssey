#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MarioCapProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class URotatingMovementComponent;

UCLASS()
class MARIOODYSSEY_API AMarioCapProjectile : public AActor
{
	GENERATED_BODY()
	
public:	
	AMarioCapProjectile();
	
	void FireInDirection(const FVector& Dir, float Speed);
protected:
	virtual void BeginPlay() override;
	
	UFUNCTION()
	void OnCapHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
	
	void StartReturn();
	
	UFUNCTION()
	void OnProjectileStopped(const FHitResult& ImpactResult);
	
	UPROPERTY(VisibleAnywhere, Category="Cap")
	USphereComponent* Collision;
	
	UPROPERTY(VisibleAnywhere, Category="Cap")
	UStaticMeshComponent* CapMesh;
	
	UPROPERTY(VisibleAnywhere, Category="Cap")
	UProjectileMovementComponent* ProjectileMove;
	
	UPROPERTY(VisibleAnywhere, Category="Cap")
	URotatingMovementComponent* RotMove;
	
	// Return 설정 
	UPROPERTY(EditDefaultsOnly, Category="Cap|Return")
	float OutgoingTime = 0.5f;          // 앞으로 날아가는 시간

	UPROPERTY(EditDefaultsOnly, Category="Cap|Return")
	float ReturnMaxSpeed = 2200.f;       // 돌아올 때 최대 속도

	UPROPERTY(EditDefaultsOnly, Category="Cap|Return")
	float HomingAccel = 8000.f;         // 귀환 가속

	UPROPERTY(EditDefaultsOnly, Category="Cap|Return")
	float CatchDistance = 30.f;          // 이 거리 안이면 "잡았다" 처리

	FTimerHandle ReturnTimer;
	bool bReturning = false;

	TWeakObjectPtr<AActor> OwnerActor;
public:	
	virtual void Tick(float DeltaTime) override;

};
