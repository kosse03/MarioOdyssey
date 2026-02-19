#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProgressBlockMover.generated.h"

class UStaticMeshComponent;
class UMarioGameInstance;

UCLASS()
class MARIOODYSSEY_API AProgressBlockMover : public AActor
{
	GENERATED_BODY()

public:
	AProgressBlockMover();
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UStaticMeshComponent* Mesh = nullptr;

	// ===== 조건 =====
	// 0이면 무시
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Progress")
	int32 RequiredSuperMoonCount = 0;

	// None이면 무시
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Progress")
	FName RequiredMoonId;

	// ===== 이동 =====
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Move")
	FVector TargetOffset = FVector(0.f, 0.f, 300.f);

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Move", meta=(ClampMin="0.01"))
	float MoveDuration = 1.2f;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Move")
	bool bTriggerOnlyOnce = true;

private:
	UPROPERTY()
	UMarioGameInstance* CachedGI = nullptr;

	bool bTriggered = false;
	bool bMoving = false;
	float MoveElapsed = 0.f;
	FVector StartLocation = FVector::ZeroVector;
	FVector EndLocation = FVector::ZeroVector;

	bool IsRequirementMet() const;
	void TryTrigger();
	void StartMove();

	UFUNCTION()
	void OnMoonCountChanged(int32 NewTotal);

	UFUNCTION()
	void OnMoonCollected(FName MoonId, int32 NewTotal);
};
