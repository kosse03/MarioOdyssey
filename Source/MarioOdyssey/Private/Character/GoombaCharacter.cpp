#include "Character/GoombaCharacter.h"

#include "DrawDebugHelpers.h"
#include "AIController.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "GameFramework/CharacterMovementComponent.h"

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

	AMarioCharacter* Mario = GetPlayerMario();

	switch (State)
	{
	case EGoombaAIState::Patrol:     UpdatePatrol(Dt); break;
	case EGoombaAIState::Chase:      UpdateChase(Dt, Mario); break;
	case EGoombaAIState::LookAround: UpdateLookAround(Dt); break;
	case EGoombaAIState::ReturnHome: UpdateReturnHome(Dt); break;
	case EGoombaAIState::Stunned:    /* 다음 단계(접촉/스턴)에서 */ break;
	}

	// 공통: 추격 중이 아니면 “30초 지나면 집으로” 타이머 누적
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

	// 이 함수는 “플레이어가 Possess했을 때”만 의미가 있으니,
	// 캡쳐 중 C키 해제 용도로 IA_Crouch만 바인딩
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC) return;

	if (IA_Crouch)
	{
		EIC->BindAction(IA_Crouch, ETriggerEvent::Started, this, &AGoombaCharacter::OnReleaseCapturePressed);
	}
}

void AGoombaCharacter::OnContactBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor) return;

	// 캡쳐 중엔 AI/컨택 데미지 일단 꺼두기
	if (bIsCaptured) return;

	AMarioCharacter* Mario = Cast<AMarioCharacter>(OtherActor);
	if (!Mario) return;

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastContactDamageTime < ContactDamageCooldown) return;
	LastContactDamageTime = Now;

	// 데미지(HP는 Mario::TakeDamage로 처리됨)
	UGameplayStatics::ApplyDamage(Mario, ContactDamage, GetController(), this, UDamageType::StaticClass());

	// 넉백
	FVector Dir = (Mario->GetActorLocation() - GetActorLocation());
	Dir.Z = 0.f;
	Dir = Dir.GetSafeNormal();

	Mario->LaunchCharacter(Dir * KnockbackStrength + FVector(0,0,KnockbackUp), true, true);
}

void AGoombaCharacter::OnReleaseCapturePressed(const FInputActionValue& Value)
{
	// 평상시엔 CapturingMario가 null이라 아무 일도 안 함
	if (!CapturingMario.IsValid()) return;

	if (UCaptureComponent* Cap = CapturingMario->GetCaptureComp())
	{
		if (Cap->IsCapturing())
		{
			Cap->ReleaseCapture(ECaptureReleaseReason::Manual);
		}
	}
}

void AGoombaCharacter::OnCaptured_Implementation(AController* Capturer, const FCaptureContext& Context)
{
	bIsCaptured = true;
	StopAIMove();
	SetState(EGoombaAIState::Stunned); // 캡쳐 중 AI 안 돎

	// 캡쳐 시작 순간엔 Capturer의 Pawn이 마리오인 상태라 캐싱 가능
	CapturingMario = Capturer ? Cast<AMarioCharacter>(Capturer->GetPawn()) : nullptr;
}

void AGoombaCharacter::OnReleased_Implementation(const FCaptureReleaseContext& Context)
{
	bIsCaptured = false;
	CapturingMario.Reset();

	// 해제 후 스턴(3초)은 “캡쳐 컴포넌트 쪽에서 호출되니”
	// 다음 단계에서 여기서 Stunned 타이머/복귀를 구현할 것.
	SetState(EGoombaAIState::LookAround);
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

	// ✅ 감지 성공이면 Red, 아니면 Green
	const AMarioCharacter* Mario = GetPlayerMario();
	const bool bDetected = CanDetectPlayer(Mario);
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
	if (bDetected && Mario)
	{
		DrawDebugLine(World, Origin, Mario->GetActorLocation(), FColor::Red, false, 0.f, 0, 1.5f);
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

	default: break;
	}
}

AMarioCharacter* AGoombaCharacter::GetPlayerMario() const
{
	return Cast<AMarioCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
}

bool AGoombaCharacter::CanDetectPlayer(const AMarioCharacter* Mario) const
{
	if (!Mario) return false;

	const FVector To = Mario->GetActorLocation() - GetActorLocation();
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
	AMarioCharacter* Mario = GetPlayerMario();
	if (CanDetectPlayer(Mario))
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

void AGoombaCharacter::UpdateChase(float Dt, AMarioCharacter* Mario)
{
	if (!CanDetectPlayer(Mario))
	{
		SetState(EGoombaAIState::LookAround);
		return;
	}

	if (AAIController* AIC = GetAICon())
	{
		AIC->MoveToActor(Mario, 70.f);
	}
}

void AGoombaCharacter::UpdateLookAround(float Dt)
{
	AMarioCharacter* Mario = GetPlayerMario();
	if (CanDetectPlayer(Mario))
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
	AMarioCharacter* Mario = GetPlayerMario();
	if (CanDetectPlayer(Mario))
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
