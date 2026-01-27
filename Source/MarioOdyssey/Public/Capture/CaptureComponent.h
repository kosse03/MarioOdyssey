#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Capture/CaptureTypes.h"
#include "CaptureComponent.generated.h"

class AMarioCharacter;
class APawn;
class APlayerController;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class MARIOODYSSEY_API UCaptureComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCaptureComponent();

	// === 외부 진입점(다음 단계에서 HatHit/C키에 연결) ===
	UFUNCTION(BlueprintCallable, Category="Capture")
	bool TryCapture(AActor* TargetActor, const FCaptureContext& Context);

	UFUNCTION(BlueprintCallable, Category="Capture")
	bool ReleaseCapture(ECaptureReleaseReason Reason);

	UFUNCTION(BlueprintCallable, Category="Capture")
	void ForceReleaseForGameOver();

	UFUNCTION(BlueprintCallable, Category="Capture")
	bool IsCapturing() const { return bIsCapturing; }

	UFUNCTION(BlueprintCallable, Category="Capture")
	APawn* GetCapturedPawn() const { return CapturedPawn.Get(); }

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	// 정책 값(튜닝은 나중에)
	UPROPERTY(EditDefaultsOnly, Category="Capture|Exit")
	float ExitForwardOffset = 110.f;

	UPROPERTY(EditDefaultsOnly, Category="Capture|Exit")
	float ExitUpOffset = 60.f;

	UPROPERTY(EditDefaultsOnly, Category="Capture|Exit")
	float ExitNavProjectRadius = 250.f;

private:
	TWeakObjectPtr<AMarioCharacter> OriginalMario;
	TWeakObjectPtr<APlayerController> CachedPC;

	TWeakObjectPtr<AActor> CapturedActor; // 인터페이스 호출용
	TWeakObjectPtr<APawn>  CapturedPawn;

	bool bIsCapturing = false;
	bool bPrevAutoManageCameraTarget = true;

private:
	void CachePlayerControllerIfNeeded();

	void ApplyMarioCaptureHide(bool bHide);
	void AttachMarioToCapturedPawn();
	void DetachMario();

	bool ComputeExitLocation(FVector& OutExitLocation) const;

private:
	UFUNCTION()
	void HandleCapturedPawnAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
	                                 AController* InstigatedBy, AActor* DamageCauser);
};
