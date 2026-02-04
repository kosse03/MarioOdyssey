#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Character/Monster/MonsterCharacterBase.h"
#include "GoombaCharacter.generated.h"

class AAIController;
class USphereComponent;
class UPrimitiveComponent;
struct FInputActionValue;

UENUM(BlueprintType)
enum class EGoombaAIState : uint8
{
	Patrol,
	Chase,
	LookAround,
	ReturnHome,
	Stunned
};

UCLASS()
class MARIOODYSSEY_API AGoombaCharacter : public AMonsterCharacterBase
{
	GENERATED_BODY()

public:
	AGoombaCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// 캡쳐 입력을 "스택 루트(가장 밑 굼바)"로 포워딩하기 위해 굼바는 입력 바인딩을 커스텀한다.
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// =========================
	// 캡쳐/해제 훅
	// =========================
	virtual void OnCapturedExtra(AController* Capturer, const FCaptureContext& Context) override;
	virtual void OnReleasedExtra(const FCaptureReleaseContext& Context) override;

	// 스택 상태에서 캡쳐 피격 반응(넉백)을 루트에 적용
	virtual void OnCapturedPawnDamaged_Implementation(float Damage, AController* InstigatorController, AActor* DamageCauser) override;

	// =========================
	// Capturable Interface (스택 최상단을 캡쳐 대상으로)
	// =========================
	virtual APawn* GetCapturePawn_Implementation() override;

	// =========================
	// AI 튜닝값
	// =========================
	UPROPERTY(EditDefaultsOnly, Category="Goomba|AI")
	float PatrolRadius = 600.f;

	UPROPERTY(EditDefaultsOnly, Category="Goomba|AI")
	float DetectRange = 500.f;

	UPROPERTY(EditDefaultsOnly, Category="Goomba|AI")
	float DetectHalfAngleDeg = 35.f; // 원뿔 반각

	UPROPERTY(EditDefaultsOnly, Category="Goomba|AI")
	float WalkSpeed = 120.f;

	UPROPERTY(EditDefaultsOnly, Category="Goomba|AI")
	float ChaseSpeed = 300.f;

	UPROPERTY(EditDefaultsOnly, Category="Goomba|AI")
	float LookAroundSeconds = 2.f;

	UPROPERTY(EditDefaultsOnly, Category="Goomba|AI")
	float ReturnHomeAfterSeconds = 30.f;

	// 언캡쳐 후 스턴
	UPROPERTY(EditDefaultsOnly, Category="Goomba|AI")
	float ReleaseStunSeconds = 3.f;

	// =========================
	// Stack (무제한)
	// =========================
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Goomba|Stack")
	TObjectPtr<USphereComponent> HeadStackSphere = nullptr;

	// 아래/위 연결(루트=null 아래, 탑=null 위)
	TWeakObjectPtr<AGoombaCharacter> StackBelow;
	TWeakObjectPtr<AGoombaCharacter> StackAbove;

	// 스택 간격(캡슐 반높이 합 - Gap). Gap이 0이면 딱 맞닿게.
	UPROPERTY(EditDefaultsOnly, Category="Goomba|Stack", meta=(ClampMin="0.0", ClampMax="20.0"))
	float StackGap = 2.f;

	// "캡쳐 굼바가 다른 굼바 머리 위에 올라갔을 때"만 스택을 쌓는다(즉, 스택 시작은 캡쳐 기반).
	UPROPERTY(EditDefaultsOnly, Category="Goomba|Stack")
	bool bOnlyStackWhenCapturedInUpperStack = true;

	UFUNCTION()
	void OnHeadStackSphereBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	                                  UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	                                  bool bFromSweep, const FHitResult& SweepResult);


	// Goomba 전용 컨택(스택 내부 자기넉백 방지)
	UFUNCTION()
	void OnContactBeginOverlap_Goomba(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// 스택 헬퍼
	bool IsStackRoot() const { return !StackBelow.IsValid(); }
	bool IsStackTop() const { return !StackAbove.IsValid(); }
	bool IsStackFollower() const { return StackBelow.IsValid(); }

	AGoombaCharacter* GetStackRoot();
	AGoombaCharacter* GetStackTop();
	AGoombaCharacter* GetCapturedInStack();
	bool HasCapturedInStack() { return GetCapturedInStack() != nullptr; }

	// upperRoot(=이 스택의 루트)를 lowerTop 위에 붙여서 스택을 합친다.
	bool AttachStackRootAbove(AGoombaCharacter* LowerTop);

	// 루트에서만 호출: 회전/오프셋/컨택스피어 상태 등을 정리
	void UpdateStackPresentation(float Dt);

	// 상태 전환(플레이어 조종 스택 <-> 일반 몬스터 스택)
	void ApplyStackModeTransition(bool bNowPlayerControlled, AGoombaCharacter* Captured);

	// 루트 AI 스턴 강제
	void ForceStun(float Seconds);

	// =========================
	// Capture-Control Input (Goomba 전용 바인딩)
	// =========================
	void Input_Move_Stack(const FInputActionValue& Value);
	void Input_Look_Passthrough(const FInputActionValue& Value);
	void Input_RunStarted_Stack(const FInputActionValue& Value);
	void Input_RunCompleted_Stack(const FInputActionValue& Value);
	void Input_JumpStarted_Stack(const FInputActionValue& Value);
	void Input_JumpCompleted_Stack(const FInputActionValue& Value);
	void Input_ReleaseCapture_Passthrough(const FInputActionValue& Value);
	void ClearCapturedHitStun_Proxy();

	void ApplyCapturedSpeedToStackRoot();

	// =========================
	// Damage forwarding (스택 어디 맞아도 캡쳐 굼바/마리오 HP로 라우팅)
	// =========================
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	                         AController* EventInstigator, AActor* DamageCauser) override;

	bool bForwardingDamage = false;

	// =========================
	// 디버그
	// =========================
	UPROPERTY(EditAnywhere, Category="Goomba|Debug")
	bool bDrawSearchCone = true;

	UPROPERTY(EditAnywhere, Category="Goomba|Debug")
	float DebugZOffset = 10.f;

	UPROPERTY(EditAnywhere, Category="Goomba|Debug", meta=(ClampMin="4", ClampMax="64"))
	int32 DebugArcSegments = 24;

	UPROPERTY(EditDefaultsOnly, Category="Goomba|AI")
	float DetectHalfAngleYawDeg = 45.f;   // 좌/우

	UPROPERTY(EditDefaultsOnly, Category="Goomba|AI")
	float DetectHalfAnglePitchDeg = 20.f; // 위/아래

	void DrawSearchConeDebug() const;

#if WITH_EDITOR
	virtual bool ShouldTickIfViewportsOnly() const override { return bDrawSearchCone; }
#endif

private:
	// 해제 후 스턴
	float StunRemain = 0.f;

	// AI 상태
	EGoombaAIState State = EGoombaAIState::Stunned;
	FVector HomeLocation = FVector::ZeroVector;

	FVector PatrolTarget = FVector::ZeroVector;
	bool bHasPatrolTarget = false;

	float LookAroundRemain = 0.f;
	float ReturnHomeTimer = 0.f;

	// stack mode cache (루트에서만 사용)
	bool bCachedPlayerControlledStack = false;
	bool bCachedOrientRotationToMovement = true;

	void SetState(EGoombaAIState NewState);

	AActor* GetPlayerTargetActor() const;
	bool CanDetectTarget(const AActor* Target) const;

	AAIController* GetAICon() const;

	void UpdatePatrol(float Dt);
	void UpdateChase(float Dt, AActor* Target);
	void UpdateLookAround(float Dt);
	void UpdateReturnHome(float Dt);
};
