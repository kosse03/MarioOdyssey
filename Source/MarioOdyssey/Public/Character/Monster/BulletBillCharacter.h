#pragma once

#include "CoreMinimal.h"
#include "Character/Monster/MonsterCharacterBase.h"
#include "BulletBillCharacter.generated.h"

struct FInputActionValue;

UCLASS()
class MARIOODYSSEY_API ABulletBillCharacter : public AMonsterCharacterBase
{
	GENERATED_BODY()

public:
	ABulletBillCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

protected:
	// =========================
	// Flight
	// =========================
	UPROPERTY(EditDefaultsOnly, Category="BulletBill|Flight")
	float FlySpeed = 400.f;

	UPROPERTY(EditDefaultsOnly, Category="BulletBill|Flight")
	float CapturedFlySpeed = 500.f;

	// 플레이어 조종 시 조향(도/초)
	UPROPERTY(EditDefaultsOnly, Category="BulletBill|Flight")
	float SteerYawRate = 220.f;

	UPROPERTY(EditDefaultsOnly, Category="BulletBill|Flight")
	float SteerPitchRate = 140.f;

	UPROPERTY(EditDefaultsOnly, Category="BulletBill|Flight")
	float PitchMin = -35.f;

	UPROPERTY(EditDefaultsOnly, Category="BulletBill|Flight")
	float PitchMax = 35.f;

	// 적 상태(미캡쳐)에서 유도 여부
	UPROPERTY(EditDefaultsOnly, Category="BulletBill|AI")
	bool bHomingToMario = true;

	UPROPERTY(EditDefaultsOnly, Category="BulletBill|AI")
	float HomingYawRate = 120.f;

	// =========================
	// Damage / Explosion
	// =========================
	UPROPERTY(EditDefaultsOnly, Category="BulletBill|Combat")
	float ImpactDamageToMario = 1.f;

	UPROPERTY(EditDefaultsOnly, Category="BulletBill|Combat")
	float ImpactDamageToMonstersWhenCaptured = 9999.f; // 일단 확실하게 죽이게 크게

	UPROPERTY(EditDefaultsOnly, Category="BulletBill|Life")
	float MaxLifeSeconds = 15.f;

	UPROPERTY(EditDefaultsOnly, Category="BulletBill|Life")
	float CapturedMaxLifeSeconds = 15.f;

	// 폭발 연출은 BP에서 처리(나이아가라/사운드)
	UFUNCTION(BlueprintImplementableEvent, Category="BulletBill|FX")
	void BP_OnExplode();

private:
	// 입력(조향)
	FVector2D SteerInput = FVector2D::ZeroVector;

	// 수명 타이머
	float LifeRemain = 0.f;

	bool bExploded = false;

	// ===== input wrappers (protected 바인딩 문제 회피용) =====
	void Input_Steer(const FInputActionValue& Value);
	void Input_LookForSteer(const FInputActionValue& Value);
	void Input_ReleaseCapture_Passthrough(const FInputActionValue& Value);
	void Input_RunStarted_Passthrough(const FInputActionValue& Value);
	void Input_RunCompleted_Passthrough(const FInputActionValue& Value);
	void Input_JumpStarted_Passthrough(const FInputActionValue& Value);
	void Input_JumpCompleted_Passthrough(const FInputActionValue& Value);

	// ===== helpers =====
	class AMarioCharacter* GetMarioViewTarget() const;
	void MoveAndHandleHit(float Dt);
	void ApplyImpactDamage(AActor* HitActor);
	void RequestReleaseIfCapturedAndExplode();
	void ExplodeInternal();
};
