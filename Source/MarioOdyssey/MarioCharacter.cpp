// Fill out your copyright notice in the Description page of Project Settings.


#include "MarioCharacter.h"
#include "MarioCapProjectile.h"
#include "Capture/CaptureComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"

#include "Capture/CapturableInterface.h"
#include "Kismet/GameplayStatics.h"


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
	
	//캡쳐
	CaptureComp = CreateDefaultSubobject<UCaptureComponent>(TEXT("CaptureComp"));

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
	
	//wall, ledge 디텍터 컴포넌트 생성, 조절
	WallDetector = CreateDefaultSubobject<UBoxComponent>(TEXT("WallDetector"));
	WallDetector->SetupAttachment(GetCapsuleComponent());
	WallDetector->SetBoxExtent(FVector(5.f, 30.f, 45.f));
	WallDetector->SetRelativeLocation(FVector(40.f, 0.f, -5.f));
	WallDetector->SetCanEverAffectNavigation(false);

	LedgeDetector = CreateDefaultSubobject<UBoxComponent>(TEXT("LedgeDetector"));
	LedgeDetector->SetupAttachment(GetCapsuleComponent());
	LedgeDetector->SetBoxExtent(FVector(10.f, 45.f, 25.f));
	LedgeDetector->SetRelativeLocation(FVector(45.f, 0.f, 55.f));
	LedgeDetector->SetCanEverAffectNavigation(false);
	
	//웅크리기
	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;
	GetCharacterMovement()->SetCrouchedHalfHeight(35.0f);

	// Player_Capsule vs Monster_Capsule 이 Block 규칙이라 Overlap이 아니라 Hit로 반응해야 함
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetGenerateOverlapEvents(true);
		Capsule->SetNotifyRigidBodyCollision(true);
	}
	ApplyMoveSpeed();
}

void AMarioCharacter::OnCaptureBegin()
{
	//애니메이션(던지기/구르기/그라운드파운드)잔여물 정리
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		if (UAnimInstance* Anim = MeshComp->GetAnimInstance())
		{
			Anim->StopAllMontages(0.05f);
		}
	}
	
	//Roll 정리
	if (bIsRolling)
	{
		AbortRoll(true);
	}
	
	//Dive 정리
	EndDive();
	
	//GroundPound, 반동점프 정리
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GroundPoundPrepareTimer);
		World->GetTimerManager().ClearTimer(GroundPoundStunTimer);
	}
	bIsGroundPoundPreparing = false;
	bIsGroundPounding = false;
	bGroundPoundStunned = false;
	bIsPoundJumping = false;
	bGroundPoundUsed = false;
	EndPoundJumpWindow();
	State.GroundPound = EGroundPoundPhase::None;
	
	//Wall action 정리
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WallSlideStartToLoopTimer);
		World->GetTimerManager().ClearTimer(WallKickStateTimer);
		World->GetTimerManager().ClearTimer(WallKickInputLockTimer);
		World->GetTimerManager().ClearTimer(WallEndOverlapGraceTimer);
	}
	bWallOverlapping = false;
	bWallKickInputLocked = false;
	WallKickStartTime = -1000.f;
	if (WallActionState != EWallActionState::None)
	{
		ResetWallSlide();
	}
	else
	{
		CurrentWallNormal = FVector::ZeroVector;
	}
	
	//점프, 특수 점프 정리
	JumpStage = 0;
	LastLandedTime = -1.f;
	bIsLongJumping = false;
	bIsBackflipping = false;
	
	//슬로프 부스트 정리
	bIsDownhillBoosting = false;
	DownhillHoldRemaining = 0.f;
	DownhillBoostAlpha = 0.f;
	
	//캐시 정리
	CachedMoveInput = FVector2D::ZeroVector;
	
	//웅크리기, 달리기 정리
	bIsRunning = false;
	bCrouchHeld = false;
	CrouchStartSpeed2D = 0.f;
	if(bIsCrouched)
	{
		UnCrouch();
	}
	bAnimIsCrouched = false;
	
	//State 컨테이너 정리
	State = FMarioState{};
	
	//물리, 이동 원복
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->StopMovementImmediately();
		MoveComp->GravityScale = DefaultGravityScale;
		MoveComp->AirControl = DefaultAirControl;
		MoveComp->MaxAcceleration = DefaultMaxAcceleration;
		MoveComp->BrakingDecelerationFalling = DefaultBrakingDecelFalling;
		MoveComp->bOrientRotationToMovement = bDefaultOrientRotationToMovement;
		MoveComp->GroundFriction = DefaultGroundFriction;
		MoveComp->BrakingDecelerationWalking = DefaultBrakingDecelWalking;
		MoveComp->BrakingFrictionFactor = DefaultBrakingFrictionFactor;
	}
	ApplyMoveSpeed();
}

void AMarioCharacter::OnCaptureEnd()
{
	//언캡쳐 후, 마리오 조작 가능 상태 복귀
	if (bIsRolling) AbortRoll(true);
	EndDive();
	if (WallActionState != EWallActionState::None) ResetWallSlide();
	
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GroundPoundPrepareTimer);
		World->GetTimerManager().ClearTimer(GroundPoundStunTimer);
		World->GetTimerManager().ClearTimer(WallKickStateTimer);
		World->GetTimerManager().ClearTimer(WallKickInputLockTimer);
		World->GetTimerManager().ClearTimer(WallEndOverlapGraceTimer);
		World->GetTimerManager().ClearTimer(RollPhaseTimer);
	}
	bIsGroundPoundPreparing = false;
	bIsGroundPounding = false;
	bGroundPoundStunned = false;
	bIsPoundJumping = false;
	EndPoundJumpWindow();
	State.GroundPound = EGroundPoundPhase::None;
	
	bWallOverlapping = false;
	bWallKickInputLocked = false;
	WallKickStartTime = -1000.f;
	
	bIsRunning = false;
	bCrouchHeld = false;
	CrouchStartSpeed2D = 0.f;
	
	if (bIsCrouched)
	{
		UnCrouch();
	}
	bAnimIsCrouched = false;
	
	JumpStage = 0;
	LastLandedTime = -1.f;
	bIsLongJumping = false;
	bIsBackflipping = false;
	
	bIsDownhillBoosting = false;
	DownhillBoostAlpha = 0.f;
	DownhillHoldRemaining = 0.f;
	
	CachedMoveInput = FVector2D::ZeroVector;
	
	State = FMarioState{};
	
	//카메라 보간 타겟 컨트롤 로데이션 맞추기
	if (Controller)
	{
		TargetControlRotation = Controller->GetControlRotation();
	}
	
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->GravityScale = DefaultGravityScale;
		MoveComp->AirControl = DefaultAirControl;
		MoveComp->MaxAcceleration = DefaultMaxAcceleration;
		MoveComp->BrakingDecelerationFalling = DefaultBrakingDecelFalling;
		MoveComp->bOrientRotationToMovement = bDefaultOrientRotationToMovement;
		MoveComp->GroundFriction = DefaultGroundFriction;
		MoveComp->BrakingDecelerationWalking = DefaultBrakingDecelWalking;
		MoveComp->BrakingFrictionFactor = DefaultBrakingFrictionFactor;
	}
	
	ApplyMoveSpeed();
}

void AMarioCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	CurrentHP = MaxHP;
	
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
		DefaultGravityScale = MoveComp->GravityScale;
		DefaultAirControl = MoveComp->AirControl;
		DefaultMaxAcceleration = MoveComp->MaxAcceleration;
		DefaultBrakingDecelFalling = MoveComp->BrakingDecelerationFalling;
		bDefaultOrientRotationToMovement = MoveComp->bOrientRotationToMovement;
		DefaultGroundFriction = MoveComp->GroundFriction;
		DefaultBrakingDecelWalking = MoveComp->BrakingDecelerationWalking;
		DefaultBrakingFrictionFactor = MoveComp->BrakingFrictionFactor;
	}
	
	// wall, ledge 디텍터 델리게이트 바인딩
	if (WallDetector)
	{
		WallDetector->OnComponentBeginOverlap.AddDynamic(this, &AMarioCharacter::OnWallDetectorBeginOverlap);
		WallDetector->OnComponentEndOverlap.AddDynamic(this, &AMarioCharacter::OnWallDetectorEndOverlap);
	}

	if (LedgeDetector)
	{
		LedgeDetector->OnComponentBeginOverlap.AddDynamic(this, &AMarioCharacter::OnLedgeDetectorBeginOverlap);
		LedgeDetector->OnComponentEndOverlap.AddDynamic(this, &AMarioCharacter::OnLedgeDetectorEndOverlap);
	}

	// 몬스터 접촉(블로킹) 데미지/넉백: Capsule Hit로 처리
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->OnComponentHit.AddDynamic(this, &AMarioCharacter::OnMarioCapsuleHit);
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
	bIsPoundJumping = false;
	GetWorld()->GetTimerManager().ClearTimer(WallKickStateTimer);

	// 착지하면 Wall 관련은 무조건 Reset 함수로 정리
	if (WallActionState != EWallActionState::None)
	{
		ResetWallSlide(); // 여기서 bOrientRotationToMovement 원복
	}
	else
	{
		// 혹시라도 false로 남아있으면 착지 시 강제 원복
		if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
		{
			MoveComp->bOrientRotationToMovement = bDefaultOrientRotationToMovement;
		}
	}

	GetWorld()->GetTimerManager().ClearTimer(WallKickInputLockTimer);
	EndWallKickInputLock();
	
	ApplyMoveSpeed();
}

///디텍터 오버랩 콜백
void AMarioCharacter::OnWallDetectorBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this) return;
	bWallOverlapping = true;

	// EndOverlap 유예 타이머가 걸려있었다면, 다시 닿은 순간 취소
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(WallEndOverlapGraceTimer);
	}

	// 이미 슬라이드 상태면 StartWallSlide를 다시 호출하지 않음(타이머/회전 리셋 방지)
	if (WallActionState == EWallActionState::SlideStart || WallActionState == EWallActionState::SlideLoop)
	{
		return;
	}

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (!MoveComp)
	{
		return;
	}

	// “벽에 지상에서 닿은 상태 → 점프/낙하로 Falling 전환”은 BeginOverlap이 1번만 오고 끝나는 경우가 있음.
	// 그래서 Falling이 아니면 바로 버리지 말고 NextTick에 1회 시도(성공하면 Slide 진입, 아니면 그냥 return)
	if (!MoveComp->IsFalling())
	{
		if (WallActionState == EWallActionState::None && GetWorld())
		{
			GetWorld()->GetTimerManager().SetTimerForNextTick(this, &AMarioCharacter::TryEnterWallSlideFromCurrentOverlap);
		}
		return;
	}


	const float Now = GetWorld()->GetTimeSeconds();
	const bool bRecentlyWallKicked = (Now - WallKickStartTime) <= WallKickSlideReenterWindow;
	const bool bFromWallKick = (WallActionState == EWallActionState::WallKick) || bRecentlyWallKicked;

	// WallKick(또는 직후)면 연속 벽차기를 위해 Slide 진입 허용
	// 단, Kick 직후 즉시 같은 벽 재포획만 아주 짧게 방지
	if (bFromWallKick)
	{
		if ((Now - WallKickStartTime) < WallKickMinReenterDelay)
		{
			return;
		}
	}
	else
	{
		// WallKick이 아닌 경우는 기존처럼 None 상태에서만 진입 허용
		if (WallActionState != EWallActionState::None)
		{
			return;
		}

		if (!IsWallActionCandidate())
		{
			return;
		}
	}

	// 보조로 법선/정면/수직벽 판정
	FHitResult WallHit;
	if (!TryGetWallHitFromDetector(SweepResult, WallHit))
	{
		return;
	}

	if (!PassesWallFilters(WallHit))
	{
		return;
	}

	// 실제로 WallKick 상태/입력락이 남아있다면 정리 후 Slide로 전환
	if (WallActionState == EWallActionState::WallKick || bWallKickInputLocked)
	{
		CancelWallKickForSlideEntry();
	}

	StartWallSlide(WallHit);
}

void AMarioCharacter::OnWallDetectorEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!OtherActor || OtherActor == this) return;
	bWallOverlapping = false;

	// SlideStart/Loop 중 EndOverlap은 “진짜로 벽에서 떨어짐”일 수도 있고,
	// 회전 고정/박스 얇음/프레임 미세 분리로 “순간 EndOverlap”만 나는 경우도 있음.
	// 그래서 바로 Reset하지 말고, 짧게 유예 후 실제 벽 존재를 재검증한다.
	if (WallActionState == EWallActionState::SlideStart || WallActionState == EWallActionState::SlideLoop)
	{
		if (GetWorld())
		{
			GetWorld()->GetTimerManager().ClearTimer(WallEndOverlapGraceTimer);
			GetWorld()->GetTimerManager().SetTimer(
				WallEndOverlapGraceTimer, this, &AMarioCharacter::ConfirmWallContactAfterEndOverlap,
				WallEndOverlapGraceTime, false);
		}
		return;
	}
}

void AMarioCharacter::OnLedgeDetectorBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this) return;
	UE_LOG(LogTemp, Log, TEXT("[LedgeDetector] BeginOverlap: %s"), *OtherActor->GetName());
}

void AMarioCharacter::OnLedgeDetectorEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!OtherActor || OtherActor == this) return;
	UE_LOG(LogTemp, Log, TEXT("[LedgeDetector] EndOverlap: %s"), *OtherActor->GetName());
}
///

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
		UpdateWallSlidePhysics(DeltaTime);
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
	//Throw Cap
	if (IA_ThrowCap)
	{
		EIC->BindAction(IA_ThrowCap, ETriggerEvent::Started, this, &AMarioCharacter::ThrowCap);
		EIC->BindAction(IA_ThrowCap, ETriggerEvent::Completed, this, &AMarioCharacter::OnThrowCapReleased);
		EIC->BindAction(IA_ThrowCap, ETriggerEvent::Canceled,  this, &AMarioCharacter::OnThrowCapReleased);
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

float AMarioCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
	class AController* EventInstigator, AActor* DamageCauser)
{
	if (bGameOver) return 0.f;
	if (DamageAmount <= 0.f) return 0.f;

	CurrentHP = FMath::Clamp(CurrentHP - DamageAmount, 0.f, MaxHP);
	
	//여기서 피격 UI/사운드/무적시간
	UE_LOG(LogTemp, Warning, TEXT("[Mario] Damage=%.2f HP=%.2f/%.2f"), DamageAmount, CurrentHP, MaxHP);

	if (CurrentHP <= 0.f)
	{
		bGameOver = true;

		// HP=0일 때만 캡쳐 해제
		if (CaptureComp && CaptureComp->IsCapturing())
		{
			CaptureComp->ForceReleaseForGameOver();
		}

		// 최소 처리
		GetCharacterMovement()->StopMovementImmediately();
	}

	return DamageAmount;
}

void AMarioCharacter::OnMarioCapsuleHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	if (!OtherActor || OtherActor == this) return;
	if (bGameOver) return;
	if (CaptureComp && CaptureComp->IsCapturing()) return; // 캡쳐 중엔 마리오가 비활성/숨김 처리됨

	// cooldown: 캡슐이 계속 미끄러지면 Hit가 연속으로 뜨는 걸 방지
	if (UWorld* World = GetWorld())
	{
		const float Now = World->GetTimeSeconds();
		if (Now - LastContactDamageTime < ContactDamageCooldown)
		{
			return;
		}
		// 몬스터 판정: CapturableInterface(권장) or 태그(보조)
		const bool bIsMonster = OtherActor->GetClass()->ImplementsInterface(UCapturableInterface::StaticClass())
			|| OtherActor->ActorHasTag(FName(TEXT("Monster")))
			|| OtherActor->ActorHasTag(FName(TEXT("Capturable")));
		if (!bIsMonster)
		{
			return;
		}

		LastContactDamageTime = Now;
	}

	// 데미지는 UE Damage 파이프라인으로 통일(현재 TakeDamage override가 HP 처리)
	UGameplayStatics::ApplyDamage(
		this,
		ContactDamage,
		(OtherActor ? OtherActor->GetInstigatorController() : nullptr),
		OtherActor,
		UDamageType::StaticClass()
	);

	// 넉백: 몬스터로부터 멀어지는 방향
	FVector Away = (GetActorLocation() - OtherActor->GetActorLocation());
	Away.Z = 0.f;
	if (!Away.Normalize())
	{
		// 위치가 거의 같으면 Hit 노멀 기반
		Away = -Hit.Normal;
		Away.Z = 0.f;
		Away.Normalize();
	}
	const FVector LaunchVel = Away * ContactKnockbackXY + FVector(0.f, 0.f, ContactKnockbackZ);
	LaunchCharacter(LaunchVel, true, true);
}

void AMarioCharacter::OnThrowCapReleased()
{
	if (!ActiveCap) return;

	if (AMarioCapProjectile* Cap = Cast<AMarioCapProjectile>(ActiveCap))
	{
		Cap->NotifyHoldReleased();
	}
}


void AMarioCharacter::Move(const FInputActionValue& Value) // 이동
{
	if (WallActionState == EWallActionState::SlideStart || WallActionState == EWallActionState::SlideLoop)
	{
		return;
	}
	if (bWallKickInputLocked)
	{
		return;
	}
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
	
	if (WallActionState == EWallActionState::SlideStart || WallActionState == EWallActionState::SlideLoop)
	{
		ExecuteWallKick();
		return;
	}

	// WallKick 상태 중에는 일반 점프 로직으로 들어가지 않게 방지
	if (WallActionState == EWallActionState::WallKick)
	{
		return;
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
		GetWorld()->GetTimerManager().SetTimerForNextTick(this, &AMarioCharacter::TryEnterWallSlideFromCurrentOverlap);
		return;
	}
	
	if (bGroundPoundStunned)
		return;
	
	TryCrouchDerivedJump();
	
	if (bIsLongJumping || bIsBackflipping)
	{
		GetWorld()->GetTimerManager().SetTimerForNextTick(this, &AMarioCharacter::TryEnterWallSlideFromCurrentOverlap);
		return;
	}
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
	GetWorld()->GetTimerManager().SetTimerForNextTick(this, &AMarioCharacter::TryEnterWallSlideFromCurrentOverlap);
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

void AMarioCharacter::ThrowCap()
{
	if (ActiveCap) return;
	if (!CapProjectileClass) return;

	UWorld* World = GetWorld();
	if (!World) return;

	// 던지기 애니
	const bool bInAir = GetCharacterMovement() && GetCharacterMovement()->IsFalling();
	if (bInAir)
	{
		if (Montage_ThrowCap_Air) PlayAnimMontage(Montage_ThrowCap_Air);
	}
	else
	{
		if (Montage_ThrowCap_Ground) PlayAnimMontage(Montage_ThrowCap_Ground);
	}

	const FVector SpawnLoc = GetActorLocation() + GetActorForwardVector() * CapSpawnOffset.X
		+ GetActorRightVector() * CapSpawnOffset.Y
		+ FVector(0.f, 0.f, CapSpawnOffset.Z);

	const FRotator SpawnRot = GetActorRotation();

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;

	AMarioCapProjectile* Cap = World->SpawnActor<AMarioCapProjectile>(CapProjectileClass, SpawnLoc, SpawnRot, Params);
	if (!Cap) return;

	ActiveCap = Cap;
	Cap->OnDestroyed.AddDynamic(this, &AMarioCharacter::OnCapDestroyed);

	const FVector Dir = GetActorForwardVector();
	Cap->FireInDirection(Dir, CapThrowSpeed);
}

void AMarioCharacter::OnCapDestroyed(AActor* DestroyedActor)
{
	if (DestroyedActor && DestroyedActor == ActiveCap)
	{
		ActiveCap = nullptr;
		
		if (CaptureComp && CaptureComp->IsCapturing())
		{
			return;
		}
		
		// 잡는 애니
		const bool bInAir = GetCharacterMovement() && GetCharacterMovement()->IsFalling();
		if (bInAir)
		{
			if (Montage_CatchCap_Air) PlayAnimMontage(Montage_CatchCap_Air);
		}
		else
		{
			if (Montage_CatchCap_Ground) PlayAnimMontage(Montage_CatchCap_Ground);
		}
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
// wall action
bool AMarioCharacter::IsWallActionCandidate() const
{
	const UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (!MoveComp || !MoveComp->IsFalling())
	{
		return false;
	}

	// WallAction 대상: 일반 점프(1~3단) + 백덤블링 + 반동점프
	// 제외: 롱점프/다이브/그라운드파운드 계열/롤
	if (bIsLongJumping || bIsDiving || bIsRolling)
	{
		return false;
	}

	if (bIsGroundPoundPreparing || bIsGroundPounding || bGroundPoundStunned)
	{
		return false;
	}

	// 포함 백덤블링 / 반동점프
	if (bIsBackflipping || bIsPoundJumping)
	{
		return true;
	}

	// 포함 일반 점프(1~3단)
	return (JumpStage >= 1 && JumpStage <= 3);
}

bool AMarioCharacter::TryGetWallHitFromDetector(const FHitResult& SweepResult, FHitResult& OutHit) const
{
	// Overlap SweepResult에 노멀이 유효하게 들어오는 케이스 먼저 사용
	if (!SweepResult.ImpactNormal.IsNearlyZero() && SweepResult.bBlockingHit)
	{
		OutHit = SweepResult;
		return true;
	}

	if (!WallDetector || !GetWorld())
	{
		return false;
	}

	const FVector Start = WallDetector->GetComponentLocation();

	// 오버랩은 WorldStatic/WorldDynamic 대상으로 하고 있으므로, 스윕도 ObjectType 기반으로 맞춤.
	FCollisionObjectQueryParams ObjParams;
	ObjParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(WallSlideTrace), false, this);
	Params.AddIgnoredActor(this);

	const FCollisionShape Shape = FCollisionShape::MakeSphere(WallTraceRadius);

	// 다방향 시도: Vel2D(있으면) → Forward/Back → Right/Left
	TArray<FVector, TInlineAllocator<5>> Dirs;

	FVector Vel2D = GetVelocity();
	Vel2D.Z = 0.f;
	if (!Vel2D.IsNearlyZero())
	{
		Dirs.Add(Vel2D.GetSafeNormal());
	}

	FVector F = GetActorForwardVector(); F.Z = 0.f;
	if (!F.IsNearlyZero()) F.Normalize();

	FVector R = GetActorRightVector(); R.Z = 0.f;
	if (!R.IsNearlyZero()) R.Normalize();

	if (!F.IsNearlyZero())
	{
		Dirs.Add(F);
		Dirs.Add(-F);
	}
	if (!R.IsNearlyZero())
	{
		Dirs.Add(R);
		Dirs.Add(-R);
	}

	for (const FVector& Dir : Dirs)
	{
		if (Dir.IsNearlyZero()) continue;

		const FVector End = Start + Dir * WallTraceDistance;

		FHitResult Hit;
		const bool bHit = GetWorld()->SweepSingleByObjectType(
			Hit, Start, End, FQuat::Identity, ObjParams, Shape, Params);

		if (bHit && Hit.bBlockingHit)
		{
			OutHit = Hit;
			return true;
		}
	}

	return false;
}

bool AMarioCharacter::PassesWallFilters(const FHitResult& WallHit) const
{
	if (!WallHit.bBlockingHit)
	{
		return false;
	}

	// 수직 벽 필터: 70~110도 허용
	// Normal과 Up의 각도 = acos(Normal.Z)
	// 70~110도면 Normal.Z가 [-cos70, +cos70] 범위
	const float MaxAbsNormalZ = FMath::Cos(FMath::DegreesToRadians(70.f)); // 약 0.342
	const FVector N = WallHit.ImpactNormal.GetSafeNormal();
	if (FMath::Abs(N.Z) > MaxAbsNormalZ)
	{
		return false;
	}

	// "거의 정면" 접근 필터
	// Forward 기반보다 "수평 속도 방향"이 백덤블링/반동점프에서 더 안정적
	FVector Vel2D = GetVelocity();
	Vel2D.Z = 0.f;

	FVector ApproachDir = Vel2D.GetSafeNormal();
	if (ApproachDir.IsNearlyZero())
	{
		ApproachDir = GetActorForwardVector();
		ApproachDir.Z = 0.f;
		ApproachDir.Normalize();
	}

	FVector WallNormal2D = N;
	WallNormal2D.Z = 0.f;
	WallNormal2D = WallNormal2D.GetSafeNormal();

	if (WallNormal2D.IsNearlyZero())
	{
		return false;
	}

	const float FrontDot = FVector::DotProduct(ApproachDir, -WallNormal2D);
	if (FrontDot < WallMinFrontDot)
	{
		return false;
	}

	return true;
}

void AMarioCharacter::StartWallSlide(const FHitResult& WallHit)
{
	// 상태 진입 (AnimBP용)
	WallActionState = EWallActionState::SlideStart;

	// 캐시
	CurrentWallNormal = WallHit.ImpactNormal.GetSafeNormal();
	
	// Slide 중엔 정면을 벽 쪽으로 고정해야 함
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->bOrientRotationToMovement = false; // 이동방향 자동회전 OFF
	}

	// 벽 바라보게 즉시 회전 고정
	FaceWallFromNormal(CurrentWallNormal);
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		FVector V = MoveComp->Velocity;

		// 벽 노멀 방향 성분 제거(벽에서 밀려나거나 파고드는 속도 제거)
		FVector N = CurrentWallNormal;
		N.Z = 0.f;
		N = N.GetSafeNormal();

		if (!N.IsNearlyZero())
		{
			const float AlongN = FVector::DotProduct(V, N);
			V -= N * AlongN;
		}

		// 상승 중엔 그대로 둬서 점프 최고점까지 간다
		if (V.Z > 0.f)
		{
			MoveComp->GravityScale = DefaultGravityScale; // 정상 점프 감속 유지
			// V.Z 건드리지 않음
		}
		else
		{
			// 하강 구간부터만 가속 없는 일정 하강
			MoveComp->GravityScale = 0.f;
			V.Z = -WallSlideDownSpeed;
		}

		MoveComp->Velocity = V;
	}
	ApplyWallSlideMovementSettings(true);
	UpdateWallSlidePhysics(0.f); // 진입 순간에도 바로 속도 정리(튀는 값 방지)
	
	// 기존 공중기 플래그는 벽 상태로 "캔슬" 처리(애니 충돌 방지 목적)
	bIsLongJumping = false;

	// Start -> Loop 전이 (타이머 사용)
	GetWorld()->GetTimerManager().ClearTimer(WallSlideStartToLoopTimer);
	GetWorld()->GetTimerManager().SetTimer(
		WallSlideStartToLoopTimer, this, &AMarioCharacter::EnterWallSlideLoop,
		WallSlideStartDuration, false);

	UE_LOG(LogTemp, Log, TEXT("[Wall] Enter SlideStart  Normal=(%.2f %.2f %.2f)"),
		CurrentWallNormal.X, CurrentWallNormal.Y, CurrentWallNormal.Z);
}

void AMarioCharacter::EnterWallSlideLoop()
{
	if (WallActionState == EWallActionState::SlideStart)
	{
		WallActionState = EWallActionState::SlideLoop;

		UpdateWallSlidePhysics(0.f); // 상승/하강 분기는 여기서 단일 처리

		UE_LOG(LogTemp, Log, TEXT("[Wall] Enter SlideLoop"));
	}
}

void AMarioCharacter::ResetWallSlide()
{
	GetWorld()->GetTimerManager().ClearTimer(WallSlideStartToLoopTimer);

	WallActionState = EWallActionState::None;
	CurrentWallNormal = FVector::ZeroVector;
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->GravityScale = DefaultGravityScale;
		MoveComp->bOrientRotationToMovement = bDefaultOrientRotationToMovement;
		ApplyWallSlideMovementSettings(false);
	}

	UE_LOG(LogTemp, Log, TEXT("[Wall] Reset -> None"));
}

void AMarioCharacter::ApplyWallSlideMovementSettings(bool bEnable)
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!Move) return;

	if (bEnable)
	{
		// 슬라이드 동안엔 떨어지면서 벽에 붙어야 하므로 Falling 강제
		Move->SetMovementMode(MOVE_Falling);
		
		Move->AirControl = WallSlideAirControl;
		Move->MaxAcceleration = WallSlideMaxAcceleration;
		Move->BrakingDecelerationFalling = WallSlideBrakingDecelFalling;
	}
	else
	{
		// 원복
		Move->AirControl = DefaultAirControl;
		Move->MaxAcceleration = DefaultMaxAcceleration;
		Move->BrakingDecelerationFalling = DefaultBrakingDecelFalling;
	}
}

void AMarioCharacter::UpdateWallSlidePhysics(float DeltaTime)
{
	if (!(WallActionState == EWallActionState::SlideStart || WallActionState == EWallActionState::SlideLoop))
	{
		return;
	}

	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!Move) return;

	if (!Move->IsFalling())
	{
		ResetWallSlide();
		return;
	}

	FaceWallFromNormal(CurrentWallNormal);

	FVector V = Move->Velocity;

	// 벽 노멀 성분 제거(벽에서 떨어지거나 파고드는 성분 제거)
	FVector N = CurrentWallNormal;
	N.Z = 0.f;
	N = N.GetSafeNormal();

	if (!N.IsNearlyZero())
	{
		const float AlongN = FVector::DotProduct(V, N);
		V -= N * AlongN;
	}

	// 핵심 분기
	if (V.Z > 0.f)
	{
		// 상승 구간: 정상 점프 감속(최고점까지)
		Move->GravityScale = DefaultGravityScale;
		// V.Z는 그대로 둠
	}
	else
	{
		// 하강 구간: 가속 없는 일정 하강
		Move->GravityScale = 0.f;
		V.Z = -WallSlideDownSpeed;
	}

	Move->Velocity = V;
}

void AMarioCharacter::ExecuteWallKick()
{
	// SlideStart/Loop에서만 허용
	if (!(WallActionState == EWallActionState::SlideStart || WallActionState == EWallActionState::SlideLoop))
	{
		return;
	}
	// SlideStart->Loop 전이 타이머는 즉시 끊기
	GetWorld()->GetTimerManager().ClearTimer(WallSlideStartToLoopTimer);
	
	ApplyWallSlideMovementSettings(false);
	
	// WallKick 상태로 전환(AnimBP가 KickJump 애니 재생)
	WallActionState = EWallActionState::WallKick;

	// 방향: 벽 노멀의 XY만 사용 (Normal은 "벽 → 캐릭터" 방향)
	FVector N = CurrentWallNormal;
	N.Z = 0.f;
	N = N.GetSafeNormal();

	if (N.IsNearlyZero())
	{
		// 보험
		N = GetActorForwardVector();
		N.Z = 0.f;
		N.Normalize();
	}

	// KickDir = "벽에서 떨어지는 방향" = Normal 방향
	const FVector KickDir = N;

	const FVector LaunchVel =
		(KickDir * WallKickHorizontalVelocity) +
		FVector(0.f, 0.f, WallKickVerticalVelocity);

	// 먼저 정면을 발사 방향으로 고정(= 180도 전환 효과)
	FaceDirection2D(LaunchVel);

	// 기존 공중기 플래그 정리(충돌 방지)
	bIsLongJumping = false;
	bIsDiving = false;

	WallKickStartTime = GetWorld()->GetTimeSeconds();
	BeginWallKickInputLock();

	// 강제로 Falling 유지
	GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	
	GetCharacterMovement()->GravityScale = DefaultGravityScale;
	// Launch
	LaunchCharacter(LaunchVel, true, true);

	// WallKick 상태 유지(애니 재생 창)
	GetWorld()->GetTimerManager().ClearTimer(WallKickStateTimer);
	GetWorld()->GetTimerManager().SetTimer(
		WallKickStateTimer, this, &AMarioCharacter::EndWallKickState,
		WallKickStateDuration, false);

	UE_LOG(LogTemp, Log, TEXT("[Wall] Execute WallKick  Vel=(%.1f %.1f %.1f)"),
		LaunchVel.X, LaunchVel.Y, LaunchVel.Z);
}

void AMarioCharacter::EndWallKickState()
{
	if (WallActionState == EWallActionState::WallKick)
	{
		WallActionState = EWallActionState::None;
		CurrentWallNormal = FVector::ZeroVector;
		
		if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
		{
			if (!bWallKickInputLocked)
			{
				MoveComp->bOrientRotationToMovement = bDefaultOrientRotationToMovement;
			}
		}
		
		UE_LOG(LogTemp, Log, TEXT("[Wall] End WallKick -> None"));
	}
}

void AMarioCharacter::BeginWallKickInputLock()
{
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (!MoveComp || !GetWorld()) return;

	if (bWallKickInputLocked)
	{
		return;
	}

	bWallKickInputLocked = true;

	CachedAirControl = MoveComp->AirControl;
	CachedMaxAcceleration = MoveComp->MaxAcceleration;

	// 순간 조작을 막아서 반대로 튀는 방향이 안정되게
	MoveComp->AirControl = 0.f;
	MoveComp->MaxAcceleration = 0.f;
	
	MoveComp->bOrientRotationToMovement = false;
	
	GetWorld()->GetTimerManager().ClearTimer(WallKickInputLockTimer);
	GetWorld()->GetTimerManager().SetTimer(
		WallKickInputLockTimer, this, &AMarioCharacter::EndWallKickInputLock,
		WallKickInputLockDuration, false);
}

void AMarioCharacter::EndWallKickInputLock()
{
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (!MoveComp) return;

	if (!bWallKickInputLocked)
	{
		return;
	}

	bWallKickInputLocked = false;

	MoveComp->AirControl = CachedAirControl;
	MoveComp->MaxAcceleration = CachedMaxAcceleration;
	
	if (WallActionState == EWallActionState::None)
	{
		MoveComp->bOrientRotationToMovement = bDefaultOrientRotationToMovement;
	}
}

void AMarioCharacter::CancelWallKickForSlideEntry()
{
	if (!GetWorld()) return;

	// WallKick 상태를 유지하려는 타이머들 제거
	GetWorld()->GetTimerManager().ClearTimer(WallKickStateTimer);
	GetWorld()->GetTimerManager().ClearTimer(WallKickInputLockTimer);

	// 이동 파라미터 복구
	EndWallKickInputLock();
}

void AMarioCharacter::TryEnterWallSlideFromCurrentOverlap()
{
	// 이미 어떤 Wall 상태면 중복 진입 금지
	if (WallActionState != EWallActionState::None)
	{
		return;
	}

	// bWallOverlapping이 순간적으로 false여도(착지/미세 분리) Trace로 판정해서 진입 가능하게 둔다.

	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (!MoveComp || !MoveComp->IsFalling())
	{
		return;
	}

	// 기존 조건 재사용(점프 1~3단 / 백덤블링 / 반동점프)
	if (!IsWallActionCandidate())
	{
		return;
	}

	// 현재 오버랩 상태에서 "벽 히트" 재확인
	FHitResult DummySweep;
	FHitResult WallHit;
	if (!TryGetWallHitFromDetector(DummySweep, WallHit))
	{
		return;
	}

	if (!PassesWallFilters(WallHit))
	{
		return;
	}

	StartWallSlide(WallHit);
}

void AMarioCharacter::ConfirmWallContactAfterEndOverlap()
{
	// 그 사이에 상태가 바뀌었으면 무시
	if (!(WallActionState == EWallActionState::SlideStart || WallActionState == EWallActionState::SlideLoop))
	{
		return;
	}

	// 유예 중에 다시 오버랩이 들어왔으면 유지
	if (bWallOverlapping)
	{
		return;
	}

	// 실제로 아직 벽이 가까이 있으면 유지, 아니면 Reset
	FHitResult DummySweep;
	FHitResult WallHit;
	if (TryGetWallHitFromDetector(DummySweep, WallHit) && PassesWallFilters(WallHit))
	{
		bWallOverlapping = true;
		CurrentWallNormal = WallHit.ImpactNormal.GetSafeNormal();
		return;
	}

	ResetWallSlide();
}

void AMarioCharacter::FaceDirection2D(const FVector& Dir)
{
	FVector D = Dir;
	D.Z = 0.f;
	if (D.IsNearlyZero())
	{
		return;
	}
	D.Normalize();

	FRotator R = D.Rotation();
	R.Pitch = 0.f;
	R.Roll  = 0.f;
	SetActorRotation(R);
}

void AMarioCharacter::FaceWallFromNormal(const FVector& WallNormal)
{
	FaceDirection2D(-WallNormal);
}

void AMarioCharacter::FaceAwayFromNormal(const FVector& WallNormal)
{
	FaceDirection2D(WallNormal);
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

FRotator AMarioCharacter::GetViewRotation() const
{
	if (bCaptureControlRotOverride)
	{
		return CaptureControlRot;
	}
	return Super::GetViewRotation();
}

void AMarioCharacter::SetCaptureControlRotationOverride(bool bEnable)
{
	bCaptureControlRotOverride = bEnable;
}

void AMarioCharacter::SetCaptureControlRotation(const FRotator& InRot)
{
	CaptureControlRot = InRot;
}

void AMarioCharacter::CaptureFeedLookInput(const FInputActionValue& Value)
{
	Look(Value);
}

void AMarioCharacter::CaptureSyncTargetControlRotation(const FRotator& InRot)
{
	TargetControlRotation = InRot;
}

void AMarioCharacter::CaptureTickCamera(float DeltaTime, APlayerController* PC)
{
	if (!bCaptureControlRotOverride) return;
	if (!PC) return;

	const FRotator Current = PC->GetControlRotation();
	const FRotator Smoothed = FMath::RInterpTo(
		Current,
		TargetControlRotation,
		DeltaTime,
		CameraRotationInterpSpeed
	);

	PC->SetControlRotation(Smoothed);
	SetCaptureControlRotation(Smoothed); // GetViewRotation()이 이 값을 반환
}
