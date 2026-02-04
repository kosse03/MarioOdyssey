#pragma once

#include "CoreMinimal.h"
#include "Character/Monster/MonsterCharacterBase.h"
#include "AttrenashinFist.generated.h"

UENUM(BlueprintType)
enum class EFistSide : uint8
{
	Left UMETA(DisplayName="Left"),
	Right UMETA(DisplayName="Right"),
};

UENUM(BlueprintType)
enum class EFistState : uint8
{
	Idle UMETA(DisplayName="Idle"),
	SlamSequence UMETA(DisplayName="SlamSequence"),
	Stunned UMETA(DisplayName="Stunned"),
	CapturedDrive UMETA(DisplayName="CapturedDrive"),
	ReturnToAnchor UMETA(DisplayName="ReturnToAnchor"),
};

UENUM(BlueprintType)
enum class EAttrenashinSlamSeq : uint8
{
	None UMETA(DisplayName="None"),
	MoveAbove UMETA(DisplayName="MoveAbove"),          // 2.5s: 목표를 매 틱 갱신하며 "스냅 없이" 이동
	FollowAbove UMETA(DisplayName="FollowAbove"),      // 1.0s: 머리 위 추적(스냅 금지)
	PreSlamPause UMETA(DisplayName="PreSlamPause"),    // 0.5s: 정지(고정)
	SlamDown UMETA(DisplayName="SlamDown"),            // SlamDownSeconds: 빠르게 내려찍기(이동이 보이게)
	PostImpactWait UMETA(DisplayName="PostImpactWait") // 1.0s: 강타 후 대기
};

UCLASS()
class MARIOODYSSEY_API AAttrenashinFist : public AMonsterCharacterBase
{
	GENERATED_BODY()

public:
	AAttrenashinFist();

	virtual void Tick(float DeltaSeconds) override;

	void InitFist(class AAttrenashinBoss* InBoss, EFistSide InSide, class USceneComponent* InAnchor);

	void StartSlamSequence(AActor* TargetActor);

	bool IsIdle() const { return State == EFistState::Idle; }
	bool IsStunned() const { return State == EFistState::Stunned; }
	bool IsCapturedDriving() const { return State == EFistState::CapturedDrive; }
	bool IsInSlamSequence() const { return State == EFistState::SlamSequence; }
	bool IsReturning() const { return State == EFistState::ReturnToAnchor; }

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

	// ===== Slam sequence =====
	EAttrenashinSlamSeq SlamSeq = EAttrenashinSlamSeq::None;
	float SlamSeqT = 0.f;
	TWeakObjectPtr<AActor> SeqTargetActor;

	FVector SeqFrozenHoverLoc = FVector::ZeroVector;
	FVector SeqFrozenImpactXY = FVector::ZeroVector;
	FVector SeqSlamDownTargetLoc = FVector::ZeroVector;
	bool bPendingStun = false;

	// ===== Return-to-anchor =====
	float ReturnT = 0.f;
	float ReturnDuration = 2.f;

	// ===== Captured drive =====
	FVector2D Steer = FVector2D::ZeroVector;

	// ===== Tunables =====
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

	// 스냅 방지용 최대 속도
	UPROPERTY(EditDefaultsOnly, Category="Attrenashin|SlamSeq")
	float MoveAboveMaxSpeed = 20000.f;

	UPROPERTY(EditDefaultsOnly, Category="Attrenashin|SlamSeq")
	float FollowAboveMaxSpeed = 20000.f;

	UPROPERTY(EditDefaultsOnly, Category="Attrenashin|SlamSeq")
	float ReturnMaxSpeed = 20000.f;

	UPROPERTY(EditDefaultsOnly, Category="Attrenashin|Stun")
	float StunSeconds = 2.2f;

	UPROPERTY(EditDefaultsOnly, Category="Attrenashin|CapturedDrive")
	float CapturedDriveSpeed = 1100.f;

	UPROPERTY(EditDefaultsOnly, Category="Attrenashin|CapturedDrive")
	float CapturedYawRate = 220.f;

	float StunRemain = 0.f;

	// ===== Internals =====
	void EnterState(EFistState NewState);

	void TickSlamSequence(float Dt);
	FVector GetDesiredHoverLocation() const;

	// 남은 시간 기반 적응 이동(스냅 금지)
	void MoveTowardAdaptive(const FVector& Target, float Dt, float TimeRemaining, float MaxSpeed, bool bSweep);

	void BeginSlamDown();
	void TickSlamDown(float Dt);

	bool IsOnIceTileAt(const FVector& WorldXY) const;

	void StartReturnToAnchor(float DurationSeconds);
	void TickReturnToAnchor(float Dt);

	void TickCapturedDrive(float Dt);
	
	bool bDamageOverlapEnabled = false;
	void SetDamageOverlapEnabled(bool bEnable);
};
