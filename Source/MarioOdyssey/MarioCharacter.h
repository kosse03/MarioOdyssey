// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "MarioState.h"
#include "MarioCharacter.generated.h"


class UCameraComponent;
class USpringArmComponent;
struct FInputActionValue;

UCLASS()
class MARIOODYSSEY_API AMarioCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AMarioCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void Landed(const FHitResult& Hit) override;//점프 OnLanded 오버라이드
	
	//카메라
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	USpringArmComponent* SpringArm;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	UCameraComponent* FollowCamera;
	
	FRotator TargetControlRotation;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera")
	float CameraRotationInterpSpeed = 24.f;
	
	FVector SpringArmTargetOffset_Default = FVector::ZeroVector;
	
	//인풋
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputMappingContext* IMC_Player;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* IA_Move;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* IA_Look;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* IA_Jump;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* IA_Crouch;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* IA_Run;
	
	// 움직임 튜닝
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Move")
	float WalkSpeed = 200.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Move")
	float RunSpeed = 500.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Move")
	float TurnRate = 720.f;
	
	UPROPERTY(BlueprintReadOnly, Category = "State|Run", meta=(AllowPrivateAccess="true"))
	bool bIsRunning = false;
	
	float DefaultGravityScale = 2.0f;
	//점프
	UPROPERTY(BlueprintReadOnly, Category = "State|Jump", meta=(AllowPrivateAccess="true"))
	int32 JumpStage = 0; // 점프 단계
	UPROPERTY(EditDefaultsOnly, Category = "Jump")
	float JumpChainTime = 0.25f; //점프 콤보 가능 타임
	
	float LastLandedTime = -1.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Jump") //점프 단계별 점프 높이
	float JumpZ_Stage1 = 850.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Jump")
	float JumpZ_Stage2 = 890.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Jump")
	float JumpZ_Stage3 = 1030.f;
	
	void StartGroundPound();
	void EndGroundPoundStun();
	
	//웅크리기
	UPROPERTY(BlueprintReadOnly, Category="State|Crouch", meta=(AllowPrivateAccess="true"))
	bool bCrouchHeld = false; // C키 홀드 상태(롱점프/백텀블링 분기용)
	
	UPROPERTY(BlueprintReadOnly, Category="State|Crouch", meta=(AllowPrivateAccess="true"))
	float CrouchStartSpeed2D = 0.f; //c를 눌렀을때의 속도 저장
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Move|Crouch")
	float CrouchSpeedScale = 0.6f; // 웅크리기 중 이동 속도 비율
	
	UPROPERTY(EditDefaultsOnly, Category="Mario|LongJump")
	float LongJump_MinSpeed2D = 10.f; // 롱점프 진입 최소 값

	UPROPERTY(EditDefaultsOnly, Category="Mario|LongJump")
	float LongJump_ForwardStrength = 950.f; // 롱점프 전방 값

	UPROPERTY(EditDefaultsOnly, Category="Mario|LongJump")
	float LongJump_UpStrength = 800.f; // 롱점프 상승 값

	UPROPERTY(EditDefaultsOnly, Category="Mario|Backflip")
	float Backflip_BackStrength = 200.f; // 백덤블링 후방 값

	UPROPERTY(EditDefaultsOnly, Category="Mario|Backflip")
	float Backflip_UpStrength = 1100.f; // 백덤블링 상승 값

	
	UPROPERTY(BlueprintReadOnly, Category="State|Jump", meta=(AllowPrivateAccess="true"))
	bool bIsLongJumping = false; // 롱점프 상태

	UPROPERTY(BlueprintReadOnly, Category="State|Jump", meta=(AllowPrivateAccess="true"))
	bool bIsBackflipping = false; // 백덤블링 상태
	
	UPROPERTY(BlueprintReadOnly, Category="State|Crouch", meta=(AllowPrivateAccess="true"))
	bool bAnimIsCrouched = false; // 웅크리기 상태
	
	//엉덩방아
	UPROPERTY(BlueprintReadOnly, Category = "State|GroundPound", meta=(AllowPrivateAccess="true"))
	bool bIsGroundPoundPreparing = false; // 공중제비 중
	UPROPERTY(BlueprintReadOnly, Category = "State|GroundPound", meta=(AllowPrivateAccess="true"))
	bool bIsGroundPounding = false; // 낙하 중
	UPROPERTY(BlueprintReadOnly, Category = "State|GroundPound", meta=(AllowPrivateAccess="true"))
	bool bGroundPoundUsed = false; //공중 1회 사용 여부
	UPROPERTY(BlueprintReadOnly, Category = "State|GroundPound", meta=(AllowPrivateAccess="true"))
	bool bGroundPoundStunned = false; // 착지 후 경직
	UPROPERTY(BlueprintReadWrite, Category = "State|GroundPound", meta=(AllowPrivateAccess="true"))
	bool bWaitingForPoundJump = false; // 반동 점프 대기
	UPROPERTY(BlueprintReadOnly, Category="State|GroundPound", meta=(AllowPrivateAccess="true"))
	bool bPoundJumpConsumed = false; // 반동점프 1회 소비 여부
	UFUNCTION(BlueprintCallable, Category="State|GroundPound")
	void EndPoundJumpWindow(); // 애님 노티파이가 호출해서 창 종료 + 소비 리셋
	UPROPERTY(BlueprintReadOnly, Category="State|GroundPound", meta=(AllowPrivateAccess="true"))
	bool bIsPoundJumping = false;
	
	UPROPERTY(EditDefaultsOnly, Category = "GroundPound")
	float GroundPoundForce = 1000.f; // 엉덩방아 가속 크기
	
	UPROPERTY(EditDefaultsOnly, Category = "GroundPound")
	float GroundPoundPrepareTime = 0.77f; //공중제비 시간
	
	UPROPERTY(EditDefaultsOnly, Category = "GroundPound")
	float GroundPoundStunTime = 1.43f; // 엉덩방아 후 경직 시간
	
	UPROPERTY(EditDefaultsOnly, Category = "GroundPound")
	float PoundJumpZVelocity = 1100.f; //반동점프 크기
	
	FTimerHandle GroundPoundPrepareTimer;
	FTimerHandle GroundPoundStunTimer;
	
	// 다이브
	UPROPERTY(BlueprintReadOnly, Category="State|Dive", meta=(AllowPrivateAccess="true"))
	bool bIsDiving = false;

	UPROPERTY(EditDefaultsOnly, Category="Mario|Dive")
	float Dive_ForwardStrength = 1100.f;

	UPROPERTY(EditDefaultsOnly, Category="Mario|Dive")
	float Dive_UpStrength = 250.f;
	
	float DefaultAirControl = 0.8f;
	float DefaultMaxAcceleration = 2048.f;
	float DefaultBrakingDecelFalling = 0.f;
	bool  bDefaultOrientRotationToMovement = true;
	
	FVector StoredPoundFacingDir = FVector::ForwardVector;
	bool bHasStoredPoundFacingDir = false;
	
	//인풋 콜백
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void OnCrouchPressed(const FInputActionValue& Value);
	void OnCrouchReleased(const FInputActionValue& Value);
	
	void OnJumpPressed();//점프
	void OnRunPressed();//달리기
	void OnRunReleased();//달리기
	void OnGroundPoundPressed();//엉덩방아
	void BeginCrouchHold();//웅크리기
	void EndCrouchHold();//웅크리기
	void TryCrouchDerivedJump();//분기(롱/백텀블) 시도
	void DoLongJump();//롱점프
	void DoBackflip();//백덤블링
	void StartDiveFromCurrentContext();//다이브 시작
	void EndDive();//다이브 끝
	
	//헬퍼
	void ApplyMoveSpeed();
	
public:	
	virtual void Tick(float DeltaTime) override;
	
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;

private:
	FMarioState State;
};
