#include "Character/Boss/AttrenashinBoss.h"
#include "Character/Boss/AttrenashinFist.h"
#include "Character/Boss/IceShardActor.h"
#include "MarioOdyssey/MarioCharacter.h"

#include "Components/SphereComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

AAttrenashinBoss::AAttrenashinBoss()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	FistAnchorL = CreateDefaultSubobject<USceneComponent>(TEXT("FistAnchorL"));
	FistAnchorL->SetupAttachment(Root);

	FistAnchorR = CreateDefaultSubobject<USceneComponent>(TEXT("FistAnchorR"));
	FistAnchorR->SetupAttachment(Root);

	HeadHitSphere = CreateDefaultSubobject<USphereComponent>(TEXT("HeadHitSphere"));
	HeadHitSphere->SetupAttachment(Root);
	HeadHitSphere->InitSphereRadius(180.f);
	// - 캡쳐된 주먹(대개 Monster 또는 Pawn)과 Overlap -> 머리 타격 카운트
	// - 플레이어(Pawn)와 Overlap -> 접촉 데미지
	// - CapProjectile과 Overlap 유지
	HeadHitSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HeadHitSphere->SetCollisionProfileName(TEXT("Boss_Head_CounterOnly"));
	HeadHitSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	HeadHitSphere->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Overlap); // CapProjectile
	HeadHitSphere->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap); // Monster
	HeadHitSphere->SetGenerateOverlapEvents(true);
}

void AAttrenashinBoss::BeginPlay()
{
	Super::BeginPlay();

	if (HeadHitSphere)
	{
		HeadHitSphere->OnComponentBeginOverlap.AddDynamic(this, &AAttrenashinBoss::OnHeadBeginOverlap);
	}

	if (FistClass && GetWorld())
	{
		FActorSpawnParameters SP;
		SP.Owner = this;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AAttrenashinFist* L = GetWorld()->SpawnActor<AAttrenashinFist>(FistClass, FistAnchorL->GetComponentTransform(), SP);
		AAttrenashinFist* R = GetWorld()->SpawnActor<AAttrenashinFist>(FistClass, FistAnchorR->GetComponentTransform(), SP);

		if (L)
		{
			L->InitFist(this, EFistSide::Left, FistAnchorL);
			LeftFist = L;
		}
		if (R)
		{
			R->InitFist(this, EFistSide::Right, FistAnchorR);
			RightFist = R;
		}

		// 보스 Tick이 주먹 Tick 이후에 실행되도록 보장(박수/트래킹 등 보스 주도 위치가 최종 승리)
		if (LeftFist.IsValid())
		{
			AddTickPrerequisiteActor(LeftFist.Get());
		}
		if (RightFist.IsValid())
		{
			AddTickPrerequisiteActor(RightFist.Get());
		}
	}

	GetWorldTimerManager().SetTimerForNextTick([this]()
	{
		StartIceRainByBothFists(); // 첫 연출(양손 동시)
	});

	SlamAttemptTimer = SlamAttemptInterval;
}

void AAttrenashinBoss::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Phase1에서 캡쳐 해제(실패) 후 중앙 복귀가 진행 중이면
	// 다른 Phase1 로직(후퇴/내려치기 등)보다 우선 처리한다.
	if (Phase == EAttrenashinPhase::Phase1 && bPhase1ReturnToCenterActive)
	{
		UpdatePhase1CaptureFailReturnToCenter(DeltaSeconds);
		return;
	}

	if (bRetreating)
	{
		UpdateRetreat(DeltaSeconds);
	}

	if (ShouldFacePlayerDuringPunch())
	{
		FacePlayerYawOnly(DeltaSeconds);
	}

	if (bPhase2FlowActive)
	{
		UpdatePhase2(DeltaSeconds);
		return;
	}

	if (Phase != EAttrenashinPhase::Phase1) return;

	AAttrenashinFist* CapturedNow = nullptr;
	if (LeftFist.IsValid() && LeftFist->IsCapturedDriving())
	{
		CapturedNow = LeftFist.Get();
	}
	else if (RightFist.IsValid() && RightFist->IsCapturedDriving())
	{
		CapturedNow = RightFist.Get();
	}

	// 안전망: 캡쳐 콜백 누락되어도 자동으로 카운터 배러지 진입
	if (CapturedNow)
	{
		if (!bCaptureBarrageActive || BarrageCapturedFist.Get() != CapturedNow)
		{
			bIsInFear = true;
			StartRetreatMotion();
			StartCaptureBarrage(CapturedNow);
		}
		return; // 캡쳐 중에는 스턴 슬램 루프 중지
	}
	else if (bCaptureBarrageActive)
	{
		StopCaptureBarrage();
		bIsInFear = false;
	}

	// IceRain 상태 중엔 자동 스턴 슬램 트리거 금지
	if ((LeftFist.IsValid() && LeftFist->IsIceRainSlam()) ||
		(RightFist.IsValid() && RightFist->IsIceRainSlam()))
	{
		return;
	}

	SlamAttemptTimer -= DeltaSeconds;
	if (SlamAttemptTimer <= 0.f)
	{
		SlamAttemptTimer = SlamAttemptInterval;
		TryStartPhase1Slam();
	}
}

AActor* AAttrenashinBoss::GetPlayerTarget() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// 캡쳐 시스템 특성상 PlayerController가 몬스터(주먹)를 Possess할 수 있음.
	// 이 경우에도 트래킹 대상은 항상 마리오여야 하므로 ViewTarget(카메라 타겟) -> Pawn -> 월드 탐색 순으로 찾는다.
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
	{
		if (AActor* VT = PC->GetViewTarget())
		{
			if (VT->IsA(AMarioCharacter::StaticClass()))
			{
				return VT;
			}
		}

		if (APawn* Pawn = PC->GetPawn())
		{
			if (Pawn->IsA(AMarioCharacter::StaticClass()))
			{
				return Pawn;
			}
		}
	}

	if (ACharacter* C = UGameplayStatics::GetPlayerCharacter(World, 0))
	{
		if (C->IsA(AMarioCharacter::StaticClass()))
		{
			return C;
		}
	}

	for (TActorIterator<AMarioCharacter> It(World); It; ++It)
	{
		AMarioCharacter* M = *It;
		if (IsValid(M))
		{
			return M;
		}
	}

	return nullptr;
}

bool AAttrenashinBoss::IsPhase2FistSpinWindow() const
{
	return bPhase2FlowActive && (Phase2State == EPhase2FlowState::Spin);
}

bool AAttrenashinBoss::IsPhase3ClapWindow() const
{
	return bPhase3FlowMode && bPhase2FlowActive && (Phase2State == EPhase2FlowState::Clap);
}

void AAttrenashinBoss::TryStartPhase1Slam()
{
	AActor* Target = GetPlayerTarget();
	if (!Target) return;

	AAttrenashinFist* Primary = bNextLeft ? LeftFist.Get() : RightFist.Get();
	AAttrenashinFist* Secondary = bNextLeft ? RightFist.Get() : LeftFist.Get();

	auto CanStart = [](AAttrenashinFist* F)
	{
		if (!F) return false;
		if (F->IsCapturedDriving()) return false;
		if (F->IsStunned()) return false;
		if (!F->IsIdle()) return false;
		return true;
	};

	AAttrenashinFist* Use = CanStart(Primary) ? Primary : (CanStart(Secondary) ? Secondary : nullptr);
	if (!Use) return;

	Use->StartSlamSequence(Target);
	bNextLeft = !bNextLeft;
}

void AAttrenashinBoss::StartIceRainByFist(bool bUseLeftFist)
{
	AAttrenashinFist* F = bUseLeftFist ? LeftFist.Get() : RightFist.Get();
	if (!F) return;

	if (F->IsIdle())
	{
		F->StartIceRainSlam();
	}
}


void AAttrenashinBoss::StartIceRainByBothFists()
{
	AAttrenashinFist* L = LeftFist.Get();
	AAttrenashinFist* R = RightFist.Get();
	if (!L || !R) return;

	//샤드레인 공격은 양손 동시 시작
	if (L->IsIdle() && R->IsIdle())
	{
		L->StartIceRainSlam();
		R->StartIceRainSlam();
	}
}


void AAttrenashinBoss::OnHeadBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                          UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                                          bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this)
	{
		return;
	}

	// 1) 플레이어 접촉 데미지
	if (AMarioCharacter* Mario = Cast<AMarioCharacter>(OtherActor))
	{
		const UWorld* World = GetWorld();
		const float Now = World ? World->GetTimeSeconds() : 0.f;
		if ((Now - LastHeadContactDamageTime) >= HeadContactDamageCooldown)
		{
			UGameplayStatics::ApplyDamage(
				Mario,
				HeadContactDamage,
				GetInstigatorController(),
				this,
				nullptr);

			LastHeadContactDamageTime = Now;
		}
		return;
	}

	// 2) 캡쳐 주먹 머리 타격 카운트
	AAttrenashinFist* Fist = Cast<AAttrenashinFist>(OtherActor);
	if (!Fist) return;

	if (bRequireCapturedFistForHeadHit && !Fist->IsCapturedDriving())
		return;

	HeadHitCount++;
	
	if (Fist->IsCapturedDriving())
	{
		if (Phase == EAttrenashinPhase::Phase1 && !bPhase2FlowActive)
		{
			BeginPhase2FromHeadHit(Fist);
		}
		else if (Phase == EAttrenashinPhase::Phase2 && bPhase2FlowActive && bPromoteToPhase3OnHeadHitDuringPhase2)
		{
			BeginPhase3FromHeadHit(Fist);
		}

		Fist->ForceReleaseCaptureByBoss();
		BoostMarioTowardWorldOrigin();
	}
}

void AAttrenashinBoss::PerformGroundSlam_IceShardRain()
{
	const FVector Center(0.f, 0.f, IceRainWorldCenterZ);
	SpawnIceShardsAt(Center);
}

void AAttrenashinBoss::SpawnIceShardsAt(const FVector& Center)
{
	if (!GetWorld()) return;
	if (!IceShardClass) return;

	FActorSpawnParameters SP;
	SP.Owner = this;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (int32 i = 0; i < IceShardCount; ++i)
	{
		const float Angle = FMath::FRandRange(0.f, 2.f * PI);
		const float Radius = FMath::Sqrt(FMath::FRand()) * IceRainWorldRadius;

		const float X = FMath::Cos(Angle) * Radius;
		const float Y = FMath::Sin(Angle) * Radius;
		const float ZJitter = FMath::FRandRange(-IceShardSpawnHeightJitter, IceShardSpawnHeightJitter);

		const FVector SpawnLoc(Center.X + X, Center.Y + Y, Center.Z + IceShardSpawnHeight + ZJitter);

		AIceShardActor* Shard = GetWorld()->SpawnActor<AIceShardActor>(IceShardClass, SpawnLoc, FRotator::ZeroRotator, SP);
		if (Shard)
		{
			Shard->InitShard(this, IceShardDamage, IceTileClass, IceTileSpawnZOffset);
		}
	}
}

void AAttrenashinBoss::NotifyFistCaptured(AAttrenashinFist* CapturedFist)
{
	if (!CapturedFist) return;

	// Phase1/2/3 공통: 주먹 캡쳐 시 보스 후퇴 + 카운터 배러지
	if (Phase != EAttrenashinPhase::Phase1 &&
		Phase != EAttrenashinPhase::Phase2 &&
		Phase != EAttrenashinPhase::Phase3)
	{
		return;
	}

	// Phase1에서 캡쳐 실패 후 복귀 타이머/이동이 걸려있다면 캡쳐가 다시 시작되는 순간 취소.
	CancelPhase1CaptureFailReturnToCenter();

	bIsInFear = true;
	StartRetreatMotion();
	StartCaptureBarrage(CapturedFist);
}

void AAttrenashinBoss::NotifyFistReleased(AAttrenashinFist* ReleasedFist)
{
	if (!ReleasedFist) return;

	if (Phase != EAttrenashinPhase::Phase1 &&
		Phase != EAttrenashinPhase::Phase2 &&
		Phase != EAttrenashinPhase::Phase3)
	{
		return;
	}

	const bool bWasInFear = bIsInFear;
	const bool bWasRetreating = bRetreating;
	const bool bWasBarrageActive = bCaptureBarrageActive;

	if (BarrageCapturedFist.Get() == ReleasedFist)
	{
		StopCaptureBarrage();
	}

	if ((!LeftFist.IsValid() || !LeftFist->IsCapturedDriving()) &&
		(!RightFist.IsValid() || !RightFist->IsCapturedDriving()))
	{
		bIsInFear = false;
		bRetreating = false;
		SetWorldDynamicIgnoredForRetreat(false);
		
		// 3초 뒤 보스/손이 맵 중앙으로 동일 속도(Phase2 복귀 속도)로 복귀.
		if (Phase == EAttrenashinPhase::Phase1 && !bPhase2FlowActive && (bWasInFear || bWasRetreating || bWasBarrageActive))
		{
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().ClearTimer(Phase1CaptureFailReturnTimerHandle);
				World->GetTimerManager().SetTimer(
					Phase1CaptureFailReturnTimerHandle,
					this,
					&AAttrenashinBoss::BeginPhase1CaptureFailReturnToCenter,
					FMath::Max(0.f, Phase1CaptureFailReturnDelaySeconds),
					false);
			}
		}
	}
}

void AAttrenashinBoss::NotifyBarrageShardHitCapturedFist(AAttrenashinFist* HitFist, const FVector& ShardVelocity)
{
	if (Phase != EAttrenashinPhase::Phase1 &&
		Phase != EAttrenashinPhase::Phase2 &&
		Phase != EAttrenashinPhase::Phase3)
	{
		return;
	}
	if (!bCaptureBarrageActive) return;
	if (!HitFist) return;
	if (HitFist != BarrageCapturedFist.Get()) return;
	if (!HitFist->IsCapturedDriving()) return;

	++BarrageHitCount;

	HitFist->ApplyCapturedShardKnockback(ShardVelocity, CaptureBarrageKnockbackStrength, CaptureBarrageKnockbackUp);

	if (BarrageHitCount >= FMath::Max(1, CaptureBarrageHitsToForceRelease))
	{
		HitFist->ForceReleaseCaptureByBoss();
		StopCaptureBarrage();
	}
}

void AAttrenashinBoss::StartCaptureBarrage(AAttrenashinFist* CapturedFist)
{
	if (!CapturedFist) return;
	if (!CapturedFist->IsCapturedDriving()) return;

	StopCaptureBarrage();

	AAttrenashinFist* Throwing = nullptr;
	if (CapturedFist == LeftFist.Get())
	{
		Throwing = RightFist.Get();
	}
	else if (CapturedFist == RightFist.Get())
	{
		Throwing = LeftFist.Get();
	}

	if (!Throwing) return;
	
	Throwing->AbortAttackAndReturnForBarrage(0.20f);

	BarrageCapturedFist = CapturedFist;
	BarrageThrowingFist = Throwing;
	BarrageHitCount = 0;
	bCaptureBarrageActive = true;

	// 정확히 1초 주기 발사
	const float Period = FMath::Max(0.1f, CaptureBarrageIntervalSeconds);
	GetWorldTimerManager().SetTimer(
		CaptureBarrageTimerHandle,
		this,
		&AAttrenashinBoss::HandleCaptureBarrageTick,
		Period,
		true,
		Period);
}

void AAttrenashinBoss::StopCaptureBarrage()
{
	GetWorldTimerManager().ClearTimer(CaptureBarrageTimerHandle);
	BarrageCapturedFist.Reset();
	BarrageThrowingFist.Reset();
	BarrageHitCount = 0;
	bCaptureBarrageActive = false;
}

FVector AAttrenashinBoss::GetAnchorLocationBySide(EFistSide Side) const
{
	if (Side == EFistSide::Left && FistAnchorL)
	{
		return FistAnchorL->GetComponentLocation();
	}
	if (Side == EFistSide::Right && FistAnchorR)
	{
		return FistAnchorR->GetComponentLocation();
	}
	return GetActorLocation();
}

void AAttrenashinBoss::HandleCaptureBarrageTick()
{
	if (!GetWorld() || !IceShardClass)
	{
		StopCaptureBarrage();
		return;
	}

	AAttrenashinFist* Captured = BarrageCapturedFist.Get();
	AAttrenashinFist* Throwing = BarrageThrowingFist.Get();

	if (!Captured || !Throwing || !Captured->IsCapturedDriving())
	{
		StopCaptureBarrage();
		return;
	}

	// 반대손이 다른 패턴으로 빠졌다면 다시 소켓 대기로 강제
	if (!Throwing->IsReturning() && !Throwing->IsIdle())
	{
		Throwing->AbortAttackAndReturnForBarrage(0.15f);
	}
	
	const FVector StartBase = GetAnchorLocationBySide(Throwing->GetFistSide());
	const FVector TargetLoc = GetPlayerTarget() ? GetPlayerTarget()->GetActorLocation() : Captured->GetActorLocation();

	FVector Dir = (TargetLoc - StartBase).GetSafeNormal();
	if (Dir.IsNearlyZero())
	{
		Dir = Throwing->GetActorForwardVector();
	}
	const FVector Start = StartBase + Dir * CaptureBarrageSpawnForwardOffset;
	const FVector Velocity = Dir * CaptureBarrageShardSpeed;

	FActorSpawnParameters SP;
	SP.Owner = this;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AIceShardActor* Shard = GetWorld()->SpawnActor<AIceShardActor>(IceShardClass, Start, Velocity.Rotation(), SP);
	if (!Shard)
	{
		return;
	}

	Shard->InitBarrageShard(this, Captured, Velocity);
}



void AAttrenashinBoss::StartRetreatMotion()
{
	AActor* Target = GetPlayerTarget();
	if (!Target)
	{
		bRetreating = false;
		SetWorldDynamicIgnoredForRetreat(false);
		return;
	}

	RetreatStart = GetActorLocation();

	// 플레이어 반대 방향(XY)
	FVector Away3D = RetreatStart - Target->GetActorLocation();
	Away3D.Z = 0.f;
	if (Away3D.IsNearlyZero())
	{
		Away3D = -GetActorForwardVector();
		Away3D.Z = 0.f;
	}
	Away3D = Away3D.GetSafeNormal();

	const FVector2D Start2D(RetreatStart.X, RetreatStart.Y);
	const FVector2D Away2D(Away3D.X, Away3D.Y);
	const float Limit = FMath::Max(0.f, RetreatWorldRadiusLimit);

	FVector2D Target2D = Start2D + Away2D * FMath::Max(0.f, RetreatDistance);

	if (Limit > 0.f)
	{
		// 7300은 "상한"이 아니라, 후퇴 레이 방향에서의 실제 도달 경계로 사용.
		// Start2D + Away2D * t 가 |.| = Limit 를 만족하는 양수 t를 찾는다.
		const float A = 1.f; // Away2D 정규화 벡터
		const float B = 2.f * FVector2D::DotProduct(Start2D, Away2D);
		const float C = Start2D.SizeSquared() - FMath::Square(Limit);
		const float Disc = B * B - 4.f * A * C;

		if (Disc >= 0.f)
		{
			const float SqrtDisc = FMath::Sqrt(Disc);
			const float T1 = (-B - SqrtDisc) / (2.f * A);
			const float T2 = (-B + SqrtDisc) / (2.f * A);

			float OutwardT = -1.f;
			if (T1 > KINDA_SMALL_NUMBER) OutwardT = T1;
			if (T2 > KINDA_SMALL_NUMBER) OutwardT = FMath::Max(OutwardT, T2);

			if (OutwardT > 0.f)
			{
				Target2D = Start2D + Away2D * OutwardT;
			}
		}
		else
		{
			// 수학적으로 교점이 없으면 기존 방식(fallback): 거리 적용 후 상한 클램프
			if (Target2D.SizeSquared() > FMath::Square(Limit))
			{
				Target2D = Target2D.GetSafeNormal() * Limit;
			}
		}

		// 수치 오차 보정
		if (Target2D.SizeSquared() > FMath::Square(Limit))
		{
			Target2D = Target2D.GetSafeNormal() * Limit;
		}
	}

	RetreatTarget = FVector(Target2D.X, Target2D.Y, RetreatStart.Z);

	RetreatT = 0.f;
	bRetreating = !RetreatStart.Equals(RetreatTarget, 1.f);
	SetWorldDynamicIgnoredForRetreat(bRetreating);
}

void AAttrenashinBoss::UpdateRetreat(float DeltaSeconds)
{
	if (!bRetreating) return;

	const FVector OldLoc = GetActorLocation();
	const float Step = FMath::Max(0.f, RetreatSpeed) * FMath::Max(0.f, DeltaSeconds);
	FVector NewLoc = FMath::VInterpConstantTo(OldLoc, RetreatTarget, DeltaSeconds, FMath::Max(1.f, RetreatSpeed));

	// 안전: overshoot 방지
	if ((RetreatTarget - OldLoc).SizeSquared2D() <= FMath::Square(Step))
	{
		NewLoc = RetreatTarget;
	}

	FVector DeltaMove = NewLoc - OldLoc;
	if (!DeltaMove.IsNearlyZero())
	{
		SetActorLocation(NewLoc, true, nullptr, ETeleportType::TeleportPhysics);
		MoveNonCapturedFistWithBoss(DeltaMove);
	}

	if (NewLoc.Equals(RetreatTarget, 1.f))
	{
		bRetreating = false;
		SetWorldDynamicIgnoredForRetreat(false);
	}
}


void AAttrenashinBoss::SetWorldDynamicIgnoredForRetreat(bool bIgnore)
{
	if (bIgnore)
	{
		if (bWorldDynamicIgnoredForRetreat)
		{
			return;
		}

		CachedWorldDynamicResponses.Empty();
		CachedWorldStaticResponses.Empty();

		TArray<UPrimitiveComponent*> PrimComps;
		GetComponents<UPrimitiveComponent>(PrimComps);

		for (UPrimitiveComponent* Prim : PrimComps)
		{
			if (!Prim) continue;

			// 현재 응답값 캐시 후, Retreat 중에만 WorldDynamic/WorldStatic 무시
			CachedWorldDynamicResponses.Add(Prim, Prim->GetCollisionResponseToChannel(ECC_WorldDynamic));
			CachedWorldStaticResponses.Add(Prim, Prim->GetCollisionResponseToChannel(ECC_WorldStatic));

			Prim->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
			Prim->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
		}

		bWorldDynamicIgnoredForRetreat = true;
		return;
	}

	if (!bWorldDynamicIgnoredForRetreat)
	{
		return;
	}

	for (const TPair<TObjectPtr<UPrimitiveComponent>, TEnumAsByte<ECollisionResponse>>& It : CachedWorldDynamicResponses)
	{
		UPrimitiveComponent* Prim = It.Key.Get();
		if (!Prim) continue;
		Prim->SetCollisionResponseToChannel(ECC_WorldDynamic, static_cast<ECollisionResponse>(It.Value));
	}

	for (const TPair<TObjectPtr<UPrimitiveComponent>, TEnumAsByte<ECollisionResponse>>& It : CachedWorldStaticResponses)
	{
		UPrimitiveComponent* Prim = It.Key.Get();
		if (!Prim) continue;
		Prim->SetCollisionResponseToChannel(ECC_WorldStatic, static_cast<ECollisionResponse>(It.Value));
	}

	CachedWorldDynamicResponses.Empty();
	CachedWorldStaticResponses.Empty();
	bWorldDynamicIgnoredForRetreat = false;
}

void AAttrenashinBoss::BeginPhase1CaptureWindow(AAttrenashinFist* CapturedFist)
{
	// 현재 버전에서는 CaptureBarrage가 캡쳐 창 역할을 대체함
	StartCaptureBarrage(CapturedFist);
}

bool AAttrenashinBoss::ShouldFacePlayerDuringPunch() const
{
	auto IsPunching = [](const AAttrenashinFist* F)
	{
		if (!F) return false;
		const EFistState S = F->GetFistState();
		return (S == EFistState::SlamSequence || S == EFistState::IceRainSlam);
	};

	return IsPunching(LeftFist.Get()) || IsPunching(RightFist.Get());
}

void AAttrenashinBoss::FacePlayerYawOnly(float DeltaSeconds)
{
	AActor* Target = GetPlayerTarget();
	if (!Target) return;

	FVector To = Target->GetActorLocation() - GetActorLocation();
	To.Z = 0.f;
	if (To.IsNearlyZero()) return;

	const FRotator Desired = To.Rotation();
	FRotator Current = GetActorRotation();
	FRotator NewRot = FMath::RInterpTo(Current, FRotator(0.f, Desired.Yaw, 0.f), DeltaSeconds, FMath::Max(0.f, FacePlayerInterpSpeed));
	NewRot.Roll = 0.f;
	NewRot.Pitch = 0.f;
	SetActorRotation(NewRot);
}

void AAttrenashinBoss::MoveNonCapturedFistWithBoss(const FVector& DeltaMove)
{
	if (DeltaMove.IsNearlyZero()) return;

	auto MoveIfNeeded = [&DeltaMove](AAttrenashinFist* F)
	{
		if (!F) return;
		if (F->IsCapturedDriving()) return;
		F->SetActorLocation(F->GetActorLocation() + DeltaMove, false, nullptr, ETeleportType::TeleportPhysics);
	};

	MoveIfNeeded(LeftFist.Get());
	MoveIfNeeded(RightFist.Get());
}

void AAttrenashinBoss::BoostMarioTowardWorldOrigin()
{
	AMarioCharacter* Mario = Cast<AMarioCharacter>(GetPlayerTarget());
	if (!Mario) return;

	FVector DirToOrigin = FVector::ZeroVector - Mario->GetActorLocation();
	DirToOrigin.Z = 0.f;
	DirToOrigin = DirToOrigin.GetSafeNormal();
	if (DirToOrigin.IsNearlyZero())
	{
		DirToOrigin = -Mario->GetActorForwardVector();
		DirToOrigin.Z = 0.f;
		DirToOrigin = DirToOrigin.GetSafeNormal();
	}

	const FVector LaunchVel = DirToOrigin * MarioReturnToOriginBoostXY + FVector(0.f, 0.f, MarioReturnToOriginBoostZ);
	Mario->LaunchCharacter(LaunchVel, true, true);
}

bool AAttrenashinBoss::AreBothFistsIdle() const
{
	const AAttrenashinFist* L = LeftFist.Get();
	const AAttrenashinFist* R = RightFist.Get();
	if (!L || !R)
	{
		return false;
	}

	auto IsReady = [](const AAttrenashinFist* F)
	{
		return F && !F->IsCapturedDriving() && F->IsIdle();
	};

	return IsReady(L) && IsReady(R);
}

void AAttrenashinBoss::ForceFistsReturnToAnchor(float ReturnSeconds)
{
	auto ForceReturn = [ReturnSeconds](AAttrenashinFist* F)
	{
		if (!F) return;
		if (F->IsCapturedDriving()) return;
		F->AbortAttackAndReturnForBarrage(FMath::Max(0.01f, ReturnSeconds));
	};

	ForceReturn(LeftFist.Get());
	ForceReturn(RightFist.Get());
}

FVector AAttrenashinBoss::BuildPhase2OppositeBossLocation() const
{
	// 월드 원점(0,0)을 지나는 지름 반대편 좌표
	return FVector(-Phase2RetreatBossLocation.X, -Phase2RetreatBossLocation.Y, Phase2RetreatBossLocation.Z);
}

void AAttrenashinBoss::BeginPhase2FromHeadHit(AAttrenashinFist* CapturedFist)
{
	BeginAdvancedPhaseFromHeadHit(EAttrenashinPhase::Phase2, CapturedFist, false);
}

void AAttrenashinBoss::BeginPhase3FromHeadHit(AAttrenashinFist* CapturedFist)
{
	BeginAdvancedPhaseFromHeadHit(EAttrenashinPhase::Phase3, CapturedFist, true);
}

void AAttrenashinBoss::BeginAdvancedPhaseFromHeadHit(EAttrenashinPhase TargetPhase, AAttrenashinFist* CapturedFist, bool bUsePhase3Rules)
{
	const EAttrenashinPhase PrevPhase = Phase;
	const bool bPromotingPhase2ToPhase3 =
		(TargetPhase == EAttrenashinPhase::Phase3) &&
		(PrevPhase == EAttrenashinPhase::Phase2) &&
		bPhase2FlowActive;

	if (bUsePhase3Rules)
	{
		if (Phase == EAttrenashinPhase::Phase3)
		{
			return;
		}
	}
	else
	{
		if (bPhase2FlowActive)
		{
			return;
		}
	}

	// Phase1 캡쳐 실패 복귀(타이머/이동)가 걸려있다면, 상위 Phase 진입 시 무조건 취소.
	CancelPhase1CaptureFailReturnToCenter();

	EnterPhase(TargetPhase);
	bPhase2FlowActive = true;
	bPhase3FlowMode = bUsePhase3Rules;
	bIsInFear = false;
	bRetreating = false;
	SetWorldDynamicIgnoredForRetreat(false);
	StopCaptureBarrage();

	Phase2RetreatBossLocation = GetActorLocation();
	Phase2CenterBossLocation = FVector(0.f, 0.f, Phase2RetreatBossLocation.Z);
	Phase2ReturnBossStartLocation = Phase2RetreatBossLocation;

	if (HeadHitSphere)
	{
		Phase2HeadRetreatWorldLocation = HeadHitSphere->GetComponentLocation();
	}
	else
	{
		Phase2HeadRetreatWorldLocation = GetActorLocation();
	}

	FVector BackDir = -GetActorForwardVector();
	BackDir.Z = 0.f;
	BackDir = BackDir.GetSafeNormal();
	if (BackDir.IsNearlyZero())
	{
		BackDir = -FVector::ForwardVector;
	}

	Phase2HeadVelocity = BackDir * Phase2HeadLaunchBackSpeed + FVector(0.f, 0.f, Phase2HeadLaunchUpSpeed);
	Phase3ClapElapsed = 0.f;
	Phase3ClapsDone = 0;
	Phase3TrackIndex = 0;
	Phase3ClapSubState = EPhase3ClapSubState::Tracking;
	Phase3ClapStepElapsed = 0.f;
	bPhase3ClapDelayActive = false;
	bPhase3ClapClosing = true;

	const float AbortReturnSeconds = bPromotingPhase2ToPhase3
		? FMath::Max(0.01f, Phase3TransitionFistReturnSeconds)
		: 0.18f;

	// 양손 상태 정리(캡쳐된 손은 해제 콜백으로 정리됨)
	auto AbortIfNeeded = [CapturedFist, AbortReturnSeconds](AAttrenashinFist* F)
	{
		if (!F) return;
		if (F == CapturedFist) return;
		if (F->IsCapturedDriving()) return;
		F->AbortAttackAndReturnForBarrage(AbortReturnSeconds);
	};
	AbortIfNeeded(LeftFist.Get());
	AbortIfNeeded(RightFist.Get());

	EnterPhase2State(EPhase2FlowState::HeadLaunch);
}

void AAttrenashinBoss::EnterPhase2State(EPhase2FlowState NewState)
{
	Phase2State = NewState;
	Phase2StateElapsed = 0.f;

	auto SetFistSpinAnimFlag = [this](bool bActive)
	{
		if (LeftFist.IsValid())
		{
			LeftFist->SetPhase2SpinAnimActive(bActive);
		}
		if (RightFist.IsValid())
		{
			RightFist->SetPhase2SpinAnimActive(bActive);
		}
	};

	auto SetFistPhase3ClapAnimFlag = [this](bool bActive)
	{
		if (LeftFist.IsValid())
		{
			LeftFist->SetPhase3ClapAnimActive(bActive);
		}
		if (RightFist.IsValid())
		{
			RightFist->SetPhase3ClapAnimActive(bActive);
		}
	};

	SetFistSpinAnimFlag(NewState == EPhase2FlowState::Spin);
	SetFistPhase3ClapAnimFlag(bPhase3FlowMode && NewState == EPhase2FlowState::Clap);

	// Phase3 박수 패턴(Clap) 동안은 손이 플레이어의 높이(Z)를 포함해 실시간 추적해야 한다.
	// 비캡쳐 손은 Flying 모드로 전환해 바닥 스냅/걷기 제약을 제거한다.
	const bool bBossWantsFlying = (bPhase3FlowMode && NewState == EPhase2FlowState::Clap);
	auto ApplyMoveMode = [bBossWantsFlying](AAttrenashinFist* F)
	{
		if (!F) return;
		if (F->IsCapturedDriving()) return;
		if (UCharacterMovementComponent* Move = F->GetCharacterMovement())
		{
			Move->SetMovementMode(bBossWantsFlying ? MOVE_Flying : MOVE_Walking);
		}
	};
	ApplyMoveMode(LeftFist.Get());
	ApplyMoveMode(RightFist.Get());

	switch (NewState)
	{
	case EPhase2FlowState::HeadLaunch:
		break;

	case EPhase2FlowState::HeadReturn:
		Phase2HeadReturnStartWorldLocation = HeadHitSphere ? HeadHitSphere->GetComponentLocation() : GetActorLocation();
		break;

	case EPhase2FlowState::Spin:
	{
		const float SpinPrepSeconds = bPhase3FlowMode
			? FMath::Max(0.01f, Phase3SpinPrepReturnSeconds)
			: 0.25f;
		ForceFistsReturnToAnchor(SpinPrepSeconds);
		break;
	}

	case EPhase2FlowState::Dash:
		StartPhase2Dash();
		break;

	case EPhase2FlowState::Clap:
		Phase3ClapElapsed = 0.f;
		Phase3ClapsDone = 0;
		Phase3TrackIndex = 0;
		Phase3ClapSubState = EPhase3ClapSubState::Tracking;
		Phase3ClapStepElapsed = 0.f;
		bPhase3ClapDelayActive = false;
		bPhase3ClapClosing = true;
		CachePhase3ClapTargets();
		break;

	case EPhase2FlowState::ReturnToCenter:
		Phase2ReturnBossStartLocation = GetActorLocation();
		ForceFistsReturnToAnchor(Phase2ReturnToCenterSeconds);
		break;

	case EPhase2FlowState::Rest:
		break;

	case EPhase2FlowState::ShardRain:
		bPhase2ShardRainStarted = false;
		// Clap/Tracking 후에도 양손이 샤드레인 시퀀스로 정상 진입하도록 앵커 복귀 보장
		ForceFistsReturnToAnchor(FMath::Max(0.01f, Phase2ReturnToCenterSeconds));
		break;

	case EPhase2FlowState::AlternatingSlamLoop:
		Phase2AlternatingSlamTimer = 0.f;
		break;

	default:
		break;
	}
}

void AAttrenashinBoss::StartPhase2Dash()
{
	AAttrenashinFist* L = LeftFist.Get();
	AAttrenashinFist* R = RightFist.Get();

	Phase2DashStartL = L ? L->GetActorLocation() : FVector::ZeroVector;
	Phase2DashStartR = R ? R->GetActorLocation() : FVector::ZeroVector;

	const FVector OppositeBossLoc = BuildPhase2OppositeBossLocation();
	const FVector LOffset = FistAnchorL ? (FistAnchorL->GetComponentLocation() - GetActorLocation()) : FVector::ZeroVector;
	const FVector ROffset = FistAnchorR ? (FistAnchorR->GetComponentLocation() - GetActorLocation()) : FVector::ZeroVector;

	Phase2DashTargetL = OppositeBossLoc + LOffset;
	Phase2DashTargetR = OppositeBossLoc + ROffset;
}

void AAttrenashinBoss::CachePhase3ClapTargets()
{
	AActor* Target = GetPlayerTarget();

	// NOTE:
	// "마리오 양쪽 트래킹"은 마리오의 회전(ActorRightVector)이 아니라,
	// 보스(머리) -> 마리오 방향을 기준으로 좌/우를 정의해야 한다.
	const FVector PlayerLoc = Target ? Target->GetActorLocation() : GetActorLocation();

	const FVector BossPoint = HeadHitSphere ? HeadHitSphere->GetComponentLocation() : GetActorLocation();

	FVector Dir = PlayerLoc - BossPoint;
	Dir.Z = 0.f;
	Dir = Dir.GetSafeNormal();

	if (Dir.IsNearlyZero())
	{
		Dir = GetActorForwardVector();
		Dir.Z = 0.f;
		Dir = Dir.GetSafeNormal();

		if (Dir.IsNearlyZero())
		{
			Dir = FVector::ForwardVector;
		}
	}

	// Right axis perpendicular to (Boss -> Player) direction
	FVector Right(-Dir.Y, Dir.X, 0.f);
	Right = Right.GetSafeNormal();
	if (Right.IsNearlyZero())
	{
		Right = FVector::RightVector;
	}

	const float SideOffset = FMath::Max(0.f, Phase3TrackSideOffset);
	const float ZOffset = Phase3TrackHeightOffset;

	FVector Base = PlayerLoc;
	Base.Z += ZOffset;

	Phase3ClapOpenL = Base - Right * SideOffset;
	Phase3ClapOpenR = Base + Right * SideOffset;

	const FVector Mid = (Phase3ClapOpenL + Phase3ClapOpenR) * 0.5f;
	const float HalfDist = FMath::Max(0.f, Phase3ClapContactHalfDistance);
	Phase3ClapCloseL = Mid - Right * HalfDist;
	Phase3ClapCloseR = Mid + Right * HalfDist;
	Phase3ClapCloseL.Z = Mid.Z;
	Phase3ClapCloseR.Z = Mid.Z;
}

void AAttrenashinBoss::UpdatePhase3Clap(float DeltaSeconds)
{
	AAttrenashinFist* L = LeftFist.Get();
	AAttrenashinFist* R = RightFist.Get();
	if (!L && !R)
	{
		EnterPhase2State(EPhase2FlowState::ShardRain);
		return;
	}


	AActor* TargetActor = GetPlayerTarget();
	// Phase3 박수 패턴 동안 머리(보스)가 항상 마리오를 바라보도록 Yaw만 보정
	if (TargetActor)
	{
		FVector ToTarget = TargetActor->GetActorLocation() - GetActorLocation();
		ToTarget.Z = 0.f;
		if (!ToTarget.IsNearlyZero())
		{
			const FRotator Desired = ToTarget.Rotation();
			const FRotator Current = GetActorRotation();
			const float LookSpeed = FMath::Max(30.f, FacePlayerInterpSpeed);
			const FRotator NewRot = FMath::RInterpTo(Current, FRotator(0.f, Desired.Yaw, 0.f), DeltaSeconds, LookSpeed);
			SetActorRotation(NewRot);
		}
	}

	auto SetFistLoc = [](AAttrenashinFist* F, const FVector& Loc)
	{
		if (!F) return;
		if (F->IsCapturedDriving()) return;
		F->SetActorLocation(Loc, false, nullptr, ETeleportType::TeleportPhysics);
	};

	auto MoveFistToward = [DeltaSeconds](AAttrenashinFist* F, const FVector& Target, float Speed)
	{
		if (!F) return;
		if (F->IsCapturedDriving()) return;
		const FVector Cur = F->GetActorLocation();
		const FVector NewLoc = FMath::VInterpConstantTo(Cur, Target, DeltaSeconds, FMath::Max(1.f, Speed));
		F->SetActorLocation(NewLoc, false, nullptr, ETeleportType::TeleportPhysics);
	};

	const int32 TargetClapCount = FMath::Max(1, Phase3ClapCount);
	const int32 MaxTrackIndex = FMath::Max(0, TargetClapCount - 1);

	switch (Phase3ClapSubState)
	{
	case EPhase3ClapSubState::Tracking:
	{
		CachePhase3ClapTargets(); // 마리오 양옆 위치를 계속 갱신
		MoveFistToward(L, Phase3ClapOpenL, Phase3TrackMoveSpeed);
		MoveFistToward(R, Phase3ClapOpenR, Phase3TrackMoveSpeed);

		Phase3ClapStepElapsed += FMath::Max(0.f, DeltaSeconds);
		const float TrackDuration = (Phase3TrackIndex <= 0)
			? FMath::Max(0.01f, Phase3FirstTrackSeconds)
			: FMath::Max(0.01f, Phase3NextTrackSeconds);

		if (Phase3ClapStepElapsed >= TrackDuration)
		{
			// 그 자리에서 대기하므로, 현재 위치를 Open 기준으로 고정
			if (L && !L->IsCapturedDriving())
			{
				Phase3ClapOpenL = L->GetActorLocation();
			}
			if (R && !R->IsCapturedDriving())
			{
				Phase3ClapOpenR = R->GetActorLocation();
			}

			// 대기 위치 기준으로 Close 목표 재계산
			FVector Axis = (Phase3ClapOpenR - Phase3ClapOpenL);
			Axis.Z = 0.f;
			Axis = Axis.GetSafeNormal();
			if (Axis.IsNearlyZero())
			{
				Axis = FVector::RightVector;
			}
			const FVector Mid = (Phase3ClapOpenL + Phase3ClapOpenR) * 0.5f;
			const float HalfDist = FMath::Max(0.f, Phase3ClapContactHalfDistance);
			Phase3ClapCloseL = Mid - Axis * HalfDist;
			Phase3ClapCloseR = Mid + Axis * HalfDist;
			Phase3ClapCloseL.Z = Mid.Z;
			Phase3ClapCloseR.Z = Mid.Z;

			Phase3ClapSubState = EPhase3ClapSubState::Hold;
			Phase3ClapStepElapsed = 0.f;
		}
		break;
	}

	case EPhase3ClapSubState::Hold:
	{
		// 고정 위치 대기
		SetFistLoc(L, Phase3ClapOpenL);
		SetFistLoc(R, Phase3ClapOpenR);

		Phase3ClapStepElapsed += FMath::Max(0.f, DeltaSeconds);
		if (Phase3ClapStepElapsed >= FMath::Max(0.01f, Phase3TrackHoldSeconds))
		{
			Phase3ClapSubState = EPhase3ClapSubState::Close;
			Phase3ClapStepElapsed = 0.f;
		}
		break;
	}

	case EPhase3ClapSubState::Close:
	{
		Phase3ClapStepElapsed += FMath::Max(0.f, DeltaSeconds);
		const float T = FMath::Clamp(Phase3ClapStepElapsed / FMath::Max(0.01f, Phase3ClapCloseSeconds), 0.f, 1.f);
		SetFistLoc(L, FMath::Lerp(Phase3ClapOpenL, Phase3ClapCloseL, T));
		SetFistLoc(R, FMath::Lerp(Phase3ClapOpenR, Phase3ClapCloseR, T));

		if (T >= 1.f - KINDA_SMALL_NUMBER)
		{
			Phase3ClapSubState = EPhase3ClapSubState::Open;
			Phase3ClapStepElapsed = 0.f;
		}
		break;
	}

	case EPhase3ClapSubState::Open:
	{
		Phase3ClapStepElapsed += FMath::Max(0.f, DeltaSeconds);
		const float T = FMath::Clamp(Phase3ClapStepElapsed / FMath::Max(0.01f, Phase3ClapOpenSeconds), 0.f, 1.f);
		SetFistLoc(L, FMath::Lerp(Phase3ClapCloseL, Phase3ClapOpenL, T));
		SetFistLoc(R, FMath::Lerp(Phase3ClapCloseR, Phase3ClapOpenR, T));

		if (T >= 1.f - KINDA_SMALL_NUMBER)
		{
			++Phase3ClapsDone;
			if (Phase3ClapsDone >= TargetClapCount)
			{
				EnterPhase2State(EPhase2FlowState::ShardRain);
				return;
			}

			Phase3TrackIndex = FMath::Clamp(Phase3TrackIndex + 1, 0, MaxTrackIndex);
			Phase3ClapSubState = EPhase3ClapSubState::Tracking;
			Phase3ClapStepElapsed = 0.f;
		}
		break;
	}

	default:
		Phase3ClapSubState = EPhase3ClapSubState::Tracking;
		Phase3ClapStepElapsed = 0.f;
		break;
	}

	//박수 패턴 동안 양손도 플레이어를 바라보도록(피치 포함) 실시간 회전
	if (TargetActor)
	{
		const FVector PlayerLoc = TargetActor->GetActorLocation();
		auto AimFistToPlayer = [&PlayerLoc](AAttrenashinFist* F)
		{
			if (!F) return;
			if (F->IsCapturedDriving()) return;
			const FVector From = F->GetActorLocation();
			FRotator LookRot = UKismetMathLibrary::FindLookAtRotation(From, PlayerLoc);
			LookRot.Roll = 0.f;
			F->SetActorRotation(LookRot);
		};
		AimFistToPlayer(L);
		AimFistToPlayer(R);
	}
}

void AAttrenashinBoss::UpdatePhase2(float DeltaSeconds)
{
	Phase2StateElapsed += FMath::Max(0.f, DeltaSeconds);

	AAttrenashinFist* L = LeftFist.Get();
	AAttrenashinFist* R = RightFist.Get();

	switch (Phase2State)
	{
	case EPhase2FlowState::HeadLaunch:
	{
		if (!HeadHitSphere)
		{
			EnterPhase2State(EPhase2FlowState::HeadReturn);
			break;
		}

		FVector HeadLoc = HeadHitSphere->GetComponentLocation();
		Phase2HeadVelocity.Z -= Phase2HeadGravity * DeltaSeconds;
		HeadLoc += Phase2HeadVelocity * DeltaSeconds;
		HeadHitSphere->SetWorldLocation(HeadLoc, false, nullptr, ETeleportType::TeleportPhysics);

		const bool bMinTimePassed = Phase2StateElapsed >= Phase2HeadLaunchMinSeconds;
		const bool bFalling = Phase2HeadVelocity.Z <= 0.f;
		const bool bBelowThreshold = HeadLoc.Z <= (Phase2HeadRetreatWorldLocation.Z - Phase2HeadFallDepthForReturn);

		if (bMinTimePassed && bFalling && bBelowThreshold)
		{
			EnterPhase2State(EPhase2FlowState::HeadReturn);
		}
		break;
	}

	case EPhase2FlowState::HeadReturn:
	{
		if (!HeadHitSphere)
		{
			EnterPhase2State(EPhase2FlowState::Spin);
			break;
		}

		const float T = FMath::Clamp(Phase2StateElapsed / FMath::Max(0.01f, Phase2HeadReturnSeconds), 0.f, 1.f);
		const FVector NewHeadLoc = FMath::Lerp(Phase2HeadReturnStartWorldLocation, Phase2HeadRetreatWorldLocation, T);
		HeadHitSphere->SetWorldLocation(NewHeadLoc, false, nullptr, ETeleportType::TeleportPhysics);

		if (T >= 1.f - KINDA_SMALL_NUMBER)
		{
			EnterPhase2State(EPhase2FlowState::Spin);
		}
		break;
	}

	case EPhase2FlowState::Spin:
	{
		if (Phase2StateElapsed >= Phase2FistSpinSeconds)
		{
			EnterPhase2State(EPhase2FlowState::Dash);
		}
		break;
	}

	case EPhase2FlowState::Dash:
	{
		const float DashSpeed = FMath::Max(0.f, Phase2FistDashSpeed);

		if (DashSpeed > KINDA_SMALL_NUMBER)
		{
			auto DashFistBySpeed = [DashSpeed, DeltaSeconds](AAttrenashinFist* F, const FVector& Target)
			{
				if (!F) return true;
				if (F->IsCapturedDriving()) return false;

				const FVector Cur = F->GetActorLocation();
				const FVector NewLoc = FMath::VInterpConstantTo(Cur, Target, DeltaSeconds, DashSpeed);
				F->SetActorLocation(NewLoc, false, nullptr, ETeleportType::TeleportPhysics);

				const bool bArrived = NewLoc.Equals(Target, 2.f);
				if (bArrived)
				{
					F->SetActorLocation(Target, true, nullptr, ETeleportType::TeleportPhysics);
				}
				return bArrived;
			};

			const bool bLArrived = DashFistBySpeed(L, Phase2DashTargetL);
			const bool bRArrived = DashFistBySpeed(R, Phase2DashTargetR);

			if (bLArrived && bRArrived)
			{
				EnterPhase2State(EPhase2FlowState::ReturnToCenter);
			}
		}
		else
		{
			const float T = FMath::Clamp(Phase2StateElapsed / FMath::Max(0.01f, Phase2FistDashSeconds), 0.f, 1.f);

			auto DashFistByTime = [T](AAttrenashinFist* F, const FVector& Start, const FVector& Target)
			{
				if (!F) return;
				if (F->IsCapturedDriving()) return;
				const FVector P2 = FMath::Lerp(Start, Target, T);
				F->SetActorLocation(P2, true, nullptr, ETeleportType::TeleportPhysics);
			};

			DashFistByTime(L, Phase2DashStartL, Phase2DashTargetL);
			DashFistByTime(R, Phase2DashStartR, Phase2DashTargetR);

			if (T >= 1.f - KINDA_SMALL_NUMBER)
			{
				EnterPhase2State(EPhase2FlowState::ReturnToCenter);
			}
		}
		break;
	}


	case EPhase2FlowState::Clap:
	{
		UpdatePhase3Clap(DeltaSeconds);
		break;
	}

	case EPhase2FlowState::ReturnToCenter:
	{
		const float T = FMath::Clamp(Phase2StateElapsed / FMath::Max(0.01f, Phase2ReturnToCenterSeconds), 0.f, 1.f);
		const FVector Current = GetActorLocation();
		const FVector NewLoc = FMath::Lerp(Phase2ReturnBossStartLocation, Phase2CenterBossLocation, T);
		const FVector DeltaMove = NewLoc - Current;

		if (!DeltaMove.IsNearlyZero())
		{
			SetActorLocation(NewLoc, true, nullptr, ETeleportType::TeleportPhysics);
			MoveNonCapturedFistWithBoss(DeltaMove);
		}

		if (T >= 1.f - KINDA_SMALL_NUMBER)
		{
			SetActorLocation(Phase2CenterBossLocation, true, nullptr, ETeleportType::TeleportPhysics);
			if (HeadHitSphere)
			{
				const FVector Offset = Phase2CenterBossLocation - Phase2RetreatBossLocation;
				HeadHitSphere->SetWorldLocation(Phase2HeadRetreatWorldLocation + Offset, false, nullptr, ETeleportType::TeleportPhysics);
			}
			EnterPhase2State(EPhase2FlowState::Rest);
		}
		break;
	}

	case EPhase2FlowState::Rest:
	{
		const float RestSeconds = bPhase3FlowMode
			? FMath::Max(0.f, Phase3RestAfterCenterSeconds)
			: FMath::Max(0.f, Phase2RestAfterCenterSeconds);
		if (Phase2StateElapsed >= RestSeconds)
		{
			EnterPhase2State(bPhase3FlowMode ? EPhase2FlowState::Clap : EPhase2FlowState::ShardRain);
		}
		break;
	}

	case EPhase2FlowState::ShardRain:
	{
		if (!bPhase2ShardRainStarted)
		{
			if (AreBothFistsIdle())
			{
				StartIceRainByBothFists();
				bPhase2ShardRainStarted = true;
			}
		}
		else
		{
			const bool bAnyIceRain = (L && L->IsIceRainSlam()) || (R && R->IsIceRainSlam());
			if (!bAnyIceRain && AreBothFistsIdle())
			{
				EnterPhase2State(EPhase2FlowState::AlternatingSlamLoop);
			}
		}
		break;
	}

	case EPhase2FlowState::AlternatingSlamLoop:
	{
		AAttrenashinFist* CapturedNow = nullptr;
		if (L && L->IsCapturedDriving())
		{
			CapturedNow = L;
		}
		else if (R && R->IsCapturedDriving())
		{
			CapturedNow = R;
		}

		// Phase2/3에서도 캡쳐 중에는 후퇴 + 카운터 배러지 우선
		if (CapturedNow)
		{
			if (!bCaptureBarrageActive || BarrageCapturedFist.Get() != CapturedNow)
			{
				bIsInFear = true;
				StartRetreatMotion();
				StartCaptureBarrage(CapturedNow);
			}
			break;
		}
		else if (bCaptureBarrageActive)
		{
			StopCaptureBarrage();
			bIsInFear = false;
			bRetreating = false;
			SetWorldDynamicIgnoredForRetreat(false);
		}

		Phase2AlternatingSlamTimer -= DeltaSeconds;
		if (Phase2AlternatingSlamTimer <= 0.f)
		{
			const float SlamInterval = bPhase3FlowMode
				? FMath::Max(0.1f, Phase3AlternatingSlamInterval)
				: FMath::Max(0.1f, Phase2AlternatingSlamInterval);
			Phase2AlternatingSlamTimer = SlamInterval;
			TryStartPhase1Slam();
		}
		break;
	}

	default:
		break;
	}
}


void AAttrenashinBoss::EnterPhase(EAttrenashinPhase NewPhase)
{
	Phase = NewPhase;
	if (Phase == EAttrenashinPhase::Phase1)
	{
		bPhase2FlowActive = false;
		bPhase3FlowMode = false;
		Phase2State = EPhase2FlowState::None;
		Phase2StateElapsed = 0.f;
		Phase2AlternatingSlamTimer = 0.f;
		Phase3ClapElapsed = 0.f;
		Phase3ClapsDone = 0;
		Phase3TrackIndex = 0;
		Phase3ClapSubState = EPhase3ClapSubState::Tracking;
		Phase3ClapStepElapsed = 0.f;
		bPhase3ClapDelayActive = false;
		bPhase3ClapClosing = true;
		bPhase2ShardRainStarted = false;

		if (LeftFist.IsValid())
		{
			LeftFist->SetPhase2SpinAnimActive(false);
			LeftFist->SetPhase3ClapAnimActive(false);
		}
		if (RightFist.IsValid())
		{
			RightFist->SetPhase2SpinAnimActive(false);
			RightFist->SetPhase3ClapAnimActive(false);
		}
	}
}

void AAttrenashinBoss::EndPhase1CaptureWindow(bool bSuccess)
{
	bPhase1CaptureWindowActive = false;
	Phase1CaptureResult = bSuccess ? EPhase1CaptureResult::Success : EPhase1CaptureResult::Fail;
	GetWorldTimerManager().ClearTimer(Phase1CaptureWindowTimerHandle);
}

void AAttrenashinBoss::OnPhase1CaptureWindowTimeout()
{
	EndPhase1CaptureWindow(false);
}

void AAttrenashinBoss::CancelPhase1CaptureFailReturnToCenter()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(Phase1CaptureFailReturnTimerHandle);
	}

	bPhase1ReturnToCenterActive = false;
	Phase1ReturnToCenterElapsed = 0.f;
	Phase1ReturnToCenterStart = FVector::ZeroVector;
	Phase1ReturnToCenterTarget = FVector::ZeroVector;
}

void AAttrenashinBoss::BeginPhase1CaptureFailReturnToCenter()
{
	// Phase2 이상으로 넘어간 상태면 Phase1 중앙 복귀를 실행하지 않는다.
	if (Phase != EAttrenashinPhase::Phase1 || bPhase2FlowActive)
	{
		CancelPhase1CaptureFailReturnToCenter();
		return;
	}

	bPhase1ReturnToCenterActive = true;
	Phase1ReturnToCenterElapsed = 0.f;
	Phase1ReturnToCenterStart = GetActorLocation();
	Phase1ReturnToCenterTarget = FVector(0.f, 0.f, Phase1ReturnToCenterStart.Z);

	// 손들은 앵커로 복귀하도록 걸어두고, 보스 이동은 Phase2 복귀 속도와 동일하게 처리.
	ForceFistsReturnToAnchor(FMath::Max(0.01f, Phase2ReturnToCenterSeconds));
}

void AAttrenashinBoss::UpdatePhase1CaptureFailReturnToCenter(float DeltaSeconds)
{
	// 안전장치: Phase1 외엔 실행하지 않음
	if (Phase != EAttrenashinPhase::Phase1)
	{
		CancelPhase1CaptureFailReturnToCenter();
		return;
	}

	Phase1ReturnToCenterElapsed += FMath::Max(0.f, DeltaSeconds);
	const float Duration = FMath::Max(0.01f, Phase2ReturnToCenterSeconds);
	const float T = FMath::Clamp(Phase1ReturnToCenterElapsed / Duration, 0.f, 1.f);

	const FVector CurBossLoc = GetActorLocation();
	const FVector NewBossLoc = FMath::Lerp(Phase1ReturnToCenterStart, Phase1ReturnToCenterTarget, T);
	const FVector DeltaMove = NewBossLoc - CurBossLoc;

	SetActorLocation(NewBossLoc, false, nullptr, ETeleportType::TeleportPhysics);
	MoveNonCapturedFistWithBoss(DeltaMove);

	if (T >= 1.f - KINDA_SMALL_NUMBER)
	{
		bPhase1ReturnToCenterActive = false;
		Phase1ReturnToCenterElapsed = 0.f;
	}
}
