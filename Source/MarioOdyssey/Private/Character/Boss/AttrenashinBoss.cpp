\
#include "Character/Boss/AttrenashinBoss.h"
#include "Character/Boss/AttrenashinFist.h"
#include "Character/Boss/IceShardActor.h"

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
	HeadHitSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HeadHitSphere->SetCollisionResponseToAllChannels(ECR_Overlap);
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
			StartIceRainByFist(true); // 첫 연출
		});
	SlamAttemptTimer = SlamAttemptInterval;
}

void AAttrenashinBoss::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Phase != EAttrenashinPhase::Phase1) return;

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

void AAttrenashinBoss::OnHeadBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                          UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                                          bool bFromSweep, const FHitResult& SweepResult)
{
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
