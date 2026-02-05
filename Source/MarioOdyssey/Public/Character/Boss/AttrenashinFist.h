#pragma once

#include "CoreMinimal.h"
#include "Character/Monster/MonsterCharacterBase.h"
#include "Character/Boss/AttrenashinTypes.h"
#include "AttrenashinFist.generated.h"

UCLASS()
class MARIOODYSSEY_API AAttrenashinFist : public AMonsterCharacterBase
{
	GENERATED_BODY()

public:
	AAttrenashinFist();

	virtual void Tick(float DeltaSeconds) override;

	void InitFist(class AAttrenashinBoss* InBoss, EFistSide InSide, class USceneComponent* InAnchor);
	void StartSlamSequence(AActor* TargetActor);

	UFUNCTION(BlueprintCallable, Category="Attrenashin|IceRain")
	void StartIceRainSlam();

	bool IsIdle() const { return State == EFistState::Idle; }
	bool IsStunned() const { return State == EFistState::Stunned; }
	bool IsCapturedDriving() const { return State == EFistState::CapturedDrive; }
	bool IsInSlamSequence() const { return State == EFistState::SlamSequence; }
	bool IsReturning() const { return State == EFistState::ReturnToAnchor; }
	bool IsIceRainSlam() const { return State == EFistState::IceRainSlam; }

	UFUNCTION(BlueprintPure, Category="Attrenashin|Anim")
	EFistSide GetFistSide() const { return Side; }

	UFUNCTION(BlueprintPure, Category="Attrenashin|Anim")
	EFistState GetFistState() const { return State; }

	UFUNCTION(BlueprintPure, Category="Attrenashin|Anim")
	EAttrenashinSlamSeq GetSlamSeq() const { return SlamSeq; }

	UFUNCTION(BlueprintPure, Category="Attrenashin|Anim")
	float GetSlamSeqTime() const { return SlamSeqT; }

	UFUNCTION(BlueprintPure, Category="Attrenashin|Anim")
	float GetMoveAboveSeconds() const { return MoveAboveSeconds; }

	UFUNCTION(BlueprintPure, Category="Attrenashin|Anim")
	float GetFollowAboveSeconds() const { return FollowAboveSeconds; }

	UFUNCTION(BlueprintPure, Category="Attrenashin|Anim")
	float GetPreSlamPauseSeconds() const { return PreSlamPauseSeconds; }

	UFUNCTION(BlueprintPure, Category="Attrenashin|Anim")
	float GetSlamDownSeconds() const { return SlamDownSeconds; }

	UFUNCTION(BlueprintPure, Category="Attrenashin|Anim")
	float GetPostImpactWaitSeconds() const { return PostImpactWaitSeconds; }

	UFUNCTION(BlueprintPure, Category="Attrenashin|Anim")
	float GetStunRemain() const { return StunRemain; }

	UFUNCTION(BlueprintPure, Category="Attrenashin|Anim")
	float GetStunSeconds() const { return StunSeconds; }

	UFUNCTION(BlueprintPure, Category="Attrenashin|Anim")
	float GetStunNormalized() const
	{
		return (StunSeconds > KINDA_SMALL_NUMBER) ? FMath::Clamp(StunRemain / StunSeconds, 0.f, 1.f) : 0.f;
	}

	UFUNCTION(BlueprintPure, Category="Attrenashin|Anim")
	FVector2D GetSteerInput() const { return Steer; }

	UFUNCTION(BlueprintCallable, Category="Attrenashin|Stun")
	void SetIgnoreIceStun(bool bIgnore) { bIgnoreIceStun = bIgnore; }

protected:
	virtual bool CanBeCaptured_Implementation(const FCaptureContext& Context) override;
	virtual void OnCapturedExtra(AController* Capturer, const FCaptureContext& Context) override;
	virtual void OnReleasedExtra(const FCaptureReleaseContext& Context) override;

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	void Input_Steer(const struct FInputActionValue& Value);

private:
	TWeakObjectPtr<class AAttrenashinBoss> Boss;
	TWeakObjectPtr<class USceneComponent> Anchor;

	EFistSide Side = EFistSide::Left;
	EFistState State = EFistState::Idle;

	EAttrenashinSlamSeq SlamSeq = EAttrenashinSlamSeq::None;
	float SlamSeqT = 0.f;
	TWeakObjectPtr<AActor> SeqTargetActor;

	FVector SeqFrozenHoverLoc = FVector::ZeroVector;
	FVector SeqFrozenImpactXY = FVector::ZeroVector;
	FVector SeqSlamDownTargetLoc = FVector::ZeroVector;
	bool bPendingStun = false;

	UPROPERTY(EditDefaultsOnly, Category="Attrenashin|IceRain", meta=(ClampMin="0.01"))
	float IceRainRiseSeconds = 1.33f;

	UPROPERTY(EditDefaultsOnly, Category="Attrenashin|IceRain", meta=(ClampMin="0.01"))
	float IceRainDropSeconds = 0.17f;

	UPROPERTY(EditDefaultsOnly, Category="Attrenashin|IceRain")
	float IceRainRiseZ = 800.f;

	float IceRainT = 0.f;
	FVector IceRainStartLoc = FVector::ZeroVector;
	FVector IceRainApexLoc = FVector::ZeroVector;
	FVector IceRainImpactLoc = FVector::ZeroVector;
	bool bIceRainImpactFired = false;

	float ReturnT = 0.f;
	float ReturnDuration = 2.f;

	FVector2D Steer = FVector2D::ZeroVector;

	bool bDamageOverlapEnabled = false;
	void SetDamageOverlapEnabled(bool bEnable);

	bool bIgnoreIceStun = false;

	UPROPERTY(EditDefaultsOnly, Category="Attrenashin|SlamSeq")
	float MoveAboveSeconds = 2.5f;

	UPROPERTY(EditDefaultsOnly, Category="Attrenashin|SlamSeq")
	float FollowAboveSeconds = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category="Attrenashin|SlamSeq")
	float PreSlamPauseSeconds = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category="Attrenashin|SlamSeq")
	float SlamDownSeconds = 0.12f;

	UPROPERTY(EditDefaultsOnly, Category="Attrenashin|SlamSeq")
	float PostImpactWaitSeconds = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category="Attrenashin|SlamSeq")
	float ReturnHomeSeconds = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category="Attrenashin|SlamSeq")
	float HoverExtraZ = 800.f;

	UPROPERTY(EditDefaultsOnly, Category="Attrenashin|SlamSeq")
	float MoveAboveMaxSpeed = 20000.f;

	UPROPERTY(EditDefaultsOnly, Category="Attrenashin|SlamSeq")
	float FollowAboveMaxSpeed = 20000.f;

	UPROPERTY(EditDefaultsOnly, Category="Attrenashin|SlamSeq")
	float ReturnMaxSpeed = 20000.f;

	UPROPERTY(EditDefaultsOnly, Category="Attrenashin|Stun", meta=(ClampMin="0.0"))
	float StunSeconds = 5.0f;

	UPROPERTY(EditDefaultsOnly, Category="Attrenashin|CapturedDrive")
	float CapturedDriveSpeed = 1100.f;

	UPROPERTY(EditDefaultsOnly, Category="Attrenashin|CapturedDrive")
	float CapturedYawRate = 220.f;

	float StunRemain = 0.f;

	void EnterState(EFistState NewState);

	void TickSlamSequence(float Dt);
	FVector GetDesiredHoverLocation() const;
	void MoveTowardAdaptive(const FVector& Target, float Dt, float TimeRemaining, float MaxSpeed, bool bSweep);

	void BeginSlamDown();
	void TickSlamDown(float Dt);

	void TickIceRainSlam(float Dt);
	void BuildIceRainImpactPoint();

	bool IsOnIceTileAt(const FVector& WorldXY) const;

	void StartReturnToAnchor(float DurationSeconds);
	void TickReturnToAnchor(float Dt);

	void TickCapturedDrive(float Dt);
};
