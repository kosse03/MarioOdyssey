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
	// ===== reveal cache =====
	TArray<TWeakObjectPtr<AActor>> RevealActors;
	bool bGlassesOn = true;

	// ===== input wrappers (protected 바인딩/접근 문제 회피) =====
	void Input_RunStarted_Proxy(const FInputActionValue& Value);
	void Input_RunCompleted_Proxy(const FInputActionValue& Value);
	void Input_ReleaseCapture_Passthrough(const FInputActionValue& Value);

	// ===== core =====
	void CacheRevealActors();
	void SetGlassesOn(bool bOn);
	void ApplyCapturedSpeed();
};
