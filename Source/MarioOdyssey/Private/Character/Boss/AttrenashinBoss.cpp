#include "Character/Boss/AttrenashinBoss.h"
#include "Character/Boss/AttrenashinFist.h"
#include "Character/Boss/IceShardActor.h"
#include "MarioOdyssey/MarioCharacter.h"

#include "Components/SphereComponent.h"
#include "Kismet/GameplayStatics.h"

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
	// 보스 머리 충돌 정책:
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
			StartCaptureBarrage(CapturedNow);
		}
		return; // 캡쳐 중에는 스턴 슬램 루프 중지
	}
	else if (bCaptureBarrageActive)
	{
		StopCaptureBarrage();
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
	return UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
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

	// 요구사항: 샤드레인 공격은 양손 동시 시작
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
	UE_LOG(LogTemp, Warning, TEXT("[AttrenashinBoss] HeadHitCount=%d"), HeadHitCount);
}

void AAttrenashinBoss::PerformGroundSlam_IceShardRain()
{
	FVector Center = GetActorLocation();

	if (bIceShardCenterOnPlayer)
	{
		if (AActor* T = GetPlayerTarget())
		{
			Center = T->GetActorLocation();
		}
	}

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
		const float Radius = FMath::Sqrt(FMath::FRand()) * IceShardRadius;

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
	StartCaptureBarrage(CapturedFist);
}

void AAttrenashinBoss::NotifyFistReleased(AAttrenashinFist* ReleasedFist)
{
	if (!ReleasedFist) return;

	if (BarrageCapturedFist.Get() == ReleasedFist)
	{
		StopCaptureBarrage();
	}
}

void AAttrenashinBoss::NotifyBarrageShardHitCapturedFist(AAttrenashinFist* HitFist, const FVector& ShardVelocity)
{
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

	// 요구사항: 캡쳐 시작 시 반대손은 기존 스턴 패턴을 끊고 소켓 대기로 전환
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

	// 플레이어 방향으로 발사(요구사항)
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
