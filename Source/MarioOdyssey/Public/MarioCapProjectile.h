#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MarioCapProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class URotatingMovementComponent;

UENUM()
enum class ECapPhase : uint8
{
	Outgoing,
	Hover,
	Returning
};

UCLASS()
class MARIOODYSSEY_API AMarioCapProjectile : public AActor
{
	GENERATED_BODY()

public:
	AMarioCapProjectile();

	void FireInDirection(const FVector& Dir, float Speed);

	// 홀드 종료
	void NotifyHoldReleased();

protected:
	virtual void BeginPlay() override;
	
	bool TryCaptureTarget(AActor* TargetActor, UPrimitiveComponent* TargetComp, const FHitResult* OptionalHit);
	
	bool IsCapturableTarget(AActor* TargetActor, UPrimitiveComponent* TargetComp) const;

	UFUNCTION()
	void OnCapHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);
	UFUNCTION()
	void OnCapBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void OnProjectileStopped(const FHitResult& ImpactResult);

	void EnterHover();
	void BeginReturn();
	void OnMinHoverReached();
	void OnMaxHoverReached();

	UPROPERTY(VisibleAnywhere, Category="Cap")
	USphereComponent* Collision;

	UPROPERTY(VisibleAnywhere, Category="Cap")
	UStaticMeshComponent* CapMesh;

	UPROPERTY(VisibleAnywhere, Category="Cap")
	UProjectileMovementComponent* ProjectileMove;

	UPROPERTY(VisibleAnywhere, Category="Cap")
	URotatingMovementComponent* RotMove;
	
	UPROPERTY(EditDefaultsOnly, Category="Cap|Combat")
	float FailedCaptureDamage = 1.f;
	// ---- 타이밍 ----
	UPROPERTY(EditDefaultsOnly, Category="Cap|Timing")
	float OutgoingDuration = 0.2f;   // 0.1초 빠르게 전진

	UPROPERTY(EditDefaultsOnly, Category="Cap|Timing")
	float MinHoverTime = 0.5f;       // 기본 체공 0.5초(짧게 눌러도 최소)

	UPROPERTY(EditDefaultsOnly, Category="Cap|Timing")
	float MaxHoverTime = 3.0f;       // 홀드 시 최대 체공 3초

	// ---- 귀환 ----
	UPROPERTY(EditDefaultsOnly, Category="Cap|Return")
	float ReturnMaxSpeed = 2200.f;
	
	UPROPERTY(EditDefaultsOnly, Category="Cap|Return")
	float ReturnSpeed = 2600.f; // Returning에서 Tick 이동 속도
	
	UPROPERTY(EditDefaultsOnly, Category="Cap|Return")
	float HomingAccel = 8000.f;

	UPROPERTY(EditDefaultsOnly, Category="Cap|Return")
	float CatchDistance = 50.f;

	UPROPERTY(EditDefaultsOnly, Category="Cap|Return")
	float ReturnUnstickDist = 8.f;

	// 마지막으로 벽에 맞았을 때 노멀(벽에서 떼는 용도)
	FVector LastBlockNormal = FVector::ZeroVector;
	bool bHasLastBlockNormal = false;
	// 런타임 상태
	ECapPhase Phase = ECapPhase::Outgoing;

	bool bHoldReleased = false;     // 키를 뗐는가?
	bool bMinHoverPassed = false;   // 최소 체공(0.5s) 지났는가?

	FTimerHandle OutgoingTimer;
	FTimerHandle MinHoverTimer;
	FTimerHandle MaxHoverTimer;

	TWeakObjectPtr<AActor> OwnerActor;

public:
	virtual void Tick(float DeltaTime) override;
};
