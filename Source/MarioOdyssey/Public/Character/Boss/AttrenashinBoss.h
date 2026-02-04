#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AttrenashinBoss.generated.h"

UENUM(BlueprintType)
enum class EAttrenashinPhase : uint8
{
	Phase0 UMETA(DisplayName="Phase0"),
	Phase1 UMETA(DisplayName="Phase1"),
	Phase2 UMETA(DisplayName="Phase2"),
	Phase3 UMETA(DisplayName="Phase3"),
};

UCLASS()
class MARIOODYSSEY_API AAttrenashinBoss : public AActor
{
	GENERATED_BODY()

public:
	AAttrenashinBoss();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root = nullptr;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> FistAnchorL = nullptr;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> FistAnchorR = nullptr;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<class USphereComponent> HeadHitSphere = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Spawn")
	TSubclassOf<class AAttrenashinFist> FistClass;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase1")
	float SlamAttemptInterval = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Head")
	bool bRequireCapturedFistForHeadHit = true;

private:
	EAttrenashinPhase Phase = EAttrenashinPhase::Phase1;

	TWeakObjectPtr<class AAttrenashinFist> LeftFist;
	TWeakObjectPtr<class AAttrenashinFist> RightFist;

	float SlamAttemptTimer = 0.f;
	bool bNextLeft = true;

	int32 HeadHitCount = 0;

	UFUNCTION()
	void OnHeadBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	                        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	                        bool bFromSweep, const FHitResult& SweepResult);

	AActor* GetPlayerTarget() const;
	void TryStartPhase1Slam();
};
