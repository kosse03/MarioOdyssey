#include "Character/Boss/AttrenashinBoss.h"
#include "Character/Boss/AttrenashinFist.h"

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

	HeadHitSphere->OnComponentBeginOverlap.AddDynamic(this, &AAttrenashinBoss::OnHeadBeginOverlap);

	if (FistClass)
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

	SlamAttemptTimer = SlamAttemptInterval;
}

void AAttrenashinBoss::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Phase != EAttrenashinPhase::Phase1) return;

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
