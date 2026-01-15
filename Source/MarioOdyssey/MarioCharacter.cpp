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
	
	GetCharacterMovement()->GravityScale  = 2.0f;//점프 중력 스케일
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
	GetCharacterMovement()->CrouchedHalfHeight = 35.0f;
	ApplyMoveSpeed();
}

void AMarioCharacter::BeginPlay()
{
	Super::BeginPlay();
	
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
}

void AMarioCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	
	//엉덩방아
	if (bIsGroundPounding)
	{
		bIsGroundPounding = false;
		bGroundPoundStunned = true;
		bWaitingForPoundJump = true;
		bPoundJumpConsumed = false;
		
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
	
	LastLandedTime = GetWorld()->GetTimeSeconds(); // 마지막 착지 시간 불러오기
	
	if (JumpStage >= 3)
	{
		JumpStage = 0;
	}
	
	bIsLongJumping = false;
	bIsBackflipping = false;
}


void AMarioCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (Controller)
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
	bAnimIsCrouched = true;
	ApplyMoveSpeed();
}

void AMarioCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	bAnimIsCrouched = false; 
	ApplyMoveSpeed();
}


void AMarioCharacter::Move(const FInputActionValue& Value) // 이동
{
	if (bIsGroundPoundPreparing || bIsGroundPounding || bGroundPoundStunned)
		return;
	
	const FVector2D Input = Value.Get<FVector2D>();
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
		// GroundPound 착지 직후(스턴/반동점프 창)에는 crouch로 상태 꼬이지 않게 막는 게 안전
		if (bGroundPoundStunned || bWaitingForPoundJump)
			return;

		BeginCrouchHold();
	}
	else
	{
		// 공중이면 기존 GroundPound 로직 재사용
		OnGroundPoundPressed();
	}
}

void AMarioCharacter::OnCrouchReleased(const FInputActionValue& Value)
{
	// 물리적 홀드 상태는 언제든 해제 반영
	bCrouchHeld = false;

	// 지상에서 실제 웅크림 상태였다면만 해제
	if (GetCharacterMovement() && GetCharacterMovement()->IsMovingOnGround() && bIsCrouched)
	{
		UnCrouch();
		ApplyMoveSpeed();
	}
}


void AMarioCharacter::OnJumpPressed() // 3단 점프, 반동 점프 구현
{
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
	bIsRunning = true;
	ApplyMoveSpeed();
}

void AMarioCharacter::OnRunReleased()
{
	bIsRunning = false;
	ApplyMoveSpeed();
}

// 엉덩방아
void AMarioCharacter::OnGroundPoundPressed()
{
	if (!GetCharacterMovement()->IsFalling()) //공중 일때 가능
		return;
	
	if (bGroundPoundUsed) // 이미 사용했다면 무시
		return;
	
	bGroundPoundUsed = true;
	bIsGroundPoundPreparing = true;
	
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

void AMarioCharacter::StartGroundPound()
{
	bIsGroundPoundPreparing = false;
	bIsGroundPounding = true;
	
	//다시 낙하 상태
	GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	GetCharacterMovement()->GravityScale = 2.0f;
	//기존 속도 제거 후 강제 하강
	GetCharacterMovement()->Velocity =FVector::ZeroVector;
	
	LaunchCharacter(FVector(0.f, 0.f, -GroundPoundForce),true, true);
}

void AMarioCharacter::EndGroundPoundStun()
{
	bGroundPoundStunned = false;
	EndPoundJumpWindow();
	bGroundPoundUsed = false;
	
}

void AMarioCharacter::EndPoundJumpWindow()
{
	bWaitingForPoundJump = false;
	bPoundJumpConsumed = false;
}

void AMarioCharacter::ApplyMoveSpeed()
{
	float Speed = bIsRunning ? RunSpeed : WalkSpeed;
	
	if (bIsCrouched || bCrouchHeld)
	{
		Speed = WalkSpeed * CrouchSpeedScale;
	}

	GetCharacterMovement()->MaxWalkSpeed = Speed;
}

