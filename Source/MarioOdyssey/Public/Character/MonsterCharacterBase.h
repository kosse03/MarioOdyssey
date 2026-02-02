#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Capture/CapturableInterface.h"
#include "Components/SphereComponent.h"
#include "InputAction.h"
#include "MonsterCharacterBase.generated.h"

struct FInputActionValue;
class AMarioCharacter;
class UInputComponent;
class UInputAction;

UCLASS(Abstract)
class MARIOODYSSEY_API AMonsterCharacterBase : public ACharacter, public ICapturableInterface
{
	GENERATED_BODY()

public:
	AMonsterCharacterBase();

	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

protected:
	// =========================
	// Contact Damage (공통)
	// =========================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Monster|Combat", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USphereComponent> ContactSphere = nullptr;

	// 프로젝트 Collision Preset 이름
	UPROPERTY(EditDefaultsOnly, Category="Monster|Combat")
	FName ContactSphereProfileName = TEXT("Monster_ContactSphere");

	UPROPERTY(EditDefaultsOnly, Category="Monster|Combat")
	float ContactDamage = 1.f;

	UPROPERTY(EditDefaultsOnly, Category="Monster|Combat")
	float ContactDamageCooldown = 0.6f;

	UPROPERTY(EditDefaultsOnly, Category="Monster|Combat")
	float KnockbackStrength = 550.f;

	UPROPERTY(EditDefaultsOnly, Category="Monster|Combat")
	float KnockbackUp = 120.f;

	// 캡쳐(플레이어 조종) 중에 "컨택 공격"을 막고 싶으면 true
	UPROPERTY(EditDefaultsOnly, Category="Monster|Combat")
	bool bDisableContactDamageWhileCaptured = true;

	// Mario 기본 상태는 MarioCharacter 캡슐 Hit에서 컨택 데미지 처리(
	UPROPERTY(EditDefaultsOnly, Category="Monster|Combat")
	bool bContactDamageAffectsMario = false;

	float LastContactDamageTime = -1000.f;

	UFUNCTION()
	void OnContactBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	                          UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
	                          const FHitResult& SweepResult);

	// =========================
	// Capture-Control Input (공통)
	// =========================
	UPROPERTY(EditDefaultsOnly, Category="Monster|Input")
	TObjectPtr<UInputAction> IA_Move = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Monster|Input")
	TObjectPtr<UInputAction> IA_Look = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Monster|Input")
	TObjectPtr<const UInputAction> IA_Run = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Monster|Input")
	TObjectPtr<const UInputAction> IA_Jump = nullptr;

	// 정책: C키(IA_Crouch)를 캡쳐 해제로 재사용
	UPROPERTY(EditDefaultsOnly, Category="Monster|Input")
	TObjectPtr<UInputAction> IA_Crouch = nullptr;

	// 캡쳐 중 이동 파라미터 (각 몬스터 BP에서 조절)
	UPROPERTY(EditDefaultsOnly, Category="Monster|Move")
	float CapturedWalkSpeed = 200.f;

	UPROPERTY(EditDefaultsOnly, Category="Monster|Move")
	float CapturedRunSpeed = 500.f;

	UPROPERTY(EditDefaultsOnly, Category="Monster|Move")
	float CapturedJumpZVelocity = 520.f;

	// =========================
	// Capture State (공통)
	// =========================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Monster|Capture")
	bool bIsCaptured = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Monster|Capture")
	bool bInputLocked = false;

	bool bRunHeld = false;

	float DefaultMaxWalkSpeed = 0.f;
	float DefaultJumpZVelocity = 0.f;

	TWeakObjectPtr<AMarioCharacter> CapturingMario;

	void ApplyCapturedMoveParams();
	void StopAIMove();

	// Input handlers
	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_RunStarted(const FInputActionValue& Value);
	void Input_RunCompleted(const FInputActionValue& Value);
	void Input_JumpStarted(const FInputActionValue& Value);
	void Input_JumpCompleted(const FInputActionValue& Value);

	UFUNCTION()
	void OnReleaseCapturePressed(const FInputActionValue& Value);

	// =========================
	// Capturable Interface (공통)
	// =========================
	UPROPERTY(EditDefaultsOnly, Category="Monster|Capture")
	bool bCapturable = false;

	virtual bool CanBeCaptured_Implementation(const FCaptureContext& Context) override { return bCapturable; }
	virtual APawn* GetCapturePawn_Implementation() override { return this; }
	virtual void OnCaptured_Implementation(AController* Capturer, const FCaptureContext& Context) override;
	virtual void OnReleased_Implementation(const FCaptureReleaseContext& Context) override;
	virtual void OnCapturedPawnDamaged_Implementation(float Damage, AController* InstigatorController, AActor* DamageCauser) override;

	// 캡쳐 중 피격 리액션 기본값 (각 몬스터별로 BP/파생클래스에서 조절)
	UPROPERTY(EditDefaultsOnly, Category="Monster|Capture")
	float CapturedHitStunSeconds = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category="Monster|Capture")
	float CapturedHitKnockbackStrength = 450.f;

	UPROPERTY(EditDefaultsOnly, Category="Monster|Capture")
	float CapturedHitKnockbackUp = 140.f;

	FTimerHandle CapturedHitStunTimer;

	void ClearCapturedHitStun();

	// 파생 클래스에서 추가 동작 필요하면 여기 오버라이드
	virtual void OnCapturedExtra(AController* Capturer, const FCaptureContext& Context) {}
	virtual void OnReleasedExtra(const FCaptureReleaseContext& Context) {}
	virtual void OnCapturedPawnDamagedExtra(float Damage, AController* InstigatorController, AActor* DamageCauser) {}
};