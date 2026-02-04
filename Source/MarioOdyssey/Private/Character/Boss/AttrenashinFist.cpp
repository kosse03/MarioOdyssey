#include "Character/Boss/AttrenashinFist.h"
#include "Character/Boss/AttrenashinBoss.h"
#include "Character/Boss/IceTileActor.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputActionValue.h"

#include "Engine/EngineTypes.h"
#include "Engine/OverlapResult.h"

AAttrenashinFist::AAttrenashinFist()
{
	PrimaryActorTick.bCanEverTick = true;

	bCapturable = true;

	// 마리오는 Hit(블로킹) 대신 "Overlap(컨택 스피어)"로 데미지 받게 할 것
	bContactDamageAffectsMario = true;

	// 플레이어는 주먹을 통과(Blocking 금지)
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		// Pawn(=Mario 포함)과는 Overlap로만 처리 → 통과 가능
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	}

	// 메시가 혹시라도 블로킹하면 통과가 깨질 수 있으니 끔
	if (USkeletalMeshComponent* SM = GetMesh())
	{
		SM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SM->SetGenerateOverlapEvents(false);
	}

	// 컨택 데미지(ContactSphere)는 "내려찍기/임팩트 구간"에만 켜도록 게이트
	SetDamageOverlapEnabled(false);

	//주먹 크기에 맞춰 컨택 스피어를 캡슐 기반으로 키움
	if (ContactSphere && GetCapsuleComponent())
	{
		const float R = GetCapsuleComponent()->GetScaledCapsuleRadius();
		ContactSphere->SetSphereRadius(FMath::Max(ContactSphere->GetScaledSphereRadius(), R + 30.f));
	}
}



void AAttrenashinFist::InitFist(AAttrenashinBoss* InBoss, EFistSide InSide, USceneComponent* InAnchor)
{
	Boss = InBoss;
	Side = InSide;
	Anchor = InAnchor;

	if (Anchor.IsValid())
	{
		SetActorLocation(Anchor->GetComponentLocation(), false, nullptr, ETeleportType::None);
		SetActorRotation(Anchor->GetComponentRotation(), ETeleportType::None);
	}
}

bool AAttrenashinFist::CanBeCaptured_Implementation(const FCaptureContext& Context)
{
	return (State == EFistState::Stunned);
}

void AAttrenashinFist::OnCapturedExtra(AController* Capturer, const FCaptureContext& Context)
{
	SlamSeq = EAttrenashinSlamSeq::None;
	State = EFistState::CapturedDrive;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}
}

void AAttrenashinFist::OnReleasedExtra(const FCaptureReleaseContext& Context)
{
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_Walking);
	}
	StartReturnToAnchor(ReturnHomeSeconds);
}

void AAttrenashinFist::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (IA_Move)
		{
			EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &AAttrenashinFist::Input_Steer);
			EIC->BindAction(IA_Move, ETriggerEvent::Completed, this, &AAttrenashinFist::Input_Steer);
		}
	}
}

void AAttrenashinFist::Input_Steer(const FInputActionValue& Value)
{
	Steer = Value.Get<FVector2D>();
}

void AAttrenashinFist::StartSlamSequence(AActor* TargetActor)
{
	if (bIsCaptured) return;

	if (State == EFistState::Stunned || State == EFistState::CapturedDrive || State == EFistState::SlamSequence || State == EFistState::ReturnToAnchor)
		return;

	SeqTargetActor = TargetActor;
	if (!SeqTargetActor.IsValid()) return;

	SlamSeq = EAttrenashinSlamSeq::MoveAbove;
	SlamSeqT = 0.f;

	bPendingStun = false;
	
	SetDamageOverlapEnabled(false);
	EnterState(EFistState::SlamSequence);
}

void AAttrenashinFist::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	switch (State)
	{
	case EFistState::Idle:
		break;

	case EFistState::SlamSequence:
		TickSlamSequence(DeltaSeconds);
		break;

	case EFistState::Stunned:
		StunRemain -= DeltaSeconds;
		if (StunRemain <= 0.f)
		{
			StartReturnToAnchor(ReturnHomeSeconds);
		}
		break;

	case EFistState::CapturedDrive:
		TickCapturedDrive(DeltaSeconds);
		break;

	case EFistState::ReturnToAnchor:
		TickReturnToAnchor(DeltaSeconds);
		break;

	default:
		break;
	}
}

void AAttrenashinFist::SetDamageOverlapEnabled(bool bEnable)
{
	if (!ContactSphere) return;
	if (bDamageOverlapEnabled == bEnable) return;
	bDamageOverlapEnabled = bEnable;

	ContactSphere->SetGenerateOverlapEvents(bEnable);
	ContactSphere->SetCollisionEnabled(bEnable ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}

void AAttrenashinFist::EnterState(EFistState NewState)
{
	State = NewState;

	if (State == EFistState::Stunned)
	{
		StunRemain = StunSeconds;
	}
}

FVector AAttrenashinFist::GetDesiredHoverLocation() const
{
	AActor* T = SeqTargetActor.Get();
	if (!T) return GetActorLocation();

	float HalfH = 0.f;
	if (const ACharacter* Ch = Cast<ACharacter>(T))
	{
		if (const UCapsuleComponent* Cap = Ch->GetCapsuleComponent())
		{
			HalfH = Cap->GetScaledCapsuleHalfHeight();
		}
	}

	const FVector HeadBase = T->GetActorLocation() + FVector(0, 0, HalfH);
	return HeadBase + FVector(0, 0, HoverExtraZ);
}

void AAttrenashinFist::MoveTowardAdaptive(const FVector& Target, float Dt, float TimeRemaining, float MaxSpeed, bool bSweep)
{
	const FVector Cur = GetActorLocation();
	FVector Delta = Target - Cur;
	const float Dist = Delta.Size();

	// 아주 작은 오차만 정리
	if (Dist <= 0.5f)
	{
		SetActorLocation(Target, bSweep, nullptr, ETeleportType::None);
		return;
	}

	Delta /= Dist; // dir

	const float SafeRemain = FMath::Max(0.001f, TimeRemaining);
	const float RequiredSpeed = Dist / SafeRemain;
	const float Speed = FMath::Min(MaxSpeed, RequiredSpeed);

	const float Step = Speed * Dt;
	const float ClampedStep = FMath::Min(Step, Dist);

	const FVector NewLoc = Cur + Delta * ClampedStep;
	SetActorLocation(NewLoc, bSweep, nullptr, ETeleportType::None);
}

void AAttrenashinFist::TickSlamSequence(float Dt)
{
	if (bIsCaptured)
	{
		SetDamageOverlapEnabled(false);
		SlamSeq = EAttrenashinSlamSeq::None;
		EnterState(EFistState::CapturedDrive);
		return;
	}

	if (SlamSeq == EAttrenashinSlamSeq::None)
	{
		SetDamageOverlapEnabled(false);
		EnterState(EFistState::Idle);
		return;
	}

	SlamSeqT += Dt;

	switch (SlamSeq)
	{
	case EAttrenashinSlamSeq::MoveAbove:
	{
		const float Remain = FMath::Max(0.f, MoveAboveSeconds - SlamSeqT);
		const FVector DesiredNow = GetDesiredHoverLocation();

		// 스텝 이동
		MoveTowardAdaptive(DesiredNow, Dt, Remain, MoveAboveMaxSpeed, true);

		if (SlamSeqT >= MoveAboveSeconds)
		{
			SlamSeq = EAttrenashinSlamSeq::FollowAbove;
			SlamSeqT = 0.f;
		}
		break;
	}

	case EAttrenashinSlamSeq::FollowAbove:
	{
		const float Remain = FMath::Max(0.f, FollowAboveSeconds - SlamSeqT);
		const FVector Desired = GetDesiredHoverLocation();

		// 스텝 이동
		MoveTowardAdaptive(Desired, Dt, Remain, FollowAboveMaxSpeed, true);

		if (SlamSeqT >= FollowAboveSeconds)
		{
			SeqFrozenHoverLoc = GetActorLocation();
			SeqFrozenImpactXY = SeqFrozenHoverLoc;

			SlamSeq = EAttrenashinSlamSeq::PreSlamPause;
			SlamSeqT = 0.f;
		}
		break;
	}

	case EAttrenashinSlamSeq::PreSlamPause:
	{
		// 정지는 고정
		SetActorLocation(SeqFrozenHoverLoc, false, nullptr, ETeleportType::None);

		if (SlamSeqT >= PreSlamPauseSeconds)
		{
			BeginSlamDown();
			SlamSeq = EAttrenashinSlamSeq::SlamDown;
			SlamSeqT = 0.f;
			
			SetDamageOverlapEnabled(true);
		}
		break;
	}

	case EAttrenashinSlamSeq::SlamDown:
	{
		TickSlamDown(Dt);
		break;
	}

	case EAttrenashinSlamSeq::PostImpactWait:
	{
		if (SlamSeqT >= PostImpactWaitSeconds)
		{
			SetDamageOverlapEnabled(false);
			
			if (bPendingStun) EnterState(EFistState::Stunned);
			else StartReturnToAnchor(ReturnHomeSeconds);

			SlamSeq = EAttrenashinSlamSeq::None;
			SlamSeqT = 0.f;
		}
		break;
	}

	default:
		break;
	}
}

void AAttrenashinFist::BeginSlamDown()
{
	const FVector XY = SeqFrozenImpactXY;

	const FVector TraceStart = XY + FVector(0, 0, 8000.f);
	const FVector TraceEnd = XY + FVector(0, 0, -8000.f);

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AttrenashinFistSlamDownTrace), false, this);

	bool bHit = false;
	if (UWorld* World = GetWorld())
	{
		bHit = World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params);
	}

	const FVector ImpactPoint = bHit ? Hit.ImpactPoint : TraceEnd;

	const float HalfH = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;
	SeqSlamDownTargetLoc = FVector(XY.X, XY.Y, ImpactPoint.Z + HalfH);

	bPendingStun = IsOnIceTileAt(FVector(XY.X, XY.Y, ImpactPoint.Z));
}

void AAttrenashinFist::TickSlamDown(float Dt)
{
	// 내려찍기 스텝 이동
	const float Remain = FMath::Max(0.f, SlamDownSeconds - SlamSeqT);
	MoveTowardAdaptive(SeqSlamDownTargetLoc, Dt, Remain, 500000.f, true);

	if (SlamSeqT >= SlamDownSeconds)
	{
		SlamSeq = EAttrenashinSlamSeq::PostImpactWait;
		SlamSeqT = 0.f;
	}
}

bool AAttrenashinFist::IsOnIceTileAt(const FVector& WorldXY) const
{
	if (!GetWorld()) return false;

	TArray<FOverlapResult> Hits;

	FCollisionObjectQueryParams Obj;
	Obj.AddObjectTypesToQuery(ECC_WorldStatic);
	Obj.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionShape Shape = FCollisionShape::MakeSphere(120.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AttrenashinIceCheck), false, this);

	const bool bAny = GetWorld()->OverlapMultiByObjectType(Hits, WorldXY, FQuat::Identity, Obj, Shape, Params);
	if (!bAny) return false;

	for (const FOverlapResult& R : Hits)
	{
		AActor* A = R.GetActor();
		if (!A) continue;

		if (A->IsA(AIceTileActor::StaticClass())) return true;
		if (A->ActorHasTag(TEXT("IceTile"))) return true;
	}
	return false;
}

void AAttrenashinFist::StartReturnToAnchor(float DurationSeconds)
{
	if (!Anchor.IsValid())
	{
		EnterState(EFistState::Idle);
		return;
	}

	SlamSeq = EAttrenashinSlamSeq::None;
	SlamSeqT = 0.f;

	ReturnDuration = FMath::Max(0.01f, DurationSeconds);
	ReturnT = 0.f;

	EnterState(EFistState::ReturnToAnchor);
}

void AAttrenashinFist::TickReturnToAnchor(float Dt)
{
	if (!Anchor.IsValid())
	{
		EnterState(EFistState::Idle);
		return;
	}

	ReturnT += Dt;

	const float Remain = FMath::Max(0.f, ReturnDuration - ReturnT);
	const FVector Home = Anchor->GetComponentLocation();
	
	MoveTowardAdaptive(Home, Dt, Remain, ReturnMaxSpeed, true);

	if (ReturnT >= ReturnDuration)
	{
		EnterState(EFistState::Idle);
	}
}

void AAttrenashinFist::TickCapturedDrive(float Dt)
{
	if (!bIsCaptured)
	{
		StartReturnToAnchor(ReturnHomeSeconds);
		return;
	}

	FRotator R = GetActorRotation();
	R.Yaw += Steer.X * CapturedYawRate * Dt;
	SetActorRotation(R, ETeleportType::None);

	const FVector Delta = GetActorForwardVector() * CapturedDriveSpeed * Dt;

	FHitResult Hit;
	AddActorWorldOffset(Delta, true, &Hit);
}
