#pragma once

#include "CoreMinimal.h"
#include "Character/Monster/MonsterCharacterBase.h"
#include "BulletBillCharacter.generated.h"

/**
 * Bullet Bill (Killer) - captureable flying projectile-like monster.
 * - Moves forward continuously (AddActorWorldOffset sweep)
 * - When NOT captured: homes to Mario on Yaw only (no pitch)
 * - When captured: player can steer Yaw left/right only (no pitch)
 * - Gravity disabled (Flying mode + GravityScale=0)
 */
UCLASS()
class MARIOODYSSEY_API ABulletBillCharacter : public AMonsterCharacterBase
{
	GENERATED_BODY()

public:
	ABulletBillCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;

	// override to bind Enhanced Input without base AddMovementInput bindings
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/** Launcher helper: temporarily disable collision right after spawn to prevent instant impact/explosion. */
	UFUNCTION(BlueprintCallable, Category="BulletBill|Spawn")
	void StartSpawnGrace(float GraceSeconds = -1.f);

protected:
	// ===== movement =====
	UPROPERTY(EditAnywhere, Category="BulletBill|Move")
	float FlySpeed = 1600.f;

	UPROPERTY(EditAnywhere, Category="BulletBill|Move")
	float CapturedFlySpeed = 1700.f;

	// ===== steering (capture) =====
	UPROPERTY(EditAnywhere, Category="BulletBill|Steer")
	float SteerYawRate = 160.f; // deg/sec

	// ===== homing (enemy) =====
	UPROPERTY(EditAnywhere, Category="BulletBill|Homing")
	bool bHomingToMario = true;

	UPROPERTY(EditAnywhere, Category="BulletBill|Homing")
	float HomingYawRate = 120.f; // deg/sec

	// ===== lifetime =====
	UPROPERTY(EditAnywhere, Category="BulletBill|Life")
	float MaxLifeSeconds = 12.f;

	// ===== damage =====
	UPROPERTY(EditAnywhere, Category="BulletBill|Damage")
	float ImpactDamageToMario = 1.f;

	UPROPERTY(EditAnywhere, Category="BulletBill|Damage")
	float ImpactDamageToMonstersWhenCaptured = 1.f;

	// ===== spawn grace =====
	UPROPERTY(EditAnywhere, Category="BulletBill|Spawn")
	float DefaultSpawnGraceSeconds = 0.15f;

protected:
	// input
	void Input_Steer(const struct FInputActionValue& Value);
	// 캡쳐 중 카메라 회전(마우스 룩). ViewTarget(Mario)로 Look 입력을 전달해 TargetControlRotation을 갱신한다.
	void Input_LookCamera(const struct FInputActionValue& Value);

	// passthroughs
	void Input_ReleaseCapture_Passthrough(const struct FInputActionValue& Value);
	void Input_RunStarted_Passthrough(const struct FInputActionValue& Value);
	void Input_RunCompleted_Passthrough(const struct FInputActionValue& Value);
	void Input_JumpStarted_Passthrough(const struct FInputActionValue& Value);
	void Input_JumpCompleted_Passthrough(const struct FInputActionValue& Value);

	class AMarioCharacter* GetMarioViewTarget() const;

	void MoveAndHandleHit(float Dt);
	void ApplyImpactDamage(AActor* HitActor);

	void RequestReleaseIfCapturedAndExplode();
	void ExplodeInternal();

	UFUNCTION(BlueprintImplementableEvent, Category="BulletBill|FX")
	void BP_OnExplode();

	// capture hooks
	virtual void OnCapturedExtra(AController* Capturer, const struct FCaptureContext& Context) override;
	virtual void OnReleasedExtra(const struct FCaptureReleaseContext& Context) override;

private:
	// state
	FVector2D SteerInput = FVector2D::ZeroVector;

	float LifeRemain = 0.f;
	bool  bExploded = false;

	// spawn grace
	bool bSpawnGraceActive = false;
	FTimerHandle SpawnGraceTimer;

	void EndSpawnGrace();
};
