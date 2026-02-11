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
class UBoxComponent;
struct FInputActionValue;
class UCaptureComponent;
class APlayerController;

UCLASS()
class MARIOODYSSEY_API AMarioCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AMarioCharacter();
	UCaptureComponent* GetCaptureComp() const { return CaptureComp; }
	
	UFUNCTION(BlueprintCallable, Category="Mario|Capture")
	void OnCaptureBegin();
	UFUNCTION(BlueprintCallable, Category="Mario|Capture")
	void OnCaptureEnd();

	UFUNCTION(BlueprintCallable, Category="Mario|Checkpoint")
	void SetCheckpointTransform(const FTransform& InCheckpointTransform);

	UFUNCTION(BlueprintCallable, Category="Mario|Checkpoint")
	bool HasCheckpoint() const { return bHasCheckpoint; }

	UFUNCTION(BlueprintCallable, Category="Mario|Checkpoint")
	FTransform GetCheckpointTransform() const { return SavedCheckpointTransform; }

	UFUNCTION(BlueprintCallable, Category="Mario|Checkpoint")
	bool TeleportToCheckpoint(bool bResetVelocity = true);
	
protected:
	virtual void BeginPlay() override;
	virtual void Landed(const FHitResult& Hit) override;//점프 OnLanded 오버라이드

	// 플레이어-몬스터 접촉(블로킹) 데미지/넉백
	UFUNCTION()
	void OnMarioCapsuleHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);
	
	//캡쳐 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mario|Capture", meta=(AllowPrivateAccess="true"))
	UCaptureComponent* CaptureComp = nullptr;
	
	//wall, ledge 디텍터 컴포넌트 오버랩 함수
	UFUNCTION()
	void OnWallDetectorBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnWallDetectorEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UFUNCTION()
	void OnLedgeDetectorBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnLedgeDetectorEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
	
	//카메라
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	USpringArmComponent* SpringArm;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	UCameraComponent* FollowCamera;
	
	FRotator TargetControlRotation;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera")
	float CameraRotationInterpSpeed = 24.f;
	
	FVector SpringArmTargetOffset_Default = FVector::ZeroVector;
	
	//디텍터( wall, ledge 컴포넌트 포인터 )
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Detection", meta=(AllowPrivateAccess="true"))
	UBoxComponent* WallDetector = nullptr; 

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Detection", meta=(AllowPrivateAccess="true"))
	UBoxComponent* LedgeDetector = nullptr;
	
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
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* IA_ThrowCap;
	
	// 움직임 튜닝
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Move")
	float WalkSpeed = 200.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Move")
	float RunSpeed = 690.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Move")
	float TurnRate = 720.f;
	
	UPROPERTY(BlueprintReadOnly, Category = "State|Run", meta=(AllowPrivateAccess="true"))
	bool bIsRunning = false;
	
	float DefaultGravityScale = 2.0f;
	
	//캐피액션
	UPROPERTY(EditDefaultsOnly, Category="Mario|Cap")
	TSubclassOf<class AMarioCapProjectile> CapProjectileClass;
	
	UPROPERTY(VisibleInstanceOnly, Category="Mario|Cap")
	TObjectPtr<AActor> ActiveCap = nullptr;
	
	UPROPERTY(EditDefaultsOnly, Category="Mario|Cap")
	float CapThrowSpeed = 1800.f;

	UPROPERTY(EditDefaultsOnly, Category="Mario|Cap")
	FVector CapSpawnOffset = FVector(30.f, 0.f, 0.f); // 일단 캐릭터
	
	UPROPERTY(EditDefaultsOnly, Category="Mario|Cap|Anim")
	UAnimMontage* Montage_ThrowCap_Ground = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Mario|Cap|Anim")
	UAnimMontage* Montage_ThrowCap_Air = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Mario|Cap|Anim")
	UAnimMontage* Montage_CatchCap_Ground = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Mario|Cap|Anim")
	UAnimMontage* Montage_CatchCap_Air = nullptr;
	
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
	float JumpZ_Stage3 = 1230.f;
	
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
	float Backflip_UpStrength = 1200.f; // 백덤블링 상승 값

	
	UPROPERTY(BlueprintReadOnly, Category="State|Jump", meta=(AllowPrivateAccess="true"))
	bool bIsLongJumping = false; // 롱점프 상태

	UPROPERTY(BlueprintReadOnly, Category="State|Jump", meta=(AllowPrivateAccess="true"))
	bool bIsBackflipping = false; // 백덤블링 상태
	
	UPROPERTY(BlueprintReadOnly, Category="State|Crouch", meta=(AllowPrivateAccess="true"))
	bool bAnimIsCrouched = false; // 웅크리기 상태
	
	//wall action
	UPROPERTY(BlueprintReadOnly, Category="State|Wall", meta=(AllowPrivateAccess="true"))
	EWallActionState WallActionState = EWallActionState::None;
	
	UPROPERTY(BlueprintReadOnly, Category="State|Wall", meta=(AllowPrivateAccess="true"))
	FVector CurrentWallNormal = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="State|Wall", meta=(AllowPrivateAccess="true"))
	bool bWallOverlapping = false;

	FTimerHandle WallSlideStartToLoopTimer;

	UPROPERTY(EditDefaultsOnly, Category="Mario|Wall")
	float WallSlideStartDuration = 0.83f; // SlideStart -> SlideLoop 전이 시간

	UPROPERTY(EditDefaultsOnly, Category="Mario|Wall")
	float WallTraceDistance = 35.f; // Overlap Begin 순간 보조 스윕 거리

	UPROPERTY(EditDefaultsOnly, Category="Mario|Wall")
	float WallTraceRadius = 12.f; // 보조 스윕 반지름

	UPROPERTY(EditDefaultsOnly, Category="Mario|Wall")
	float WallMinFrontDot = 0.75f; // "거의 정면" 접근 판정(0.7~0.85 튜닝)
	
	UPROPERTY(EditDefaultsOnly, Category="Mario|Wall")
	float WallKickHorizontalVelocity = 500.f; // 수평 이동

	UPROPERTY(EditDefaultsOnly, Category="Mario|Wall")
	float WallKickVerticalVelocity = 920.f; // 수직 이동

	UPROPERTY(EditDefaultsOnly, Category="Mario|Wall")
	float WallKickStateDuration = 0.1f; // WallKick 상태 유지 시간(AnimBP에서 KickJump 재생용)

	FTimerHandle WallKickStateTimer;
	//EndOverlap 순간에 바로 Reset되지 않도록 유예(회전 고정/박스 얇음 때문에 바로 EndOverlap 나는 케이스 방지)
	UPROPERTY(EditDefaultsOnly, Category="Mario|Wall")
	float WallEndOverlapGraceTime = 0.08f;

	UPROPERTY(EditDefaultsOnly, Category="Mario|Wall")
	float WallKickMinReenterDelay = 0.06f; // WallKick 직후 같은 벽 즉시 재포획 방지 (연속 킥 가능)

	UPROPERTY(EditDefaultsOnly, Category="Mario|Wall")
	float WallKickInputLockDuration = 0.12f; // WallKick 직후 입력/가속 잠금 시간

	UPROPERTY(EditDefaultsOnly, Category="Mario|Wall")
	float WallKickSlideReenterWindow = 0.35f; // 벽차기 후 이 시간 동안은 Slide 재진입을 WallKick 경로로 처리
	
	UPROPERTY(EditDefaultsOnly, Category="Mario|Wall")
	float WallSlideMaxDownSpeed = 550.f;   // 아래로 떨어지는 속도 상한(큰 값 = 빨리 미끌)

	UPROPERTY(EditDefaultsOnly, Category="Mario|Wall")
	float WallSlideGravityScale = 0.9f;    // 슬라이드 중 중력 스케일 (기본보다 낮추면 더 천천히 미끌)

	UPROPERTY(EditDefaultsOnly, Category="Mario|Wall")
	float WallSlideAirControl = 0.05f;     // 슬라이드 중 공중 조작

	UPROPERTY(EditDefaultsOnly, Category="Mario|Wall")
	float WallSlideMaxAcceleration = 200.f; // 슬라이드 중 가속(좌우 흔들림 억제)

	UPROPERTY(EditDefaultsOnly, Category="Mario|Wall")
	float WallSlideBrakingDecelFalling = 2048.f; // 슬라이드 중 횡속 감쇠(선택)
	
	UPROPERTY(EditDefaultsOnly, Category="Mario|Wall")
	float WallSlideDownSpeed = 220.f; // 슬라이드 하강 속도- 가속 없이 일정
	
	FTimerHandle WallKickInputLockTimer;

	FTimerHandle WallEndOverlapGraceTimer;
	
	float WallKickStartTime = -1000.f; // WallKick 시작 시각
	bool bWallKickInputLocked = false;
	
	float CachedAirControl = 0.f;// 입력 잠금 동안 임시로 바꿀 이동 파라미터 캐시
	float CachedMaxAcceleration = 0.f;
	
	//ledge action
	UPROPERTY(BlueprintReadOnly, Category="State|Ledge", meta=(AllowPrivateAccess="true"))
	ELedgeState LedgeActionState = ELedgeState::None;
	
	//구르기
	UPROPERTY(BlueprintReadOnly, Category="State|Roll", meta=(AllowPrivateAccess="true"))
	bool bIsRolling = false;

	UPROPERTY(BlueprintReadOnly, Category="State|Roll", meta=(AllowPrivateAccess="true"))
	ERollPhase RollPhase = ERollPhase::None;

	UPROPERTY(BlueprintReadOnly, Category="State|Roll", meta=(AllowPrivateAccess="true"))
	FVector RollDirection = FVector::ZeroVector;
	
	UPROPERTY(EditDefaultsOnly, Category="Mario|Roll")
	float RollSpeed = 900.f;

	UPROPERTY(EditDefaultsOnly, Category="Mario|Roll")
	float RollStartDuration = 0.17f;   // RollStart 애니 길이

	UPROPERTY(EditDefaultsOnly, Category="Mario|Roll")
	float RollLoopDuration = 0.0f;    // 0이면 Loop 무한(외부에서 End 호출 방식)

	UPROPERTY(EditDefaultsOnly, Category="Mario|Roll")
	float RollEndDuration = 0.27f;     // RollEnd 애니 길이

	UPROPERTY(EditDefaultsOnly, Category="Mario|Roll")
	float RollGroundFriction = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category="Mario|Roll")
	float RollBrakingDecel = 180.f;

	UPROPERTY(EditDefaultsOnly, Category="Mario|Roll")
	float RollMaxAcceleration = 2000.f; // 롤 가속(앞으로 튕기게)

	UPROPERTY(EditDefaultsOnly, Category="Mario|Roll")
	float RollSteerInterpSpeed = 3.f; // 롤 중 방향 전환(좌/우 조작)

	UPROPERTY(EditDefaultsOnly, Category="Mario|Roll")
	float RollEndSpeed2D = 160.f; // 속도가 이 값 이하로 떨어지면 RollEnd로 전이

	UPROPERTY(EditDefaultsOnly, Category="Mario|Roll")
	float RollReverseCancelDot = -0.35f; // 입력 방향이 이 값 이하(반대)이면 롤 종료

	UPROPERTY(EditDefaultsOnly, Category="Mario|Roll")
	float RollStartForceInputTime = 0.18f; // 롤 시작 직후 짧게 전진 입력을 강제(시작 모션에 추진력 부여)

	float RollStartForceInputRemaining = 0.f; // 시작 강제 추진 타이머
	
	FTimerHandle RollPhaseTimer;
	
	float DefaultGroundFriction = 8.f; // 이동값 캐싱 
	float DefaultBrakingDecelWalking = 2048.f;
	float DefaultBrakingFrictionFactor = 2.f;

	FVector2D CachedMoveInput = FVector2D::ZeroVector;// move 입력 저장
	
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
	float PoundJumpZVelocity = 1200.f; //반동점프 크기
	
	FTimerHandle GroundPoundPrepareTimer;
	FTimerHandle GroundPoundStunTimer;
	
	// 다이브
	UPROPERTY(BlueprintReadOnly, Category="State|Dive", meta=(AllowPrivateAccess="true"))
	bool bIsDiving = false;

	UPROPERTY(EditDefaultsOnly, Category="Mario|Dive")
	float Dive_ForwardStrength = 1100.f;

	UPROPERTY(EditDefaultsOnly, Category="Mario|Dive")
	float Dive_UpStrength = 450.f;
	
	float DefaultAirControl = 0.8f;
	float DefaultMaxAcceleration = 2048.f;
	float DefaultBrakingDecelFalling = 0.f;
	bool  bDefaultOrientRotationToMovement = true;
	
	FVector StoredPoundFacingDir = FVector::ForwardVector;
	bool bHasStoredPoundFacingDir = false;
	
	// 내리막길 부스트, 턴
	UPROPERTY(EditDefaultsOnly, Category="Mario|SlopeBoost")
	float MinSlopeAngleDeg = 12.f;          // 이 각도 이상이면 내리막

	UPROPERTY(EditDefaultsOnly, Category="Mario|SlopeBoost")
	float BoostMaxAddSpeed = 400.f;         // 기본 RunSpeed + 최대 추가 속도

	UPROPERTY(EditDefaultsOnly, Category="Mario|SlopeBoost")
	float BoostRiseSpeed = 1.5f;             // 부스트 올라가는 보간 속도

	UPROPERTY(EditDefaultsOnly, Category="Mario|SlopeBoost")
	float BoostDecaySpeed = 1.f;            // 부스트 내려가는 보간 속도

	UPROPERTY(EditDefaultsOnly, Category="Mario|SlopeBoost")
	float BoostHoldTime = 2.35f;            // 부스트 시간 유지(부스트 어느정도 유지)

	UPROPERTY(EditDefaultsOnly, Category="Mario|SlopeBoost")
	float DownhillDotThreshold = 0.55f;     // 내리막 방향과 속도 방향 조건

	UPROPERTY(EditDefaultsOnly, Category="Mario|SlopeBoost")
	float MinSpeedForBoost = 250.f;         // 가속 진입 최소 속도

	// animbp용
	UPROPERTY(BlueprintReadOnly, Category="State|SlopeBoost", meta=(AllowPrivateAccess="true"))
	bool bIsDownhillBoosting = false;

	UPROPERTY(BlueprintReadOnly, Category="State|SlopeBoost", meta=(AllowPrivateAccess="true"))
	float DownhillBoostAlpha = 0.f;         // 0~1

	float DownhillHoldRemaining = 0.f;
	
	//HP
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mario|HP", meta=(AllowPrivateAccess="true"))
	float MaxHP = 3.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Mario|HP", meta=(AllowPrivateAccess="true"))
	float CurrentHP = 3.f;

	UPROPERTY(BlueprintReadOnly, Category="Mario|HP", meta=(AllowPrivateAccess="true"))
	bool bGameOver = false;

	UFUNCTION(BlueprintCallable, Category="Mario|HP")
	float GetHP() const { return CurrentHP; }

	UFUNCTION(BlueprintCallable, Category="Mario|HP")
	bool IsGameOver() const { return bGameOver; }
	
	UPROPERTY(EditDefaultsOnly, Category="Mario|Combat") // 피격 스턴
	float HitStunSeconds = 2.57f;
	
	UPROPERTY(BlueprintReadOnly, Category="Mario|Combat", meta=(AllowPrivateAccess="true")) // 피격 스턴 animbp
	bool bHitStun = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Mario|Checkpoint", meta=(AllowPrivateAccess="true"))
	bool bHasCheckpoint = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Mario|Checkpoint", meta=(AllowPrivateAccess="true"))
	FTransform SavedCheckpointTransform = FTransform::Identity;
	
	// 플레이어가 몬스터와 닿았을 때(블로킹) 데미지/넉백
	UPROPERTY(EditDefaultsOnly, Category="Mario|Combat")
	float ContactDamage = 1.f;

	UPROPERTY(EditDefaultsOnly, Category="Mario|Combat")
	float ContactKnockbackXY = 700.f;

	UPROPERTY(EditDefaultsOnly, Category="Mario|Combat")
	float ContactKnockbackZ = 320.f;

	UPROPERTY(EditDefaultsOnly, Category="Mario|Combat")
	float ContactDamageCooldown = 0.55f;

	float LastContactDamageTime = -1000.f;
	
	
	//인풋 콜백
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void OnCrouchPressed(const FInputActionValue& Value);
	void OnCrouchReleased(const FInputActionValue& Value);
	void UpdateDownhillBoost(float DeltaSeconds);
	void ForceDisableDownhillBoost(float DeltaSeconds);
	
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
	void ThrowCap();// 캐피 액션 모자 던지기
	UFUNCTION()
	void OnCapDestroyed(AActor* DestroyedActor);
	// 구르기
	bool CanStartRoll() const;
	FVector ComputeRollDirection() const;
	void StartRoll();
	void EnterRollLoop();
	void BeginRollEnd();
	void FinishRoll();    // 정상 종료(End 애니 포함)
	void AbortRoll(bool bForceStandUp);     // 즉시 종료
	void EndRollInternal(bool bForceStandUp);
	
	//wall action
	bool IsWallActionCandidate() const;
	bool TryGetWallHitFromDetector(const FHitResult& SweepResult, FHitResult& OutHit) const;
	bool PassesWallFilters(const FHitResult& WallHit) const;

	void StartWallSlide(const FHitResult& WallHit);
	void EnterWallSlideLoop();
	void ResetWallSlide();
	
	void ApplyWallSlideMovementSettings(bool bEnable);
	void UpdateWallSlidePhysics(float DeltaTime);
	
	void ExecuteWallKick();
	void EndWallKickState();
	
	void BeginWallKickInputLock();
	void EndWallKickInputLock();
	void CancelWallKickForSlideEntry();
	
	void TryEnterWallSlideFromCurrentOverlap();
	
	// EndOverlap 이후 유예시간 뒤 실제로 벽이 떨어졌는지 재검증
	void ConfirmWallContactAfterEndOverlap();
	
	void FaceDirection2D(const FVector& Dir);
	void FaceWallFromNormal(const FVector& WallNormal);
	void FaceAwayFromNormal(const FVector& WallNormal);
	//헬퍼
	void ApplyMoveSpeed();
	float GetBaseMoveSpeed() const; // 
public:	
	virtual void Tick(float DeltaTime) override;
	
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
							 class AController* EventInstigator, AActor* DamageCauser) override;
	//캡쳐 카메라 세팅
	virtual FRotator GetViewRotation() const override;
	void SetCaptureControlRotationOverride(bool bEnable);
	void SetCaptureControlRotation(const FRotator& InRot);
	void CaptureFeedLookInput(const FInputActionValue& Value);
	void CaptureSyncTargetControlRotation(const FRotator& InRot);
	void CaptureTickCamera(float DeltaTime, APlayerController* PC);
	
private:
	FMarioState State;

	bool IsMonsterActor(AActor* OtherActor, UPrimitiveComponent* OtherComp) const;
	
	void OnThrowCapReleased();
	
	//캡쳐 카메라 세팅
	bool bCaptureControlRotOverride = false;
	FRotator CaptureControlRot = FRotator::ZeroRotator;
	
	//마리오 피격 스턴
	bool bInputLocked = false;
	FTimerHandle HitStunTimer;
	void ClearHitStun();
};
