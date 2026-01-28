#include "Character/GoombaCharacter.h"

#include "DrawDebugHelpers.h"
#include "AIController.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"

#include "MarioOdyssey/MarioCharacter.h"
#include "Capture/CaptureComponent.h"

AGoombaCharacter::AGoombaCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// AI 사용(배치/스폰 시 자동 AI)
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = AAIController::StaticClass();

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 720.f, 0.f);
	
	//Sphere 접촉 데미지 적용 콜리젼
	ContactSphere = CreateDefaultSubobject<USphereComponent>(TEXT("ContactSphere"));
	ContactSphere->SetupAttachment(RootComponent);
	ContactSphere->InitSphereRadius(70.f);
	ContactSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ContactSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	ContactSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ContactSphere->SetGenerateOverlapEvents(true);

	ContactSphere->OnComponentBeginOverlap.AddDynamic(this, &AGoombaCharacter::OnContactBeginOverlap);
}

void AGoombaCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		DefaultJumpZVelocity = Move->JumpZVelocity;
	}
	
	HomeLocation = GetActorLocation();
	SetState(EGoombaAIState::Patrol);
}

void AGoombaCharacter::Tick(float Dt)
{
	Super::Tick(Dt);
	////서칭콘 디버그용//////////////////////////
#if !(UE_BUILD_SHIPPING)
	if (bDrawSearchCone)
	{
		DrawSearchConeDebug();
	}
#endif
	/////////////////////////////////////////
	// 캡쳐 중에는 AI 로직 돌리지 않음(플레이어가 조종)
	if (bIsCaptured) return;

	// PlayerController가 Possess 중이면(캡쳐 중 컨트롤 전환 등) AI 로직 중단
	if (Controller && Controller->IsPlayerController()) return;

	AActor* Target = GetPlayerTargetActor();

	switch (State)
	{
	case EGoombaAIState::Patrol:     UpdatePatrol(Dt); break;
	case EGoombaAIState::Chase:		 UpdateChase(Dt, Target); break;
	case EGoombaAIState::LookAround: UpdateLookAround(Dt); break;
	case EGoombaAIState::ReturnHome: UpdateReturnHome(Dt); break;
	case EGoombaAIState::Stunned:
		StunRemain -=Dt;
		if (StunRemain <= 0.f)
		{
			SetState(EGoombaAIState::LookAround);
		}
		break;
	}

	// 추격 중이 아니면 “30초 지나면 집으로” 타이머 누적
	if (State != EGoombaAIState::Chase)
	{
		ReturnHomeTimer += Dt;
		if (ReturnHomeTimer >= ReturnHomeAfterSeconds)
		{
			ReturnHomeTimer = 0.f;
			SetState(EGoombaAIState::ReturnHome);
		}
	}
}

void AGoombaCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 이 함수는 “플레이어가 Possess했을 때”만 의미가 있음.
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (IA_Move)
			EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &AGoombaCharacter::Input_Move);

		if (IA_Look)
			EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AGoombaCharacter::Input_Look);

		if (IA_Crouch)
			EIC->BindAction(IA_Crouch, ETriggerEvent::Started, this, &AGoombaCharacter::OnReleaseCapturePressed);
		if (IA_Jump)
			EIC->BindAction(IA_Jump, ETriggerEvent::Started, this, &AGoombaCharacter::Input_JumpStarted);
			EIC->BindAction(IA_Jump, ETriggerEvent::Completed, this, &AGoombaCharacter::Input_JumpCompleted);

		if (IA_Run)
			EIC->BindAction(IA_Run, ETriggerEvent::Started, this, &AGoombaCharacter::Input_RunStarted);
			EIC->BindAction(IA_Run, ETriggerEvent::Completed, this, &AGoombaCharacter::Input_RunCompleted);
	}
}

void AGoombaCharacter::OnContactBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor) return;
	if (bIsCaptured) return; // 본인이 캡쳐중이면 컨택 공격 안 함

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!PlayerPawn || OtherActor != PlayerPawn) return; // “플레이어가 조종 중인 Pawn”만 때림

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastContactDamageTime < ContactDamageCooldown) return;
	LastContactDamageTime = Now;

	// PlayerPawn에 데미지
	UGameplayStatics::ApplyDamage(PlayerPawn, ContactDamage, GetController(), this, UDamageType::StaticClass());

	// 넉백(플레이어가 굼바여도 ACharacter라면 LaunchCharacter로 밀림)
	if (ACharacter* HitChar = Cast<ACharacter>(PlayerPawn))
	{
		FVector Dir = (HitChar->GetActorLocation() - GetActorLocation());
		Dir.Z = 0.f;
		Dir = Dir.GetSafeNormal();
		HitChar->LaunchCharacter(Dir * KnockbackStrength + FVector(0,0,KnockbackUp), true, true);
	}
}

void AGoombaCharacter::OnReleaseCapturePressed(const FInputActionValue& Value)
{
	if (!bIsCaptured) return;

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	AMarioCharacter* Mario = Cast<AMarioCharacter>(PC->GetViewTarget());
	if (!Mario) return;

	if (UCaptureComponent* CapComp = Mario->FindComponentByClass<UCaptureComponent>())
	{
		CapComp->ReleaseCapture(ECaptureReleaseReason::Manual);
	}
}

void AGoombaCharacter::ApplyCapturedMoveParams()
{
	if (!bIsCaptured) return;
	
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = bRunHeld ? CapturedRunSpeed : CapturedWalkSpeed;
		Move->JumpZVelocity = CapturedJumpZVelocity;
	}
}

void AGoombaCharacter::Input_Move(const FInputActionValue& Value)
{
	if (bInputLocked) return;
	
	const FVector2D Axis = Value.Get<FVector2D>();
	if (!Controller) return;
	
	const FRotator ControlRot = Controller->GetControlRotation();
	const FRotator YawRot(0.f, ControlRot.Yaw, 0.f);

	const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
	const FVector Right   = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);

	AddMovementInput(Forward, Axis.Y);
	AddMovementInput(Right,   Axis.X);
}

void AGoombaCharacter::Input_RunStarted(const FInputActionValue& Value)
{
	if (bInputLocked) return;
	if (!Controller || !Controller->IsPlayerController()) return;
	
	bRunHeld = true;
	ApplyCapturedMoveParams();
}

void AGoombaCharacter::Input_RunCompleted(const FInputActionValue& Value)
{
	if (!Controller || !Controller->IsPlayerController()) return;
	
	bRunHeld = false;
	ApplyCapturedMoveParams();
}

void AGoombaCharacter::Input_JumpStarted(const FInputActionValue& Value)
{
	if (bInputLocked) return;
	if (!Controller || !Controller->IsPlayerController()) return;
	if (!bIsCaptured) return; // 캡쳐 중에만 점프 허용

	Jump();
}

void AGoombaCharacter::Input_JumpCompleted(const FInputActionValue& Value)
{
	if (!Controller || !Controller->IsPlayerController()) return;
	StopJumping();
}

void AGoombaCharacter::Input_Look(const FInputActionValue& Value)
{
	if (bInputLocked) return;
	if (!bIsCaptured) return;

	if (CapturingMario.IsValid())
	{
		CapturingMario->CaptureFeedLookInput(Value);
	}
}

void AGoombaCharacter::OnCaptured_Implementation(AController* Capturer, const FCaptureContext& Context)
{
	bIsCaptured = true;
	SetCapturedCameraCollision(true);
	StopAIMove();
	SetState(EGoombaAIState::Stunned); // 캡쳐 중 AI 안 돎
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	StunRemain =0.f;
	bInputLocked = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CapturedHitStunTimer);
	}
	// 캡쳐 시작 순간엔 Capturer의 Pawn이 마리오인 상태라 캐싱 가능
	CapturingMario = Capturer ? Cast<AMarioCharacter>(Capturer->GetPawn()) : nullptr;
	
	bRunHeld = false;
	ApplyCapturedMoveParams();
}

void AGoombaCharacter::OnReleased_Implementation(const FCaptureReleaseContext& Context)
{
	bIsCaptured = false;
	SetCapturedCameraCollision(false);
	bRunHeld = false;
	bInputLocked = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CapturedHitStunTimer);
	}
	
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->JumpZVelocity = DefaultJumpZVelocity;
	}
	
	CapturingMario.Reset();
	
	//해제 후 스턴 -> 두리번 -> 복귀
	StunRemain = ReleaseStunSeconds;
	SetState(EGoombaAIState::Stunned);
}

void AGoombaCharacter::OnCapturedPawnDamaged_Implementation(float Damage, AController* InstigatorController,
	AActor* DamageCauser)
{
	//캡쳐 중에만 굼바 피격 리액션 (HP는 마리오로 라우팅)
	if (!bIsCaptured) return;
	
	FVector FromLoc = GetActorLocation() - GetActorForwardVector() * 100.f;
	if (DamageCauser)
	{
		FromLoc = DamageCauser->GetActorLocation();
	}
	else if (InstigatorController && InstigatorController->GetPawn())
	{
		FromLoc = InstigatorController->GetPawn()->GetActorLocation();
	}
	
	FVector Dir = (GetActorLocation() - FromLoc);
	Dir.Z = 0.f;
	Dir = Dir.GetSafeNormal();
	
	LaunchCharacter(Dir * CapturedHitKnockbackStrength + FVector(0, 0, CapturedHitKnockbackUp), true, true);
	
	//잠시 입력 잠금
	bInputLocked = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CapturedHitStunTimer);
		World->GetTimerManager().SetTimer(CapturedHitStunTimer, this, &AGoombaCharacter::ClearCapturedHitStun, CapturedHitStunSeconds, false);
	}
}

void AGoombaCharacter::DrawSearchConeDebug() const
{
	UWorld* World = GetWorld();
	if (!World) return;

	const FVector Origin = GetActorLocation() + FVector(0,0,DebugZOffset);
	const FVector Dir = GetActorForwardVector();

	const float Len = DetectRange;
	const float AngleW = FMath::DegreesToRadians(DetectHalfAngleYawDeg);
	const float AngleH = FMath::DegreesToRadians(DetectHalfAnglePitchDeg);

	// 감지 성공이면 Red, 아니면 Green
	const AActor* Target = GetPlayerTargetActor();
	const bool bDetected = CanDetectTarget(Target);
	const FColor ConeColor = bDetected ? FColor::Red : FColor::Green;

	DrawDebugCone(
		World,
		Origin,
		Dir,
		Len,
		AngleW,
		AngleH,
		FMath::Max(8, DebugArcSegments),
		ConeColor,
		false,
		0.f,
		0,
		1.5f
	);

	// (선택) 감지 중이면 마리오까지 라인도 같이 표시
	if (bDetected && Target)
	{
		DrawDebugLine(World, Origin, Target->GetActorLocation(), FColor::Red, false, 0.f, 0, 1.5f);
	}
}

void AGoombaCharacter::SetCapturedCameraCollision(bool bCaptured)
{
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (!Capsule) return;

	if (bCaptured)
	{
		if (!bSavedCameraCollision)
		{
			PrevCapsuleCameraResponse = Capsule->GetCollisionResponseToChannel(ECC_Camera);
			if (MeshComp)
			{
				PrevMeshCameraResponse = MeshComp->GetCollisionResponseToChannel(ECC_Camera);
			}
			bSavedCameraCollision = true;
		}

		Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		if (MeshComp)
		{
			MeshComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		}
	}
	else
	{
		if (!bSavedCameraCollision) return;

		Capsule->SetCollisionResponseToChannel(ECC_Camera, PrevCapsuleCameraResponse);
		if (MeshComp)
		{
			MeshComp->SetCollisionResponseToChannel(ECC_Camera, PrevMeshCameraResponse);
		}
		bSavedCameraCollision = false;
	}
}

void AGoombaCharacter::ClearCapturedHitStun()
{
	bInputLocked = false;
}

void AGoombaCharacter::SetState(EGoombaAIState NewState)
{
	if (State == NewState) return;
	State = NewState;

	switch (State)
	{
	case EGoombaAIState::Patrol:
		GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
		bHasPatrolTarget = false;
		break;

	case EGoombaAIState::Chase:
		GetCharacterMovement()->MaxWalkSpeed = ChaseSpeed;
		break;

	case EGoombaAIState::LookAround:
		StopAIMove();
		LookAroundRemain = LookAroundSeconds;
		break;

	case EGoombaAIState::ReturnHome:
		GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
		break;
		
	case EGoombaAIState::Stunned:
		StopAIMove();
		if (bIsCaptured)
		{
			GetCharacterMovement()->MaxWalkSpeed = bRunHeld ? CapturedRunSpeed : CapturedWalkSpeed;
		}
		else
		{
			GetCharacterMovement()->MaxWalkSpeed = 0.f;
		}
		break;

	default: break;
	}
}

AActor* AGoombaCharacter::GetPlayerTargetActor() const
{
	if (!GetWorld()) return nullptr;
	return UGameplayStatics::GetPlayerPawn(GetWorld(), 0); // 평상시=마리오, 캡쳐중=굼바
}

bool AGoombaCharacter::CanDetectTarget(const AActor* Target) const
{
	if (!Target) return false;

	const FVector To = Target->GetActorLocation() - GetActorLocation();
	const float Dist2D = FVector(To.X, To.Y, 0.f).Size();
	if (Dist2D > DetectRange) return false;

	const FVector Dir2D = FVector(To.X, To.Y, 0.f).GetSafeNormal();
	const FVector Fwd2D = FVector(GetActorForwardVector().X, GetActorForwardVector().Y, 0.f).GetSafeNormal();

	const float CosHalf = FMath::Cos(FMath::DegreesToRadians(DetectHalfAngleDeg));
	return FVector::DotProduct(Fwd2D, Dir2D) >= CosHalf;
}


AAIController* AGoombaCharacter::GetAICon() const
{
	return Cast<AAIController>(Controller);
}

void AGoombaCharacter::StopAIMove()
{
	if (AAIController* AIC = GetAICon())
	{
		AIC->StopMovement();
	}
}

void AGoombaCharacter::UpdatePatrol(float Dt)
{
	AActor* Target = GetPlayerTargetActor();
	if (CanDetectTarget(Target))
	{
		ReturnHomeTimer = 0.f;
		SetState(EGoombaAIState::Chase);
		return;
	}

	if (!bHasPatrolTarget)
	{
		// NavMesh에서 스폰 근처 랜덤 포인트
		if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld()))
		{
			FNavLocation Out;
			if (NavSys->GetRandomReachablePointInRadius(HomeLocation, PatrolRadius, Out))
			{
				PatrolTarget = Out.Location;
				bHasPatrolTarget = true;

				if (AAIController* AIC = GetAICon())
				{
					AIC->MoveToLocation(PatrolTarget, 25.f);
				}
			}
		}
	}
	else
	{
		const float Dist = FVector::Dist2D(GetActorLocation(), PatrolTarget);
		if (Dist < 60.f)
		{
			bHasPatrolTarget = false; // 도착하면 다음 랜덤 목적지
		}
	}
}

void AGoombaCharacter::UpdateChase(float Dt, AActor* Target)
{
	if (!CanDetectTarget(Target))
	{
		SetState(EGoombaAIState::LookAround);
		return;
	}

	if (AAIController* AIC = GetAICon())
	{
		AIC->MoveToActor(Target, 70.f);
	}
}

void AGoombaCharacter::UpdateLookAround(float Dt)
{
	AActor* Target = GetPlayerTargetActor();
	if (CanDetectTarget(Target))
	{
		ReturnHomeTimer = 0.f;
		SetState(EGoombaAIState::Chase);
		return;
	}

	LookAroundRemain -= Dt;

	// 두리번(아주 약하게 회전)
	AddControllerYawInput(0.35f);

	if (LookAroundRemain <= 0.f)
	{
		SetState(EGoombaAIState::Patrol);
	}
}

void AGoombaCharacter::UpdateReturnHome(float Dt)
{
	AActor* Target = GetPlayerTargetActor();
	if (CanDetectTarget(Target))
	{
		ReturnHomeTimer = 0.f;
		SetState(EGoombaAIState::Chase);
		return;
	}

	if (AAIController* AIC = GetAICon())
	{
		AIC->MoveToLocation(HomeLocation, 40.f);
	}

	if (FVector::Dist2D(GetActorLocation(), HomeLocation) < 80.f)
	{
		SetState(EGoombaAIState::Patrol);
	}
}
