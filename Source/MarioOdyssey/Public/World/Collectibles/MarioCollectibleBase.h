#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MarioCollectibleBase.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class USoundBase;

UCLASS(Abstract)
class MARIOODYSSEY_API AMarioCollectibleBase : public AActor
{
	GENERATED_BODY()

public:
	AMarioCollectibleBase();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Collectible")
	USphereComponent* Sphere = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Collectible")
	UStaticMeshComponent* Mesh = nullptr;

	// ===== Sound =====
	// 수집 시 재생할 사운드(코인/하트/슈퍼문 별로 BP에서 다르게 지정)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Collectible|Sound")
	USoundBase* CollectSound = nullptr;

	// true: 2D(UI) 사운드처럼 재생, false: 월드 위치에서 재생
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Collectible|Sound")
	bool bPlaySound2D = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Collectible|Sound", meta=(ClampMin="0.0"))
	float CollectSoundVolume = 1.f;


	// ===== Simple motion (spin + float) =====
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Collectible|Motion")
	bool bEnableSpin = true;

	// Degrees per second around Z (Yaw)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Collectible|Motion", meta=(ClampMin="0.0"))
	float SpinSpeedDegPerSec = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Collectible|Motion")
	bool bEnableBob = true;

	// Amplitude (UU) on Z axis
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Collectible|Motion", meta=(ClampMin="0.0"))
	float BobAmplitude = 12.f;

	// Radians per second for sine wave
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Collectible|Motion", meta=(ClampMin="0.0"))
	float BobSpeed = 2.f;

	// 닿자마자 먹는 방식
	UFUNCTION()
	void OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// AMarioCharacter(BP_Mario 포함)일 때만 수집하도록 기본 필터
	virtual bool IsValidCollector(AActor* OtherActor) const;

	// 실제 효과(코인 증가/HP 회복/문 획득) 구현
	virtual void OnCollected(AActor* Collector) PURE_VIRTUAL(AMarioCollectibleBase::OnCollected, );

	// BP에서 사운드/이펙트 등을 붙이고 싶을 때 사용
	UFUNCTION(BlueprintImplementableEvent, Category="Collectible")
	void BP_OnCollected(AActor* Collector);

	// 먹은 뒤 처리
	virtual void AfterCollected();

private:
	bool bCollected = false;
	FVector InitialMeshRelativeLocation = FVector::ZeroVector;
	float MotionTime = 0.f;
	float BobPhase = 0.f;
};
