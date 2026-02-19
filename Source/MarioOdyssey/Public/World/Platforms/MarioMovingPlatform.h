#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MarioMovingPlatform.generated.h"

class UStaticMeshComponent;
class UMarioGameInstance;

UCLASS()
class MARIOODYSSEY_API AMarioMovingPlatform : public AActor
{
	GENERATED_BODY()

public:
	AMarioMovingPlatform();
	virtual void Tick(float DeltaSeconds) override;

	/** Manually activate movement (also used internally when requirements are met). */
	UFUNCTION(BlueprintCallable, Category="Move")
	void ActivateMovement(bool bResetPhase = true);

	/** Manually stop movement. */
	UFUNCTION(BlueprintCallable, Category="Move")
	void DeactivateMovement();

	/** Returns whether the platform is currently moving. */
	UFUNCTION(BlueprintCallable, Category="Move")
	bool IsMovementActive() const { return bActive; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Visible mesh + collision (Root). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	UStaticMeshComponent* Mesh = nullptr;

	// ===== Progress gating =====

	/** 0이면 무시. (예: 3개 이상 모았을 때 활성화) */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Progress")
	int32 RequiredSuperMoonCount = 0;

	/** None이면 무시. (특정 문 ID를 먹어야 활성화) */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Progress")
	FName RequiredMoonId;

	/** 조건 충족 시 1회만 활성화하고 이후에는 조건 변화를 무시. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Progress")
	bool bTriggerOnlyOnce = true;

	// ===== Movement =====

	/** StartLocation 기준으로 이동할 목표 오프셋(월드/로컬 선택). */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Move")
	FVector TargetOffset = FVector(400.f, 0.f, 0.f);

	/** Start->End(편도) 이동 시간(초). PingPong이면 왕복은 2배. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Move", meta=(ClampMin="0.01"))
	float MoveDuration = 2.0f;

	/** true면 Start<->End 왕복, false면 Start->End 도착 후 Start로 점프(루프). */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Move")
	bool bPingPong = true;

	/** true면 SmoothStep으로 가감속, false면 선형. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Move")
	bool bEaseInOut = true;

	/** true면 액터의 시작 회전을 기준으로 TargetOffset을 로컬방향으로 해석. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Move")
	bool bUseLocalOffset = true;

	/** 요구조건이 없을 때, 시작부터 움직일지 여부. */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Move")
	bool bStartActive = true;

	/** 활성화 시 랜덤 페이즈로 시작(여러 플랫폼이 같은 타이밍으로 움직이는 것 방지). */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Move")
	bool bRandomStartPhase = false;

private:
	UPROPERTY()
	UMarioGameInstance* CachedGI = nullptr;

	bool bTriggered = false;
	bool bActive = false;

	float MoveElapsed = 0.f;

	FVector StartLocation = FVector::ZeroVector;
	FRotator StartRotation = FRotator::ZeroRotator;
	FVector OffsetWorld = FVector::ZeroVector;
	FVector EndLocation = FVector::ZeroVector;

	/** Compute endpoints from current placement. */
	void RefreshEndpoints();

	/** Checks whether progress requirements are met (if any). */
	bool IsRequirementMet() const;

	/** Attempts to activate movement based on requirements / start flags. */
	void TryActivateFromProgress();

	/** Converts elapsed time to [0..1] alpha for current cycle. */
	float CalcAlpha(float Elapsed) const;

	UFUNCTION()
	void OnMoonCountChanged(int32 NewTotal);

	UFUNCTION()
	void OnMoonCollected(FName MoonId, int32 NewTotal);
};
