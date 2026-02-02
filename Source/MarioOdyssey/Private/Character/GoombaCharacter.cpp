#include "Character/GoombaCharacter.h"

#include "AIController.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/DamageType.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"

// Enhanced Input
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"

static float CalcStackOffsetZ(const AGoombaCharacter* Below, const AGoombaCharacter* Above, float Gap)
{
	if (!Below || !Above) return 0.f;
	const UCapsuleComponent* BelowCapsule = Below->GetCapsuleComponent();
	const UCapsuleComponent* AboveCapsule = Above->GetCapsuleComponent();
	if (!BelowCapsule || !AboveCapsule) return 0.f;

	const float BelowHH = BelowCapsule->GetScaledCapsuleHalfHeight();
	const float AboveHH = AboveCapsule->GetScaledCapsuleHalfHeight();
	return (BelowHH + AboveHH) + Gap;// +Gap = separation, not penetration
}

AGoombaCharacter::AGoombaCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// AI 사용(배치/스폰 시 자동 AI)
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = AAIController::StaticClass();

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 720.f, 0.f);

	// 굼바는 기본적으로 캡쳐 가능
	bCapturable = true;

	// (선택) 컨택 스피어 반경(베이스 기본값 70)
	if (ContactSphere)
	{
		ContactSphere->SetSphereRadius(70.f);
	}

	// 스택 판정용 머리 스피어
	HeadStackSphere = CreateDefaultSubobject<USphereComponent>(TEXT("HeadStackSphere"));
	if (HeadStackSphere)
	{
		HeadStackSphere->SetupAttachment(GetCapsuleComponent());
		HeadStackSphere->InitSphereRadius(18.f);
		HeadStackSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		HeadStackSphere->SetCollisionResponseToAllChannels(ECR_Overlap);
		HeadStackSphere->SetGenerateOverlapEvents(true);

		HeadStackSphere->OnComponentBeginOverlap.AddDynamic(this, &AGoombaCharacter::OnHeadStackSphereBeginOverlap);
	}
}

void AGoombaCharacter::BeginPlay()
{
	Super::BeginPlay();

	HomeLocation = GetActorLocation();
	SetState(EGoombaAIState::Patrol);

	// 캡슐 높이에 맞춰 머리 스피어 위치 보정
	if (HeadStackSphere && GetCapsuleComponent())
	{
		const float HH = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
		HeadStackSphere->SetRelativeLocation(FVector(0.f, 0.f, HH));
	}
}

void AGoombaCharacter::OnContactBeginOverlap_Goomba(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor) return;

	// 본인이 캡쳐중이면(플레이어 조종) 컨택 공격을 막는 옵션(베이스 정책 유지)
	if (bDisableContactDamageWhileCaptured && bIsCaptured) return;

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!PlayerPawn || OtherActor != PlayerPawn) return; // “플레이어가 조종 중인 Pawn”만 때림

	// ===== 스택 내부 자기 넉백/데미지 방지 =====
	// (캡쳐 굼바가 다른 굼바 머리 위에 안착했을 때, 아래 굼바 ContactSphere가 PlayerPawn(=캡쳐 굼바)을 때리며 LaunchCharacter로 계속 밀어내는 문제 방지)
	if (AGoombaCharacter* PlayerGoomba = Cast<AGoombaCharacter>(PlayerPawn))
	{
		AGoombaCharacter* MyRoot = GetStackRoot();
		if (!MyRoot) MyRoot = this;

		AGoombaCharacter* PlayerRoot = PlayerGoomba->GetStackRoot();
		if (!PlayerRoot) PlayerRoot = PlayerGoomba;

		if (MyRoot == PlayerRoot)
		{
			return; // 같은 스택이면 무시
		}
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastContactDamageTime < ContactDamageCooldown) return;
	LastContactDamageTime = Now;

	UGameplayStatics::ApplyDamage(PlayerPawn, ContactDamage, GetController(), this, UDamageType::StaticClass());

	// 넉백(플레이어가 몬스터여도 ACharacter면 LaunchCharacter로 밀림)
	if (ACharacter* HitChar = Cast<ACharacter>(PlayerPawn))
	{
		FVector Dir = (HitChar->GetActorLocation() - GetActorLocation());
		Dir.Z = 0.f;
		Dir = Dir.GetSafeNormal();
		HitChar->LaunchCharacter(Dir * KnockbackStrength + FVector(0, 0, KnockbackUp), true, true);
	}
}


void AGoombaCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// 베이스 바인딩을 쓰지 않고, 굼바는 스택 루트로 포워딩하는 바인딩을 사용한다.
	if (!PlayerInputComponent) return;

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC) return;

	if (IA_Move)
	{
		EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &AGoombaCharacter::Input_Move_Stack);
	}
	if (IA_Look)
	{
		EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AGoombaCharacter::Input_Look_Passthrough);
	}

	if (IA_Run)
	{
		EIC->BindAction(IA_Run, ETriggerEvent::Started, this, &AGoombaCharacter::Input_RunStarted_Stack);
		EIC->BindAction(IA_Run, ETriggerEvent::Completed, this, &AGoombaCharacter::Input_RunCompleted_Stack);
	}

	if (IA_Jump)
	{
		EIC->BindAction(IA_Jump, ETriggerEvent::Started, this, &AGoombaCharacter::Input_JumpStarted_Stack);
		EIC->BindAction(IA_Jump, ETriggerEvent::Completed, this, &AGoombaCharacter::Input_JumpCompleted_Stack);
	}

	// 정책: C키(IA_Crouch)를 캡쳐 해제로 재사용
	if (IA_Crouch)
	{
		EIC->BindAction(IA_Crouch, ETriggerEvent::Started, this, &AGoombaCharacter::Input_ReleaseCapture_Passthrough);
	}
}

void AGoombaCharacter::OnCapturedExtra(AController* Capturer, const FCaptureContext& Context)
{
	// 캡쳐 중엔 AI 로직은 Tick에서 중단되지만, 상태/속도는 정리해둔다.
	StunRemain = 0.f;
	SetState(EGoombaAIState::Stunned);

	// 스택 루트 기준으로 "플레이어 조종 스택" 모드로 전환
	if (AGoombaCharacter* Root = GetStackRoot())
	{
		Root->UpdateStackPresentation(0.f); // 즉시 1회 정리
	}
}

void AGoombaCharacter::OnReleasedExtra(const FCaptureReleaseContext& Context)
{
	// 해제 후 스턴 -> 두리번 -> 복귀 (스택이면 루트에 적용)
	if (AGoombaCharacter* Root = GetStackRoot())
	{
		Root->ForceStun(ReleaseStunSeconds);
		Root->UpdateStackPresentation(0.f);
	}

	// 본인(=기존 정책 유지)
	StunRemain = ReleaseStunSeconds;
	SetState(EGoombaAIState::Stunned);
}

APawn* AGoombaCharacter::GetCapturePawn_Implementation()
{
	// 스택 어디를 모자로 맞춰도 "스택 최상단"이 캡쳐 대상
	return GetStackTop();
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

	// 스택 팔로워는 AI/정렬은 루트가 담당
	if (IsStackFollower())
	{
		return;
	}

	// 루트(또는 단독)는 매 Tick 스택 정렬/모드 반영
	UpdateStackPresentation(Dt);

	// 스택 안에 캡쳐 굼바가 있으면(플레이어 조종 중) 루트 AI 중단
	if (HasCapturedInStack())
	{
		return;
	}

	// PlayerController가 Possess 중이면(예: 디버그로 Possess) AI 로직 중단
	if (Controller && Controller->IsPlayerController())
	{
		return;
	}

	AActor* Target = GetPlayerTargetActor();

	switch (State)
	{
	case EGoombaAIState::Patrol:     UpdatePatrol(Dt); break;
	case EGoombaAIState::Chase:		 UpdateChase(Dt, Target); break;
	case EGoombaAIState::LookAround: UpdateLookAround(Dt); break;
	case EGoombaAIState::ReturnHome: UpdateReturnHome(Dt); break;
	case EGoombaAIState::Stunned:
		StunRemain -= Dt;
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
		// 루트 AI는 0으로 멈추고, 캡쳐 조종은 UpdateStackPresentation/입력에서 속도 관리
		GetCharacterMovement()->MaxWalkSpeed = 0.f;
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

void AGoombaCharacter::DrawSearchConeDebug() const
{
	UWorld* World = GetWorld();
	if (!World) return;

	const FVector Origin = GetActorLocation() + FVector(0, 0, DebugZOffset);
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

// ======================================================
// Stack helpers
// ======================================================

AGoombaCharacter* AGoombaCharacter::GetStackRoot()
{
	AGoombaCharacter* Cur = this;
	while (Cur && Cur->StackBelow.IsValid())
	{
		Cur = Cur->StackBelow.Get();
	}
	return Cur;
}

AGoombaCharacter* AGoombaCharacter::GetStackTop()
{
	AGoombaCharacter* Cur = this;
	while (Cur && Cur->StackAbove.IsValid())
	{
		Cur = Cur->StackAbove.Get();
	}
	return Cur;
}

AGoombaCharacter* AGoombaCharacter::GetCapturedInStack()
{
	AGoombaCharacter* Root = GetStackRoot();
	for (AGoombaCharacter* Cur = Root; Cur; Cur = Cur->StackAbove.Get())
	{
		if (Cur->bIsCaptured)
		{
			return Cur;
		}
	}
	return nullptr;
}

bool AGoombaCharacter::AttachStackRootAbove(AGoombaCharacter* LowerTop)
{
	if (!LowerTop) return false;

	AGoombaCharacter* UpperRoot = GetStackRoot();
	if (!UpperRoot || !UpperRoot->IsStackRoot()) return false;

	AGoombaCharacter* LowerRealTop = LowerTop->GetStackTop();
	if (!LowerRealTop || !LowerRealTop->IsStackTop()) return false;

	// 같은 스택이면 무시
	if (UpperRoot == LowerRealTop->GetStackRoot()) return false;

	// UpperRoot는 반드시 "위 스택 루트"여야 함
	// (혹시라도 붙어있으면 떼고 정리)
	if (UpperRoot->GetAttachParentActor())
	{
		UpperRoot->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}

	// 링크 연결
	UpperRoot->StackBelow = LowerRealTop;
	LowerRealTop->StackAbove = UpperRoot;

	// 스택 이웃끼리는 이동 스윕 충돌을 무시(서로 밀어내며 떨리는 현상 방지)
	if (UCapsuleComponent* UpperCap = UpperRoot->GetCapsuleComponent())
	{
		UpperCap->IgnoreActorWhenMoving(LowerRealTop, true);
	}
	if (UCapsuleComponent* LowerCap = LowerRealTop->GetCapsuleComponent())
	{
		LowerCap->IgnoreActorWhenMoving(UpperRoot, true);
	}

	// Attach + 오프셋
	const float OffsetZ = CalcStackOffsetZ(LowerRealTop, UpperRoot, StackGap);
	UpperRoot->AttachToComponent(LowerRealTop->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
	UpperRoot->SetActorRelativeLocation(FVector(0.f, 0.f, OffsetZ));

	// 팔로워는 이동 비활성(루트만 이동)
	UpperRoot->GetCharacterMovement()->DisableMovement();

	return true;
}

void AGoombaCharacter::OnHeadStackSphereBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                                    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                                                    bool bFromSweep, const FHitResult& SweepResult)
{
	// "내 머리"는 스택 최상단만 유효 (중간 머리로는 스택 합치기 금지)
	if (!IsStackTop()) return;

	AGoombaCharacter* OtherGoomba = Cast<AGoombaCharacter>(OtherActor);
	if (!OtherGoomba || OtherGoomba == this) return;

	AGoombaCharacter* UpperRoot = OtherGoomba->GetStackRoot();
	if (!UpperRoot) return;

	// 스택 시작은 "캡쳐 굼바(또는 캡쳐 포함 스택)"가 위에서 내려올 때만
	if (bOnlyStackWhenCapturedInUpperStack)
	{
		if (!UpperRoot->HasCapturedInStack())
		{
			return;
		}
	}

	// 위 스택이 떨어지는 중/점프 낙하 중일 때만
	UCharacterMovementComponent* UpperMove = UpperRoot->GetCharacterMovement();
	if (!UpperMove) return;

	const bool bUpperFalling = UpperMove->IsFalling() || (UpperRoot->GetVelocity().Z < -40.f);
	if (!bUpperFalling) return;

	// 위에서 내려오는지(대충) 확인
	if (UpperRoot->GetActorLocation().Z <= GetActorLocation().Z + 10.f)
	{
		return;
	}

	// 이미 붙어있는/순환 방지
	AGoombaCharacter* MyRoot = GetStackRoot();
	if (!MyRoot) return;
	if (UpperRoot == MyRoot) return;

	// 이미 스택에 붙어있으면(중복 BeginOverlap) 무시
	if (UpperRoot->StackBelow.IsValid())
	{
		return;
	}

	// 실제로 붙이는 대상은 "내 스택 최상단"(=this)
	const bool bAttached = UpperRoot->AttachStackRootAbove(this);
	if (!bAttached) return;

	// 합쳐진 최하단 루트에서 모드/회전/컨택 정리
	MyRoot->UpdateStackPresentation(0.f);
}

void AGoombaCharacter::UpdateStackPresentation(float Dt)
{
	AGoombaCharacter* Root = this;
	if (!Root || !Root->IsStackRoot()) return;

	AGoombaCharacter* Captured = Root->GetCapturedInStack();
	const bool bPlayerControlled = (Captured != nullptr);

	// 모드 전환 처리(루트에서만)
	if (bCachedPlayerControlledStack != bPlayerControlled)
	{
		ApplyStackModeTransition(bPlayerControlled, Captured);
		bCachedPlayerControlledStack = bPlayerControlled;
	}

	// 회전 기준
	const float DesiredYaw = (Captured != nullptr)
		? Captured->GetActorRotation().Yaw
		: Root->GetActorRotation().Yaw;

	// 루트도 캡쳐 굼바 방향으로 통일(캡쳐 포함 스택)
	if (Captured)
	{
		FRotator R = Root->GetActorRotation();
		R.Yaw = DesiredYaw;
		Root->SetActorRotation(R);
	}

	// 루트 -> 위로 올라가며 오프셋/회전/컨택 정리
	AGoombaCharacter* Below = Root;
	for (AGoombaCharacter* Cur = Root->StackAbove.Get(); Cur; Cur = Cur->StackAbove.Get())
	{
		// Attach 유지 + 오프셋 고정
		if (Cur->GetAttachParentActor() != Below)
		{
			Cur->AttachToComponent(Below->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
		}

		const float OffsetZ = CalcStackOffsetZ(Below, Cur, StackGap);
		Cur->SetActorRelativeLocation(FVector(0.f, 0.f, OffsetZ));

		// 회전 통일
		FRotator CR = Cur->GetActorRotation();
		CR.Yaw = DesiredYaw;
		Cur->SetActorRotation(CR);

		// 팔로워는 이동 비활성(루트만 이동)
		if (UCharacterMovementComponent* M = Cur->GetCharacterMovement())
		{
			M->DisableMovement();
			M->bOrientRotationToMovement = false;
		}

		Below = Cur;

		// Followers must NOT block movement. Keep only overlap (still capturable by queries).
		if (UCapsuleComponent* Cap = Cur->GetCapsuleComponent())
		{
			Cap->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Cap->SetCollisionResponseToAllChannels(ECR_Overlap);
		}

	}
}

void AGoombaCharacter::ApplyStackModeTransition(bool bNowPlayerControlled, AGoombaCharacter* Captured)
{
	AGoombaCharacter* Root = this;
	if (!Root || !Root->IsStackRoot()) return;

	UCharacterMovementComponent* RootMove = Root->GetCharacterMovement();
	if (RootMove)
	{
		if (bNowPlayerControlled)
		{
			// AI 중단 + 플레이어 조종 스택: 캡쳐 굼바 방향으로 회전 통일
			Root->StopAIMove();

			// 루트 회전은 우리가 강제로 맞출 거라 이동 기반 회전 끔
			bCachedOrientRotationToMovement = RootMove->bOrientRotationToMovement;
			RootMove->bOrientRotationToMovement = false;

			// 속도는 캡쳐 굼바(입력) 기준으로 맞춤
			ApplyCapturedSpeedToStackRoot();

			// 컨택 데미지/오버랩은 스택 내부 자해/중복 방지 위해 전부 꺼둠
			for (AGoombaCharacter* Cur = Root; Cur; Cur = Cur->StackAbove.Get())
			{
				if (Cur->ContactSphere)
				{
					Cur->ContactSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				}
			}
		}
		else
		{
			// 일반 몬스터 스택: 루트는 AI 회전/이동 기반 회전 복구
			RootMove->bOrientRotationToMovement = bCachedOrientRotationToMovement;

			// 컨택 데미지는 "루트만" 켬(스택 중복 데미지 방지)
			for (AGoombaCharacter* Cur = Root; Cur; Cur = Cur->StackAbove.Get())
			{
				if (!Cur->ContactSphere) continue;
				if (Cur == Root)
				{
					Cur->ContactSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
				}
				else
				{
					Cur->ContactSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				}
			}
		}
	}
}

void AGoombaCharacter::ForceStun(float Seconds)
{
	StunRemain = FMath::Max(0.f, Seconds);
	SetState(EGoombaAIState::Stunned);
}

void AGoombaCharacter::ApplyCapturedSpeedToStackRoot()
{
	AGoombaCharacter* Captured = GetCapturedInStack();
	if (!Captured) return;

	AGoombaCharacter* Root = Captured->GetStackRoot();
	if (!Root) return;

	UCharacterMovementComponent* RootMove = Root->GetCharacterMovement();
	if (!RootMove) return;

	// 캡쳐 굼바의 RunHeld 기준으로 루트 속도 세팅
	RootMove->MaxWalkSpeed = Captured->bRunHeld ? Captured->CapturedRunSpeed : Captured->CapturedWalkSpeed;
	RootMove->JumpZVelocity = Captured->CapturedJumpZVelocity;
}

// ======================================================
// Input forwarding (captured goomba -> stack root)
// ======================================================

void AGoombaCharacter::Input_Move_Stack(const FInputActionValue& Value)
{
	if (bInputLocked) return;

	const FVector2D Axis = Value.Get<FVector2D>();
	if (Axis.IsNearlyZero()) return;

	AGoombaCharacter* Root = GetStackRoot();
	if (!Root) return;

	// Control yaw는 "캡쳐 굼바(현재 Possess)"의 컨트롤 회전을 사용
	const FRotator ControlRot = Controller ? Controller->GetControlRotation() : GetActorRotation();
	const FRotator YawRot(0.f, ControlRot.Yaw, 0.f);

	const FVector Fwd = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);

	// 입력은 항상 루트에 적용(스택이면 전체가 이동)
	Root->AddMovementInput(Fwd, Axis.Y);
	Root->AddMovementInput(Right, Axis.X);
}

void AGoombaCharacter::Input_Look_Passthrough(const FInputActionValue& Value)
{
	// 베이스의 카메라 전달 로직 그대로 사용
	Input_Look(Value);
}

void AGoombaCharacter::Input_RunStarted_Stack(const FInputActionValue& Value)
{
	if (bInputLocked) return;
	bRunHeld = true;
	ApplyCapturedSpeedToStackRoot();
}

void AGoombaCharacter::Input_RunCompleted_Stack(const FInputActionValue& Value)
{
	bRunHeld = false;
	ApplyCapturedSpeedToStackRoot();
}

void AGoombaCharacter::Input_JumpStarted_Stack(const FInputActionValue& Value)
{
	if (bInputLocked) return;

	AGoombaCharacter* Root = GetStackRoot();
	if (!Root) return;

	// 점프도 루트에서 (스택 전체가 뜨는 효과)
	Root->Jump();
}

void AGoombaCharacter::Input_JumpCompleted_Stack(const FInputActionValue& Value)
{
	AGoombaCharacter* Root = GetStackRoot();
	if (!Root) return;

	Root->StopJumping();
}

void AGoombaCharacter::Input_ReleaseCapture_Passthrough(const FInputActionValue& Value)
{
	// 베이스의 캡쳐 해제 로직을 그대로 호출 (베이스 함수는 protected)
	OnReleaseCapturePressed(Value);
}

void AGoombaCharacter::ClearCapturedHitStun_Proxy()
{
	// 베이스의 피격 스턴 해제 로직 호출
	ClearCapturedHitStun();
}

// ======================================================
// Damage forwarding
// ======================================================

float AGoombaCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
                                   AController* EventInstigator, AActor* DamageCauser)
{
	AGoombaCharacter* Captured = GetCapturedInStack();
	if (Captured && Captured != this && !bForwardingDamage)
	{
		bForwardingDamage = true;

		// 스택 어디 맞아도 캡쳐 굼바로 전달 -> CaptureComponent가 마리오 HP로 라우팅
		UClass* DTClass = DamageEvent.DamageTypeClass.Get();
		if (!DTClass)
		{
			DTClass = UDamageType::StaticClass();
		}

		UGameplayStatics::ApplyDamage(Captured, DamageAmount, EventInstigator, DamageCauser, DTClass);


		bForwardingDamage = false;
		return 0.f;
	}

	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

// ======================================================
// Captured pawn damaged reaction (knockback to stack root)
// ======================================================

void AGoombaCharacter::OnCapturedPawnDamaged_Implementation(float Damage, AController* InstigatorController, AActor* DamageCauser)
{
	// 캡쳐 중이 아니면 베이스 로직과 동일하게 무시
	if (!bIsCaptured) return;

	// 입력 잠금
	bInputLocked = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CapturedHitStunTimer);
		World->GetTimerManager().SetTimer(
			CapturedHitStunTimer, this, &AGoombaCharacter::ClearCapturedHitStun_Proxy,
			CapturedHitStunSeconds, false);
	}

	// 넉백은 스택 루트에 적용(전체 스택이 밀리게)
	AGoombaCharacter* Root = GetStackRoot();
	if (!Root) Root = this;

	if (DamageCauser)
	{
		const FVector From = DamageCauser->GetActorLocation();
		const FVector To = Root->GetActorLocation();
		FVector Dir = (To - From);
		Dir.Z = 0.f;
		Dir = Dir.GetSafeNormal();

		const FVector Launch = Dir * CapturedHitKnockbackStrength + FVector(0.f, 0.f, CapturedHitKnockbackUp);
		Root->LaunchCharacter(Launch, true, true);
	}

	// 파생 추가 동작
	OnCapturedPawnDamagedExtra(Damage, InstigatorController, DamageCauser);
}
