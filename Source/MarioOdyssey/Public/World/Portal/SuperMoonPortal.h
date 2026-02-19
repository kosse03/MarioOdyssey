#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SuperMoonPortal.generated.h"

class UBoxComponent;
class UMarioGameInstance;
class AMarioCharacter;

/**
 * SuperMoonPortal
 * - Mesh 없는 콜리전 포탈
 * - 슈퍼문 RequiredSuperMoons 개 이상이면 활성화(콜리전 ON)
 * - 플레이어(마리오)가 닿으면 FadeIn -> OpenLevel -> (다음 맵 BeginPlay에서 FadeOut)
 * - 다음 맵에서 적용할 SpawnTransform/FadeOut은 GameInstance에 임시 저장
 */
UCLASS()
class MARIOODYSSEY_API ASuperMoonPortal : public AActor
{
	GENERATED_BODY()

public:
	ASuperMoonPortal();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category="Portal")
	TObjectPtr<UBoxComponent> Trigger = nullptr;

	// ===== Settings =====
	UPROPERTY(EditAnywhere, Category="Portal|Condition", meta=(ClampMin="0"))
	int32 RequiredSuperMoons = 10;

	UPROPERTY(EditAnywhere, Category="Portal|Travel")
	FName TargetLevelName = TEXT("Arena");

	UPROPERTY(EditAnywhere, Category="Portal|Travel")
	FVector TargetSpawnLocation = FVector(-19727.f, 0.f, 7300.f);

	UPROPERTY(EditAnywhere, Category="Portal|Travel")
	FRotator TargetSpawnRotation = FRotator(0.f, 0.f, 0.f);

	UPROPERTY(EditAnywhere, Category="Portal|Fade", meta=(ClampMin="0.0"))
	float FadeInSeconds = 0.35f;

	UPROPERTY(EditAnywhere, Category="Portal|Fade", meta=(ClampMin="0.0"))
	float BlackHoldSeconds = 0.05f;

	UPROPERTY(EditAnywhere, Category="Portal|Fade", meta=(ClampMin="0.0"))
	float FadeOutSeconds = 0.35f;

	UPROPERTY(EditAnywhere, Category="Portal|Fade")
	bool bLockInputUntilFadeOut = true;

	// ===== Runtime =====
	bool bPortalEnabled = false;
	bool bTravelInProgress = false;

	FTimerHandle TravelTimer;

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	                          UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	                          bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleMoonCountChanged(int32 NewTotalMoons);

	void UpdateEnabledState();
	void StartTravel(AMarioCharacter* Mario);
	void OpenTargetLevel();

	void StartBlackFade(float FromAlpha, float ToAlpha, float DurationSeconds, bool bHoldWhenFinished) const;

	UMarioGameInstance* GetGI() const;
};
