#include "World/Collectibles/MarioCollectibleBase.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "MarioOdyssey/MarioCharacter.h"

#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

AMarioCollectibleBase::AMarioCollectibleBase()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	SetRootComponent(Sphere);
	Sphere->InitSphereRadius(60.f);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Sphere->SetGenerateOverlapEvents(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Sphere);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AMarioCollectibleBase::BeginPlay()
{
	Super::BeginPlay();

	if (Sphere)
	{
		Sphere->OnComponentBeginOverlap.AddDynamic(this, &AMarioCollectibleBase::OnSphereBeginOverlap);
	}

	// Cache initial state for motion
	if (Mesh)
	{
		InitialMeshRelativeLocation = Mesh->GetRelativeLocation();
	}

	// Slightly desync bobbing so every pickup doesn't move in perfect sync
	BobPhase = FMath::FRandRange(0.f, 2.f * PI);

	const bool bNeedTick =
		(bEnableSpin && !FMath::IsNearlyZero(SpinSpeedDegPerSec)) ||
		(bEnableBob && !FMath::IsNearlyZero(BobAmplitude) && !FMath::IsNearlyZero(BobSpeed));

	SetActorTickEnabled(bNeedTick);
}



void AMarioCollectibleBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!Mesh)
	{
		return;
	}

	MotionTime += DeltaSeconds;

	// Spin around Z (Yaw)
	if (bEnableSpin && !FMath::IsNearlyZero(SpinSpeedDegPerSec))
	{
		const float DeltaYaw = SpinSpeedDegPerSec * DeltaSeconds;
		Mesh->AddRelativeRotation(FRotator(0.f, DeltaYaw, 0.f));
	}

	// Bob up/down on Z
	if (bEnableBob && !FMath::IsNearlyZero(BobAmplitude) && !FMath::IsNearlyZero(BobSpeed))
	{
		FVector NewLoc = InitialMeshRelativeLocation;
		NewLoc.Z += FMath::Sin(MotionTime * BobSpeed + BobPhase) * BobAmplitude;
		Mesh->SetRelativeLocation(NewLoc);
	}
	else
	{
		// Ensure we don't leave the mesh offset if bobbing is disabled at runtime
		Mesh->SetRelativeLocation(InitialMeshRelativeLocation);
	}
}
bool AMarioCollectibleBase::IsValidCollector(AActor* OtherActor) const
{
	return OtherActor && OtherActor->IsA(AMarioCharacter::StaticClass());
}

void AMarioCollectibleBase::OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bCollected)
	{
		return;
	}

	if (!IsValidCollector(OtherActor))
	{
		return;
	}

	bCollected = true;

	// 중복 오버랩 방지
	if (Sphere)
	{
		Sphere->SetGenerateOverlapEvents(false);
		Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// 수집 사운드
	if (CollectSound)
	{
		if (bPlaySound2D)
		{
			UGameplayStatics::PlaySound2D(this, CollectSound, CollectSoundVolume);
		}
		else
		{
			UGameplayStatics::PlaySoundAtLocation(this, CollectSound, GetActorLocation(), CollectSoundVolume);
		}
	}

	OnCollected(OtherActor);
	BP_OnCollected(OtherActor);
	AfterCollected();
}


void AMarioCollectibleBase::AfterCollected()
{
	Destroy();
}
