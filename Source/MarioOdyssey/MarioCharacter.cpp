// Fill out your copyright notice in the Description page of Project Settings.


#include "MarioCharacter.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"


AMarioCharacter::AMarioCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	//캐릭터 회전
	bUseControllerRotationYaw = false; //컨트롤러 yaw를 직접 따라가지 않음
	GetCharacterMovement()->bOrientRotationToMovement = true; // 이동방향으로 캐릭터가 회전
	GetCharacterMovement()->RotationRate = FRotator(0.0f, TurnRate, 0.0f);
	
	//이동
	GetCharacterMovement()->AirControl = 0.8f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2048.f;
	
	GetCharacterMovement()->GravityScale  = DefaultGravityScale;//점프 중력 스케일
	//카메라
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(RootComponent);
	SpringArm->TargetArmLength = 450.f;
	SpringArm->bUsePawnControlRotation = true;
	
	SpringArm->bInheritPitch = true;
	SpringArm->bInheritYaw   = true;
	SpringArm->bInheritRoll  = false;
	
	//카메라 지연 끄기
	SpringArm->bEnableCameraLag = false;
	SpringArm->bEnableCameraRotationLag = false;
	
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(SpringArm);
	FollowCamera->bUsePawnControlRotation = false;
	
	//웅크리기
	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
	GetCharacterMovement()->SetCrouchedHalfHeight(35.0f);
	ApplyMoveSpeed();
}

void AMarioCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	if (SpringArm)
	{
		SpringArmTargetOffset_Default = SpringArm->TargetOffset;
	}
	
	//인핸스 인풋 매핑 추가
	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		PC->PlayerCameraManager->ViewPitchMin = -60.f;
		PC->PlayerCameraManager->ViewPitchMax =  35.f;
		
		TargetControlRotation = PC->GetControlRotation();
		
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
			{
				if (IMC_Player)
				{
					Subsystem->AddMappingContext(IMC_Player, 0);
				}
			}
		}
	}
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		DefaultAirControl = MoveComp->AirControl;
		DefaultMaxAcceleration = MoveComp->MaxAcceleration;
		DefaultBrakingDecelFalling = MoveComp->BrakingDecelerationFalling;
		bDefaultOrientRotationToMovement = MoveComp->bOrientRotationToMovement;
		DefaultGroundFriction = MoveComp->GroundFriction;
		DefaultBrakingDecelWalking = MoveComp->BrakingDecelerationWalking;
		DefaultBrakingFrictionFactor = MoveComp->BrakingFrictionFactor;

	}
}

void AMarioCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	EndDive();
	//엉덩방아
	if (bIsGroundPounding)
	{
		bIsGroundPounding = false;
		bGroundPoundStunned = true;
		bWaitingForPoundJump = true;
		bPoundJumpConsumed = false;
		State.GroundPound = EGroundPoundPhase::Stunned;
		State.bPoundJumpWindowOpen = true;
		State.bPoundJumpConsumed = false;
		JumpStage = 0;
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		
		//TODO: 착지 애니메이션 / 충격파
		
		GetWorld()->GetTimerManager().SetTimer(
			GroundPoundStunTimer, this, &AMarioCharacter::EndGroundPoundStun,
			GroundPoundStunTime, false);
	}
	else
	{
		bGroundPoundUsed = false;
		EndPoundJumpWindow();
	}
	
	if (bIsGroundPoundPreparing) // preparing 상태에서 착지가 되버릴시 이동 영구 막힘 방지
	{
		bIsGroundPoundPreparing = false;
		bGroundPoundUsed = false;

		GetWorld()->GetTimerManager().ClearTimer(GroundPoundPrepareTimer);

		if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
		{
			MoveComp->GravityScale = DefaultGravityScale;
		}

		State.GroundPound = EGroundPoundPhase::None;
	}
	
	LastLandedTime = GetWorld()->GetTimeSeconds(); // 마지막 착지 시간 불러오기
	
	if (JumpStage >= 3)
	{
		JumpStage = 0;
	}
	
	bIsLongJumping = false;
	bIsBackflipping = false;
	
	ApplyMoveSpeed();
}


void AMarioCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bIsRolling)
{
    UCharacterMovementComponent* MoveComp = GetCharacterMovement();
    if (!MoveComp)
    {
        return;
    }

    // 떨어지면 즉시 Abort
    if (!MoveComp->IsMovingOnGround())
    {
        AbortRoll(false);
    }
    else
    {
        // 롤 중 속도 상한은 RollSpeed
        MoveComp->MaxWalkSpeed = RollSpeed;
    	MoveComp->MaxWalkSpeedCrouched = RollSpeed;
        // 현재 입력 기반
        FVector DesiredDir = RollDirection;

        const FVector2D Input = CachedMoveInput;
        const float InputMag = Input.Size();

        if (Controller && InputMag > 0.1f)
        {
            const FRotator ControlRot = Controller->GetControlRotation();
            const FRotator YawRot(0.f, ControlRot.Yaw, 0.f);

            const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
            const FVector Right   = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);

            DesiredDir = (Right * Input.X + Forward * Input.Y);
            DesiredDir.Z = 0.f;
            DesiredDir.Normalize();
        }

        // 반대 입력이면 RollEnd로 전이 
        if (RollPhase == ERollPhase::Loop && InputMag > 0.1f)
        {
            const float Dot = FVector::DotProduct(RollDirection, DesiredDir);
            if (Dot <= RollReverseCancelDot)
            {
                BeginRollEnd();
            }
        }

        // 방향을 부드럽게 갱신
        if (RollPhase != ERollPhase::End && InputMag > 0.1f)
        {
            RollDirection = FMath::VInterpTo(RollDirection, DesiredDir, DeltaTime, RollSteerInterpSpeed);
            RollDirection.Z = 0.f;
            RollDirection.Normalize();
        }

        // 캐릭터 회전도 롤 방향으로
        SetActorRotation(RollDirection.Rotation());
    	
        //    StartForceTime 동안은 입력 없어도 추진, 이후엔 입력 있을 때만 추진.
        bool bDrive = false;

        if (RollPhase != ERollPhase::End)
        {
            if (RollStartForceInputRemaining > 0.f)
            {
                RollStartForceInputRemaining -= DeltaTime;
                bDrive = true;
            }
            else if (InputMag > 0.1f)
            {
                bDrive = true;
            }
        }

        if (bDrive)
        {
            AddMovementInput(RollDirection, 1.0f);
        }

        // 속도가 충분히 떨어지면 자동으로 End 진입
        const float Speed2D = MoveComp->Velocity.Size2D();
        if (RollPhase == ERollPhase::Loop && Speed2D <= RollEndSpeed2D)
        {
            BeginRollEnd();
        }
    }
}
	if (Controller) // 카메라 위치에 따른 보간(웅크리기)
	{
		const FRotator Current = Controller->GetControlRotation();
		const FRotator Smoothed = FMath::RInterpTo(
			Current,
			TargetControlRotation,
			DeltaTime,
			CameraRotationInterpSpeed
		);

		Controller->SetControlRotation(Smoothed);
	}
	if (!bIsRolling)
	{
		UpdateDownhillBoost(DeltaTime);
	}
}

void AMarioCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC) return;
	
	//Move, Look
	if (IA_Move) EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &AMarioCharacter::Move);
	if (IA_Look) EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AMarioCharacter::Look);
	
	//Jump
	if (IA_Jump) EIC->BindAction(IA_Jump, ETriggerEvent::Started, this, &AMarioCharacter::OnJumpPressed);
	
	//crouch
	if (IA_Crouch)
	{
		EIC->BindAction(IA_Crouch, ETriggerEvent::Started,   this, &AMarioCharacter::OnCrouchPressed);
		EIC->BindAction(IA_Crouch, ETriggerEvent::Completed, this, &AMarioCharacter::OnCrouchReleased);
		EIC->BindAction(IA_Crouch, ETriggerEvent::Canceled,  this, &AMarioCharacter::OnCrouchReleased);
	}
	
	//Run
	if (IA_Run)
	{
		EIC->BindAction(IA_Run, ETriggerEvent::Started, this, &AMarioCharacter::OnRunPressed);
		EIC->BindAction(IA_Run, ETriggerEvent::Completed, this, &AMarioCharacter::OnRunReleased);
	}
}

void AMarioCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	if (SpringArm)
	{
		FVector Offset = SpringArmTargetOffset_Default;
		Offset.Z += ScaledHalfHeightAdjust;   // 스케일 반영된 값 사용
		SpringArm->TargetOffset = Offset;
	}
	bAnimIsCrouched = true;
	ApplyMoveSpeed();
}

void AMarioCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	if (SpringArm)
	{
		SpringArm->TargetOffset = SpringArmTargetOffset_Default;
	}
	bAnimIsCrouched = false; 
	ApplyMoveSpeed();
}


void AMarioCharacter::Move(const FInputActionValue& Value) // 이동
{
	if (!State.CanMove())
		return;
	if (bIsDiving)
	{
		return;
	}
	const FVector2D Input = Value.Get<FVector2D>();
	CachedMoveInput = Input;
	
	if (bIsRolling) return;
	if (!Controller) return;
	
	const FRotator ControlRot = Controller->GetControlRotation();
	const FRotator YawRot(0.f, ControlRot.Yaw, 0.f);
	
	const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
	
	AddMovementInput(Right, Input.X);
	AddMovementInput(Forward, Input.Y);
}

void AMarioCharacter::Look(const FInputActionValue& Value) // 카메라
{
	const FVector2D LookAxis = Value.Get<FVector2D>();
	
	TargetControlRotation.Yaw   += LookAxis.X;
	TargetControlRotation.Pitch += LookAxis.Y;
}

void AMarioCharacter::OnCrouchPressed(const FInputActionValue& Value)
{
	if (!GetCharacterMovement()) return;

	const bool bOnGround = GetCharacterMovement()->IsMovingOnGround();

	if (bOnGround)
	{
		// GroundPound 착지후에는 다이브 불가
		if (bGroundPoundStunned || bWaitingForPoundJump)
		{
			return;
		}

		BeginCrouchHold();
	}
	else
	{
		// 이미 GroundPound 진행 중(Preparing/Pounding)이라면 C 재입력 = Dive
		if (bIsGroundPoundPreparing || bIsGroundPounding)
		{
			StartDiveFromCurrentContext();
			return;
		}
		// 공중이면 기존 GroundPound 로직 재사용
		OnGroundPoundPressed();
	}
}

void AMarioCharacter::OnCrouchReleased(const FInputActionValue& Value)
{
	// 물리적 홀드 상태는 언제든 해제 반영
	bCrouchHeld = false;
	
	if (bIsRolling)
		return;
	
	// 지상에서 실제 웅크림 상태였다면만 해제
	if (GetCharacterMovement() && GetCharacterMovement()->IsMovingOnGround() && bIsCrouched)
	{
		UnCrouch();
		ApplyMoveSpeed();
	}
}

void AMarioCharacter::UpdateDownhillBoost(float DeltaSeconds)
{
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
    if (!MoveComp) return;

    // 특수 상태에서는 부스트 금지
    if (bIsDiving) { ForceDisableDownhillBoost(DeltaSeconds); return; }
    if (!State.CanMove()) { ForceDisableDownhillBoost(DeltaSeconds); return; }
	if (bIsRolling) { ForceDisableDownhillBoost(DeltaSeconds); return; }
	if (bIsCrouched || bCrouchHeld)
	{
		ForceDisableDownhillBoost(DeltaSeconds);
		return;
	}
	
    // 지상에서
    if (!MoveComp->IsMovingOnGround())
    {
        ForceDisableDownhillBoost(DeltaSeconds);
        return;
    }
	// 달리기 아닐 때: 부스트를 서서히 꺼서 WalkSpeed로 복귀
	if (!bIsRunning)
	{
		DownhillHoldRemaining = 0.f;
		DownhillBoostAlpha = FMath::FInterpTo(DownhillBoostAlpha, 0.f, DeltaSeconds, BoostDecaySpeed);
		bIsDownhillBoosting = (DownhillBoostAlpha > 0.05f);

		const float BaselineSpeed = GetBaseMoveSpeed(); // 여기선 WalkSpeed(또는 crouch)
		MoveComp->MaxWalkSpeed = BaselineSpeed + BoostMaxAddSpeed * DownhillBoostAlpha; // 서서히 감소
		return;
	}
    const FVector Vel2D(MoveComp->Velocity.X, MoveComp->Velocity.Y, 0.f);
    const float Speed2D = Vel2D.Size();
    if (Speed2D < MinSpeedForBoost)
    {
        ForceDisableDownhillBoost(DeltaSeconds);
        return;
    }

    const FFindFloorResult& Floor = MoveComp->CurrentFloor;
    if (!Floor.IsWalkableFloor())
    {
        ForceDisableDownhillBoost(DeltaSeconds);
        return;
    }

    const FVector FloorNormal = Floor.HitResult.ImpactNormal.GetSafeNormal();
    const FVector Up = FVector::UpVector;

    // 경사 각도
    const float CosAngle = FVector::DotProduct(FloorNormal, Up);
    const float SlopeAngleDeg = FMath::RadiansToDegrees(
        FMath::Acos(FMath::Clamp(CosAngle, -1.f, 1.f)));

    const bool bSlopeEnough = (SlopeAngleDeg >= MinSlopeAngleDeg);

    // 내리막 방향(중력 방향을 바닥 평면에 투영)
    const FVector GravityDir = -Up;
    FVector DownhillDir = GravityDir - FVector::DotProduct(GravityDir, FloorNormal) * FloorNormal;
    if (!DownhillDir.Normalize())
    {
        ForceDisableDownhillBoost(DeltaSeconds);
        return;
    }

    const FVector VelDir = Vel2D.GetSafeNormal();
    FVector Downhill2D(DownhillDir.X, DownhillDir.Y, 0.f);
    if (!Downhill2D.Normalize())
    {
        ForceDisableDownhillBoost(DeltaSeconds);
        return;
    }

    const float DownhillDot = FVector::DotProduct(VelDir, Downhill2D);
	const bool bIsRunningDownhill = bIsRunning && bSlopeEnough && (DownhillDot >= DownhillDotThreshold);

    // 유지 시간
    if (bIsRunningDownhill)
    {
        DownhillHoldRemaining = BoostHoldTime;
    }
    else
    {
        DownhillHoldRemaining = FMath::Max(0.f, DownhillHoldRemaining - DeltaSeconds);
    }

    const bool bShouldBoost = (bIsRunningDownhill || DownhillHoldRemaining > 0.f);

    // 보간
    const float TargetAlpha = bShouldBoost ? 1.f : 0.f;
    const float InterpSpeed = bShouldBoost ? BoostRiseSpeed : BoostDecaySpeed;
    DownhillBoostAlpha = FMath::FInterpTo(DownhillBoostAlpha, TargetAlpha, DeltaSeconds, InterpSpeed);

    bIsDownhillBoosting = (DownhillBoostAlpha > 0.05f);

    // 속도 적용: 기본 속도 + 추가 속도
	const float BaselineSpeed = GetBaseMoveSpeed();
	const float TargetMaxSpeed = BaselineSpeed + BoostMaxAddSpeed * DownhillBoostAlpha;

    // 여기서 직접 MaxWalkSpeed
    MoveComp->MaxWalkSpeed = TargetMaxSpeed;
}

void AMarioCharacter::ForceDisableDownhillBoost(float DeltaSeconds)
{
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (!MoveComp) return;

	DownhillHoldRemaining = 0.f;
	DownhillBoostAlpha = FMath::FInterpTo(DownhillBoostAlpha, 0.f, DeltaSeconds, BoostDecaySpeed);
	bIsDownhillBoosting = (DownhillBoostAlpha > 0.05f);

	const float BaselineSpeed = GetBaseMoveSpeed();
	MoveComp->MaxWalkSpeed = BaselineSpeed + BoostMaxAddSpeed * DownhillBoostAlpha;
}


void AMarioCharacter::OnJumpPressed() // 3단 점프, 반동 점프 구현
{
	if (bIsRolling)
	{
		AbortRoll(true);
	}
	if (GetCharacterMovement()->IsFalling())
	{
		return;
	}
	
	bIsPoundJumping = false;
	if (bIsGroundPoundPreparing || bIsGroundPounding )
		return;
	
	if (bWaitingForPoundJump)
	{
		if (bPoundJumpConsumed)
			return;

		bPoundJumpConsumed = true;
		bWaitingForPoundJump = false;
		bIsPoundJumping = true;
		bGroundPoundStunned = false;
		bGroundPoundUsed = false;
		
		State.GroundPound = EGroundPoundPhase::None;
		State.bPoundJumpWindowOpen = false;
		State.bPoundJumpConsumed = true;
		
		GetWorld()->GetTimerManager().ClearTimer(GroundPoundStunTimer);

		LaunchCharacter(FVector(0.f, 0.f, PoundJumpZVelocity), true, true);
		return;
	}
	
	if (bGroundPoundStunned)
		return;
	
	TryCrouchDerivedJump();
	
	if (bIsLongJumping || bIsBackflipping)
		return;
	
	const float CurrentTime = GetWorld()->GetTimeSeconds();
	
	const bool bCanChain = (LastLandedTime > 0.f) && (CurrentTime - LastLandedTime <= JumpChainTime);
	
	if (!bCanChain)
	{
		JumpStage = 1;
	}
	else
	{
		JumpStage++;
	}
	
	JumpStage = FMath::Clamp(JumpStage,1 ,3); // 점프 최소값 최댓값 제한
	
	switch (JumpStage)
	{
	case 1:
		GetCharacterMovement()->JumpZVelocity = JumpZ_Stage1;
		break;
	case 2:
		GetCharacterMovement()->JumpZVelocity = JumpZ_Stage2;
		break;
	case 3:
		GetCharacterMovement()->JumpZVelocity = JumpZ_Stage3;
		break;
	}
	
	Jump();
	
}

void AMarioCharacter::OnRunPressed()
{
	if (CanStartRoll())
	{
		StartRoll();
		return;
	}
	bIsRunning = true;
	ApplyMoveSpeed();
}

void AMarioCharacter::OnRunReleased()
{
	if (bIsRolling)
		return;
	bIsRunning = false;
	ApplyMoveSpeed();
}

// 엉덩방아
void AMarioCharacter::OnGroundPoundPressed()
{
	if (bIsRolling) return;
	if (GetCharacterMovement()->IsMovingOnGround())
	{
		return;
	}
	if (bIsLongJumping)
	{
		return;
	}
	if (!GetCharacterMovement()->IsFalling()) //공중 일때 가능
		return;
	
	if (bGroundPoundUsed) // 이미 사용했다면 무시
		return;
	
	bGroundPoundUsed = true;
	bIsGroundPoundPreparing = true;
	State.GroundPound = EGroundPoundPhase::Preparing;
	
	// GroundPound 시작 시점의 “바라보던 방향” 저장(캐릭터 기반)
	
	StoredPoundFacingDir = GetActorForwardVector().GetSafeNormal();
	bHasStoredPoundFacingDir = true;
	
	
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->GravityScale = 0.f;
	GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	//TODO  :  공중제비 애니메이션 재생
	
	GetWorld()->GetTimerManager().SetTimer(GroundPoundPrepareTimer, this, &AMarioCharacter::StartGroundPound, GroundPoundPrepareTime, false);
}

void AMarioCharacter::BeginCrouchHold()
{
	bCrouchHeld = true;
	
	CrouchStartSpeed2D = GetVelocity().Size2D();
	
	if (!bIsCrouched)
	{
		Crouch();
	}
	
	ApplyMoveSpeed();
}

void AMarioCharacter::EndCrouchHold()
{
	bCrouchHeld = false;
	
	CrouchStartSpeed2D = 0.f;
	
	if (bIsCrouched)
	{
		UnCrouch();
	}
	
	ApplyMoveSpeed();
}

void AMarioCharacter::TryCrouchDerivedJump()
{
	if (!GetCharacterMovement()) return;

	// 지상에서만
	if (!GetCharacterMovement()->IsMovingOnGround())
		return;

	// 웅크리기 의도(홀드 또는 실제 crouched)
	if (!(bIsCrouched || bCrouchHeld))
		return;

	const float Speed2D = FMath::Max(CrouchStartSpeed2D, GetVelocity().Size2D());

	if (Speed2D >= LongJump_MinSpeed2D) DoLongJump();
	else DoBackflip();
	
	CrouchStartSpeed2D = 0.f;
}

void AMarioCharacter::DoLongJump()
{
	// 웅크리기 일단 해제
	if (bIsCrouched)
	{
		UnCrouch();
	}

	bIsLongJumping = true;
	bIsBackflipping = false;

	// 3단 점프 체인 끊기(롱점프는 별도 기술)
	JumpStage = 0;
	LastLandedTime = 0.f;

	const FVector Forward = GetActorForwardVector();
	const FVector LaunchVel = (Forward * LongJump_ForwardStrength) + FVector(0.f, 0.f, LongJump_UpStrength);

	LaunchCharacter(LaunchVel, true, true);	
}

void AMarioCharacter::DoBackflip()
{
	if (bIsCrouched)
	{
		UnCrouch();
	}

	bIsBackflipping = true;
	bIsLongJumping = false;

	JumpStage = 0;
	LastLandedTime = 0.f;

	const FVector Back = -GetActorForwardVector();
	const FVector LaunchVel = (Back * Backflip_BackStrength) + FVector(0.f, 0.f, Backflip_UpStrength);

	LaunchCharacter(LaunchVel, true, true);
}

void AMarioCharacter::StartDiveFromCurrentContext()
{
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (!MoveComp) return;
	
	if (bGroundPoundStunned || bWaitingForPoundJump) // 스턴, 반동중에는 다이브 금지
		return;
	
	// 이미 다이브 중이면 무시
	if (bIsDiving) return;

	// 사용할 방향 결정(저장된 방향 우선)
	FVector DiveDir = bHasStoredPoundFacingDir ? StoredPoundFacingDir : GetActorForwardVector();
	DiveDir = DiveDir.GetSafeNormal();
	if (DiveDir.IsNearlyZero())
	{
		DiveDir = GetActorForwardVector().GetSafeNormal();
	}

	// GroundPound 관련 상태 강제 종료
	bIsGroundPoundPreparing = false;
	bIsGroundPounding = false;
	bGroundPoundStunned = false;

	// 반동점프 창도 닫기
	EndPoundJumpWindow();
	bWaitingForPoundJump = false;

	// 타이머 정리
	GetWorld()->GetTimerManager().ClearTimer(GroundPoundPrepareTimer);
	GetWorld()->GetTimerManager().ClearTimer(GroundPoundStunTimer);

	// State 동기화
	State.GroundPound = EGroundPoundPhase::None;
	State.AirAction = EMarioAirAction::Dive;

	// Dive 상태 on
	bIsDiving = true;
	//점프 연계 초기화
	JumpStage = 0;
	LastLandedTime = 0.f;
	// Prepare에서 0으로 만들었던 중력 복구
	MoveComp->GravityScale = DefaultGravityScale;

	// 다이브 방향 고정
	MoveComp->bOrientRotationToMovement = false;
	SetActorRotation(DiveDir.Rotation());

	// 다이브 중 공중조작 완전 차단
	MoveComp->AirControl = 0.0f;
	MoveComp->MaxAcceleration = 0.0f;
	MoveComp->BrakingDecelerationFalling = 0.0f;

	// 지상 스턴에서 발동하는 경우도 있으니, 다이브는 Falling으로 강제
	MoveComp->SetMovementMode(MOVE_Falling);

	// Launch
	const FVector LaunchVel = (DiveDir * Dive_ForwardStrength) + FVector(0.f, 0.f, Dive_UpStrength);
	LaunchCharacter(LaunchVel, true, true);
}

void AMarioCharacter::EndDive()
{
	if (!bIsDiving) return;

	bIsDiving = false;

	// State 정리
	if (State.AirAction == EMarioAirAction::Dive)
	{
		State.AirAction = EMarioAirAction::None;
	}

	// 이동/회전 원복
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->AirControl = DefaultAirControl;
		MoveComp->MaxAcceleration = DefaultMaxAcceleration;
		MoveComp->BrakingDecelerationFalling = DefaultBrakingDecelFalling;
		MoveComp->bOrientRotationToMovement = bDefaultOrientRotationToMovement;
	}
}

bool AMarioCharacter::CanStartRoll() const
{
	const UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (!MoveComp) return false;

	if (bIsRolling) return false;
	if (!MoveComp->IsMovingOnGround()) return false;

	// 웅크리기 상태에서만
	if (!bIsCrouched) return false;

	// 특수 상태에서는 금지
	if (!State.CanMove()) return false; // GroundPound Preparing/Pounding/Stunned 포함
	if (bIsDiving) return false;
	if (bIsLongJumping || bIsBackflipping || bIsPoundJumping) return false;
	if (bIsGroundPoundPreparing || bIsGroundPounding) return false;
	if (bGroundPoundStunned || bWaitingForPoundJump) return false;

	return true;
}

FVector AMarioCharacter::ComputeRollDirection() const
{
	FVector Dir = GetLastMovementInputVector();
	Dir.Z = 0.f;

	if (!Dir.Normalize())
	{
		Dir = GetActorForwardVector();
		Dir.Z = 0.f;
		Dir.Normalize();
	}
	return Dir;
}

void AMarioCharacter::StartRoll()
{
	bIsRolling = true;
	RollPhase = ERollPhase::Start;
	RollStartForceInputRemaining = RollStartForceInputTime;

	// 달리기 플래그는 롤이 소비
	bIsRunning = false;

	// 방향 초기화(입력 없으면 전방)
	RollDirection = ComputeRollDirection();

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		// 롤용 세팅
		DefaultMaxAcceleration = Move->MaxAcceleration;
		DefaultGroundFriction = Move->GroundFriction;
		DefaultBrakingDecelWalking = Move->BrakingDecelerationWalking;
		DefaultBrakingFrictionFactor = Move->BrakingFrictionFactor;
		bDefaultOrientRotationToMovement = Move->bOrientRotationToMovement;

		Move->bOrientRotationToMovement = false;
		Move->MaxWalkSpeed = RollSpeed;
		Move->MaxAcceleration = RollMaxAcceleration;

		// 관성 느낌
		Move->GroundFriction = RollGroundFriction;
		Move->BrakingDecelerationWalking = RollBrakingDecel;
		Move->BrakingFrictionFactor = 0.5f;
	}

	// Start -> Loop 타이머(혹은 AnimNotify로 대체 가능)
	GetWorldTimerManager().ClearTimer(RollPhaseTimer);
	GetWorldTimerManager().SetTimer(RollPhaseTimer, this, &AMarioCharacter::EnterRollLoop, RollStartDuration, false);
}

void AMarioCharacter::EnterRollLoop()
{
	if (!bIsRolling) return;

	RollPhase = ERollPhase::Loop;

	GetWorld()->GetTimerManager().ClearTimer(RollPhaseTimer);

	if (RollLoopDuration <= 0.f)
	{
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(
		RollPhaseTimer,
		this,
		&AMarioCharacter::BeginRollEnd,
		RollLoopDuration,
		false
	);
}

void AMarioCharacter::BeginRollEnd()
{
	if (!bIsRolling) return;

	RollPhase = ERollPhase::End;

	GetWorld()->GetTimerManager().ClearTimer(RollPhaseTimer);
	GetWorld()->GetTimerManager().SetTimer(
		RollPhaseTimer,
		this,
		&AMarioCharacter::FinishRoll,
		RollEndDuration,
		false
	);
}

void AMarioCharacter::FinishRoll()
{
	EndRollInternal(true);
}

void AMarioCharacter::AbortRoll(bool bForceStandUp)
{
	EndRollInternal(bForceStandUp);
}

void AMarioCharacter::StartGroundPound()
{
	bIsGroundPoundPreparing = false;
	bIsGroundPounding = true;
	State.GroundPound = EGroundPoundPhase::Pounding;
	
	//다시 낙하 상태
	GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	GetCharacterMovement()->GravityScale = DefaultGravityScale;
	//기존 속도 제거 후 강제 하강
	GetCharacterMovement()->Velocity =FVector::ZeroVector;
	
	LaunchCharacter(FVector(0.f, 0.f, -GroundPoundForce),true, true);
}

void AMarioCharacter::EndGroundPoundStun()
{
	bGroundPoundStunned = false;
	EndPoundJumpWindow();
	bGroundPoundUsed = false;
	State.GroundPound = EGroundPoundPhase::None;
}

void AMarioCharacter::EndPoundJumpWindow()
{
	bWaitingForPoundJump = false;
	bPoundJumpConsumed = false;
	State.bPoundJumpWindowOpen = false;
	State.bPoundJumpConsumed = false;
}

void AMarioCharacter::EndRollInternal(bool bForceStandUp)
{
	if (!bIsRolling) return;

	bIsRolling = false;
	RollPhase = ERollPhase::None;
	RollStartForceInputRemaining = 0.f;

	GetWorldTimerManager().ClearTimer(RollPhaseTimer);

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->bOrientRotationToMovement = bDefaultOrientRotationToMovement;
		Move->MaxAcceleration = DefaultMaxAcceleration;
		Move->GroundFriction = DefaultGroundFriction;
		Move->BrakingDecelerationWalking = DefaultBrakingDecelWalking;
		Move->BrakingFrictionFactor = DefaultBrakingFrictionFactor;
	}

	// 점프 캔슬 같은 경우엔 백점블링으로 새지 않게 강제로 스탠딩 처리
	if (bForceStandUp)
	{
		bCrouchHeld = false;
	}

	// 롤 종료 후 C를 누르고 있지 않다면 일어남
	if (!bCrouchHeld && bIsCrouched)
	{
		UnCrouch();
	}

	ApplyMoveSpeed();
}

void AMarioCharacter::ApplyMoveSpeed()
{
	if (bIsRolling)
		return;
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (!MoveComp)
		return;

	const float BaseSpeed = GetBaseMoveSpeed();

	MoveComp->MaxWalkSpeed = BaseSpeed;
	MoveComp->MaxWalkSpeedCrouched = BaseSpeed;
}

float AMarioCharacter::GetBaseMoveSpeed() const
{
	float Speed = bIsRunning ? RunSpeed : WalkSpeed;

	// 웅크리기(홀드) 포함해서 속도 낮춤
	if (bIsCrouched || bCrouchHeld)
	{
		Speed = WalkSpeed * CrouchSpeedScale;
	}

	return Speed;
}

