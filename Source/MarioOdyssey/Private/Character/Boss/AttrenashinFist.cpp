#include "Character/Boss/AttrenashinFist.h"
#include "Character/Boss/AttrenashinBoss.h"
#include "Character/Boss/IceTileActor.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "InputActionValue.h"
#include "Capture/CaptureComponent.h"
#include "MarioOdyssey/MarioCharacter.h"

#include "Engine/EngineTypes.h"
#include "Engine/OverlapResult.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"
#include "Logging/LogMacros.h"


DEFINE_LOG_CATEGORY_STATIC(LogAttrenashinDbg, Log, All);

static FORCEINLINE bool AttrBossDbgEnabled_Fist()
{
	if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("boss.debug.attrenashin")))
	{
		return CVar->GetInt() != 0;
	}
	return false;
}


AAttrenashinFist::AAttrenashinFist()
{
	PrimaryActorTick.bCanEverTick = true;

	bCapturable = true;
	bContactDamageAffectsMario = true; // ContactSphere 데미지 사용

	if (UCapsuleComponent* Cap = GetCapsuleComponent())
	{
		// 코드에서 보스 주먹 충돌 프리셋 강제 적용
		// - 월드에는 Blockmm
		// - Mario/Pawn, CapProjectile에는 Overlap (캡쳐/강제해제 이벤트용)
		Cap->SetCollisionProfileName(TEXT("Monster Capsule"));
		Cap->SetGenerateOverlapEvents(true);
		Cap->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		Cap->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap); // CapProjectile
	}

	if (USkeletalMeshComponent* Sk = GetMesh())
	{
		Sk->SetCollisionProfileName(TEXT("Monster Mesh"));
		Sk->SetGenerateOverlapEvents(false);
	}

	if (ContactSphere)
	{
		ContactSphere->SetCollisionProfileName(TEXT("Monster_ContactSphere"));
		ContactSphere->SetGenerateOverlapEvents(true);
	}

	SetDamageOverlapEnabled(false);
}

void AAttrenashinFist::SetDamageOverlapEnabled(bool bEnable)
{
	if (!ContactSphere) return;
	if (bDamageOverlapEnabled == bEnable) return;

	bDamageOverlapEnabled = bEnable;

	if (bEnable)
	{
		ContactSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		ContactSphere->SetGenerateOverlapEvents(true);
	}
	else
	{
		ContactSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ContactSphere->SetGenerateOverlapEvents(false);
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
	bPhase2SpinAnimActive = false;
	bPhase3ClapAnimActive = false;
	SetIgnoreIceStun(false);

	State = EFistState::CapturedDrive;
	SetDamageOverlapEnabled(false);
	CapturedKnockbackVelocity = FVector::ZeroVector;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}

	if (bLockCapturedDriveWorldZ)
	{
		FVector L = GetActorLocation();
		L.Z = CapturedDriveLockedWorldZ;
		SetActorLocation(L, false, nullptr, ETeleportType::None);
	}

	if (Boss.IsValid())
	{
		Boss->NotifyFistCaptured(this);
	}
}

void AAttrenashinFist::OnReleasedExtra(const FCaptureReleaseContext& Context)
{
	bPhase2SpinAnimActive = false;
	bPhase3ClapAnimActive = false;
	SetIgnoreIceStun(false);
	SetDamageOverlapEnabled(false);
	CapturedKnockbackVelocity = FVector::ZeroVector;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_Walking);
	}

	if (Boss.IsValid())
	{
		Boss->NotifyFistReleased(this);
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

void AAttrenashinFist::ForceReleaseCaptureByBoss()
{
	if (!bIsCaptured) return;

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	AMarioCharacter* Mario = Cast<AMarioCharacter>(PC->GetViewTarget());
	if (!Mario)
	{
		Mario = Cast<AMarioCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	}
	if (!Mario) return;

	if (UCaptureComponent* CapComp = Mario->FindComponentByClass<UCaptureComponent>())
	{
		CapComp->ReleaseCapture(ECaptureReleaseReason::Manual);
	}
}

void AAttrenashinFist::ApplyCapturedShardKnockback(const FVector& ShardVelocity, float Strength, float Upward)
{
	if (!bIsCaptured) return;

	(void)Upward; // 캡쳐 중 Z 고정 정책으로 세로 넉백은 사용하지 않음

	FVector Dir = ShardVelocity.GetSafeNormal();
	Dir.Z = 0.f;
	Dir = Dir.GetSafeNormal();
	if (Dir.IsNearlyZero())
	{
		Dir = GetActorForwardVector();
		Dir.Z = 0.f;
		Dir = Dir.GetSafeNormal();
	}

	if (Dir.IsNearlyZero())
	{
		return;
	}

	CapturedKnockbackVelocity += Dir * FMath::Max(0.f, Strength);
	CapturedKnockbackVelocity.Z = 0.f;

	const float MaxSpd = FMath::Max(0.f, CapturedKnockbackMaxSpeed);
	if (MaxSpd > 0.f)
	{
		CapturedKnockbackVelocity = CapturedKnockbackVelocity.GetClampedToMaxSize(MaxSpd);
	}
}


void AAttrenashinFist::AbortAttackAndReturnForBarrage(float ReturnSeconds)
{
	if (bIsCaptured) return;
	bPhase2SpinAnimActive = false;
	bPhase3ClapAnimActive = false;

	SetIgnoreIceStun(false);
	SetDamageOverlapEnabled(false);

	bPendingStun = false;
	SlamSeq = EAttrenashinSlamSeq::None;
	SlamSeqT = 0.f;
	SeqTargetActor.Reset();

	IceRainT = 0.f;
	bIceRainImpactFired = false;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->SetMovementMode(MOVE_Walking);
	}

	StartReturnToAnchor(FMath::Max(0.01f, ReturnSeconds));
}

void AAttrenashinFist::StartSlamSequence(AActor* TargetActor)
{
	if (bIsCaptured) return;
	bPhase2SpinAnimActive = false;
	bPhase3ClapAnimActive = false;

	if (State == EFistState::Stunned || State == EFistState::CapturedDrive || State == EFistState::SlamSequence ||
		State == EFistState::ReturnToAnchor || State == EFistState::IceRainSlam)
		return;

	SeqTargetActor = TargetActor;
	if (!SeqTargetActor.IsValid()) return;

	// 기획 고정: 번갈아 내려치기 준비 대기 시간은 최소 0.9초
	PreSlamPauseSeconds = FMath::Max(0.9f, PreSlamPauseSeconds);

	SlamSeq = EAttrenashinSlamSeq::MoveAbove;
	SlamSeqT = 0.f;

	bPendingStun = false;
	SetDamageOverlapEnabled(false);

	EnterState(EFistState::SlamSequence);
}

void AAttrenashinFist::StartIceRainSlam()
{
	if (bIsCaptured) return;
	bPhase2SpinAnimActive = false;
	bPhase3ClapAnimActive = false;

	if (State == EFistState::CapturedDrive || State == EFistState::Stunned ||
		State == EFistState::ReturnToAnchor || State == EFistState::SlamSequence ||
		State == EFistState::IceRainSlam)
	{
		return;
	}

	// 기존 스턴 시퀀스와 분리
	SlamSeq = EAttrenashinSlamSeq::None;
	SlamSeqT = 0.f;

	bPendingStun = false;      // 핵심: 스턴 금지
	SetIgnoreIceStun(true);    // 핵심: 얼음타일 스턴 무시
	SetDamageOverlapEnabled(false);

	IceRainT = 0.f;
	bIceRainImpactFired = false;

	IceRainStartLoc = GetActorLocation();
	IceRainApexLoc = IceRainStartLoc + FVector(0.f, 0.f, IceRainRiseZ);
	BuildIceRainImpactPoint();

	EnterState(EFistState::IceRainSlam);
}

void AAttrenashinFist::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 데미지 오버랩은 스턴용 SlamDown 구간에만 ON
	const bool bShouldDamageOverlap = (State == EFistState::SlamSequence) &&
		(SlamSeq == EAttrenashinSlamSeq::SlamDown || SlamSeq == EAttrenashinSlamSeq::PostImpactWait);
	SetDamageOverlapEnabled(bShouldDamageOverlap);

	switch (State)
	{
	case EFistState::Idle:
		break;

	case EFistState::SlamSequence:
		TickSlamSequence(DeltaSeconds);
		break;

	case EFistState::IceRainSlam:
		TickIceRainSlam(DeltaSeconds);
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

void AAttrenashinFist::EnterState(EFistState NewState)
{
	const EFistState PrevState = State;
	State = NewState;

	if (AttrBossDbgEnabled_Fist())
	{
		UE_LOG(LogAttrenashinDbg, Warning, TEXT("[Fist EnterState] %s Side=%d %d->%d Loc=%s"),
			*GetName(),
			(int32)Side,
			(int32)PrevState,
			(int32)State,
			*GetActorLocation().ToString());
	}

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

	if (Dist <= 0.5f)
	{
		SetActorLocation(Target, bSweep, nullptr, ETeleportType::None);
		return;
	}

	Delta /= Dist;

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
		SlamSeq = EAttrenashinSlamSeq::None;
		EnterState(EFistState::CapturedDrive);
		return;
	}

	if (SlamSeq == EAttrenashinSlamSeq::None)
	{
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
		SetActorLocation(SeqFrozenHoverLoc, false, nullptr, ETeleportType::None);

		if (SlamSeqT >= PreSlamPauseSeconds)
		{
			BeginSlamDown();
			SlamSeq = EAttrenashinSlamSeq::SlamDown;
			SlamSeqT = 0.f;
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

	// 스턴용 시퀀스에서만 타일 스턴 판정
	bPendingStun = (!bIgnoreIceStun) && IsOnIceTileAt(FVector(XY.X, XY.Y, ImpactPoint.Z));
}

void AAttrenashinFist::TickSlamDown(float Dt)
{
	const float Remain = FMath::Max(0.f, SlamDownSeconds - SlamSeqT);
	MoveTowardAdaptive(SeqSlamDownTargetLoc, Dt, Remain, 500000.f, true);

	if (SlamSeqT >= SlamDownSeconds)
	{
		SlamSeq = EAttrenashinSlamSeq::PostImpactWait;
		SlamSeqT = 0.f;
	}
}

void AAttrenashinFist::BuildIceRainImpactPoint()
{
	const FVector XY = IceRainStartLoc;
	const FVector TraceStart = FVector(XY.X, XY.Y, XY.Z + 8000.f);
	const FVector TraceEnd   = FVector(XY.X, XY.Y, XY.Z - 8000.f);

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AttrenashinIceRainTrace), false, this);

	const bool bHit = GetWorld() &&
		GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params);

	const FVector Impact = bHit ? Hit.ImpactPoint : TraceEnd;
	const float HalfH = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.f;
	IceRainImpactLoc = FVector(XY.X, XY.Y, Impact.Z + HalfH);
}

void AAttrenashinFist::TickIceRainSlam(float Dt)
{
	IceRainT += Dt;

	const float Total = IceRainRiseSeconds + IceRainDropSeconds;

	if (IceRainT <= IceRainRiseSeconds)
	{
		// 1.33초 상승
		const float Alpha = FMath::Clamp(IceRainT / IceRainRiseSeconds, 0.f, 1.f);
		const float Eased = FMath::InterpEaseOut(0.f, 1.f, Alpha, 2.0f);
		const FVector P = FMath::Lerp(IceRainStartLoc, IceRainApexLoc, Eased);
		SetActorLocation(P, true, nullptr, ETeleportType::None);
	}
	else
	{
		// 0.17초 급하강
		const float DropAlpha = FMath::Clamp((IceRainT - IceRainRiseSeconds) / IceRainDropSeconds, 0.f, 1.f);
		const float Eased = FMath::InterpEaseIn(0.f, 1.f, DropAlpha, 3.0f);
		const FVector P = FMath::Lerp(IceRainApexLoc, IceRainImpactLoc, Eased);
		SetActorLocation(P, true, nullptr, ETeleportType::None);

		// 착지 시점 1회 호출
		if (!bIceRainImpactFired && DropAlpha >= 1.f - KINDA_SMALL_NUMBER)
		{
			bIceRainImpactFired = true;
			if (Boss.IsValid())
			{
				Boss->PerformGroundSlam_IceShardRain();
			}
		}
	}

	if (IceRainT >= Total)
	{
		SetIgnoreIceStun(false);
		StartReturnToAnchor(ReturnHomeSeconds);
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
	const FRotator HomeRot = Anchor->GetComponentRotation();

	const FVector BeforeLoc = GetActorLocation();
	const bool bBossClap = Boss.IsValid() && Boss->IsPhase3ClapWindow();
	if (AttrBossDbgEnabled_Fist() && bBossClap)
	{
		UE_LOG(LogAttrenashinDbg, Warning, TEXT("[FistReturnToAnchor] %s Side=%d State=%d Loc=%s Home=%s t=%.2f/%.2f"),
			*GetName(),
			(int32)Side,
			(int32)State,
			*BeforeLoc.ToString(),
			*Home.ToString(),
			ReturnT,
			ReturnDuration);
	}

	MoveTowardAdaptive(Home, Dt, Remain, ReturnMaxSpeed, true);

	if (AttrBossDbgEnabled_Fist() && bBossClap)
	{
		const FVector AfterLoc = GetActorLocation();
		UE_LOG(LogAttrenashinDbg, Warning, TEXT("[FistReturnToAnchor] After Loc=%s (moved=%.1f)"), *AfterLoc.ToString(), (AfterLoc - BeforeLoc).Size());
	}

	// 복귀 중 축(회전)도 원복
	const float Alpha = FMath::Clamp(ReturnDuration > 0.f ? (ReturnT / ReturnDuration) : 1.f, 0.f, 1.f);
	const FRotator NewRot = FMath::Lerp(GetActorRotation(), HomeRot, Alpha);
	SetActorRotation(NewRot, ETeleportType::None);

	if (ReturnT >= ReturnDuration)
	{
		SetActorLocation(Home, false, nullptr, ETeleportType::None);
		SetActorRotation(HomeRot, ETeleportType::None);
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

	bool bShiftDash = false;
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		bShiftDash = PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift);
	}

	float Speed = CapturedDriveSpeed * (bShiftDash ? CapturedDashSpeedMultiplier : 1.0f);
	if (bShiftDashWhenNoInput)
	{
		Speed = FMath::Max(0.f, Speed);
	}

	const FVector DriveDelta = GetActorForwardVector() * Speed * Dt;
	CapturedKnockbackVelocity.Z = 0.f;
	const FVector KnockDelta = CapturedKnockbackVelocity * Dt;

	FHitResult Hit;
	AddActorWorldOffset(DriveDelta + KnockDelta, true, &Hit);

	CapturedKnockbackVelocity = FMath::VInterpTo(
		CapturedKnockbackVelocity,
		FVector::ZeroVector,
		Dt,
		FMath::Max(0.f, CapturedKnockbackDamping));
	CapturedKnockbackVelocity.Z = 0.f;

	if (bLockCapturedDriveWorldZ)
	{
		FVector L = GetActorLocation();
		L.Z = CapturedDriveLockedWorldZ;
		SetActorLocation(L, false, nullptr, ETeleportType::None);
	}
}
