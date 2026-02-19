#pragma once

#include "CoreMinimal.h"
#include "Character/Monster/MonsterCharacterBase.h"
#include "VolteDaCharacter.generated.h"

struct FInputActionValue;
class AController;

UCLASS()
class MARIOODYSSEY_API AVolteDaCharacter : public AMonsterCharacterBase
{
	GENERATED_BODY()

public:
	AVolteDaCharacter();

	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

protected:

virtual void Tick(float DeltaSeconds) override;

// ===== Reveal behavior =====
// If true: reveal invisible blocks only while walking on ground (not running, not falling).
UPROPERTY(EditAnywhere, Category="VolteDa|Reveal")
bool bRevealOnlyWhileWalking = true;

// ===== Simple flee AI (when NOT captured) =====
UPROPERTY(EditAnywhere, Category="VolteDa|FleeAI")
bool bEnableFleeAI = true;

UPROPERTY(EditAnywhere, Category="VolteDa|FleeAI", meta=(ClampMin="0.0"))
float FleeDetectDistance = 1400.0f;

// Dot threshold for "player is in front" check (0~1). Higher = narrower cone.
UPROPERTY(EditAnywhere, Category="VolteDa|FleeAI", meta=(ClampMin="-1.0", ClampMax="1.0"))
float FleeSeeDotThreshold = 0.45f;

// Keep fleeing for a short duration once triggered (prevents jitter).
UPROPERTY(EditAnywhere, Category="VolteDa|FleeAI", meta=(ClampMin="0.0"))
float FleePersistSeconds = 0.6f;

	// Capture hooks
	virtual void OnCapturedExtra(AController* Capturer, const FCaptureContext& Context) override;
	virtual void OnReleasedExtra(const FCaptureReleaseContext& Context) override;

	// 선글라스 ON일 때 느린 속도, OFF일 때 빠른 속도
	UPROPERTY(EditDefaultsOnly, Category="VolteDa|Move")
	float CapturedSpeed_GlassesOn = 230.f;

	UPROPERTY(EditDefaultsOnly, Category="VolteDa|Move")
	float CapturedSpeed_GlassesOff = 520.f;

	// 숨김 대상 태그 (액터 태그)
	UPROPERTY(EditDefaultsOnly, Category="VolteDa|Reveal")
	FName RevealActorTag = TEXT("MoeEyeHidden");

private:
    float FleeRemain = 0.f;

	// ===== reveal cache =====
	TArray<TWeakObjectPtr<AActor>> RevealActors;
	bool bGlassesOn = true;
	bool bRevealVisibleApplied = false; // 마지막으로 적용된 "보임" 상태 캐시

	// ===== input wrappers (protected 바인딩/접근 문제 회피) =====
	void Input_RunStarted_Proxy(const FInputActionValue& Value);
	void Input_RunCompleted_Proxy(const FInputActionValue& Value);
	void Input_ReleaseCapture_Passthrough(const FInputActionValue& Value);

	// ===== core =====
	void CacheRevealActors();
	void ApplyRevealVisibility(bool bVisible);
	void SetGlassesOn(bool bOn);
	void ApplyCapturedSpeed();
};
