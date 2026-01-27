#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Capture/CapturableInterface.h"
#include "Components/SphereComponent.h"
#include "InputAction.h"
#include "GoombaCharacter.generated.h"

struct FInputActionValue;
class AMarioCharacter;
class AAIController;

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
class MARIOODYSSEY_API AGoombaCharacter : public ACharacter, public ICapturableInterface
{
	GENERATED_BODY()

public:
	AGoombaCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	
	//접촉 데미지, 넉백
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Goomba|Combat", meta=(AllowPrivateAccess="true"))
	USphereComponent* ContactSphere = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Goomba|Combat")
	float ContactDamage = 1.f;

	UPROPERTY(EditDefaultsOnly, Category="Goomba|Combat")
	float ContactDamageCooldown = 0.6f;

	UPROPERTY(EditDefaultsOnly, Category="Goomba|Combat")
	float KnockbackStrength = 550.f;

	UPROPERTY(EditDefaultsOnly, Category="Goomba|Combat")
	float KnockbackUp = 120.f;

	float LastContactDamageTime = -1000.f;

	UFUNCTION()
	void OnContactBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
							   UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
							   const FHitResult& SweepResult);

	// Input 캡쳐 중 C키 해제용: IA_Crouch 재사용 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> IA_Crouch = nullptr;

	void OnReleaseCapturePressed(const FInputActionValue& Value);

	// AI 튜닝값 
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

	//ICapturableInterface (일단 “캡쳐 가능” 기본)
	virtual bool CanBeCaptured_Implementation(const FCaptureContext& Context) override { return true; }
	virtual APawn* GetCapturePawn_Implementation() override { return this; }
	virtual void OnCaptured_Implementation(AController* Capturer, const FCaptureContext& Context) override;
	virtual void OnReleased_Implementation(const FCaptureReleaseContext& Context) override;
	virtual void OnCapturedPawnDamaged_Implementation(float Damage, AController* InstigatorController, AActor* DamageCauser) override {}

	//서칭콘 디버그용//////////////////////////////////////
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
	////////////////////////////////////////////////////
private:
	// 캡쳐 상태
	bool bIsCaptured = false;
	TWeakObjectPtr<AMarioCharacter> CapturingMario;

	// AI 상태
	EGoombaAIState State = EGoombaAIState::Stunned;
	FVector HomeLocation = FVector::ZeroVector;

	FVector PatrolTarget = FVector::ZeroVector;
	bool bHasPatrolTarget = false;

	float LookAroundRemain = 0.f;
	float ReturnHomeTimer = 0.f;

private:
	void SetState(EGoombaAIState NewState);

	AMarioCharacter* GetPlayerMario() const;
	bool CanDetectPlayer(const AMarioCharacter* Mario) const;

	AAIController* GetAICon() const;
	void StopAIMove();

	void UpdatePatrol(float Dt);
	void UpdateChase(float Dt, AMarioCharacter* Mario);
	void UpdateLookAround(float Dt);
	void UpdateReturnHome(float Dt);
};
