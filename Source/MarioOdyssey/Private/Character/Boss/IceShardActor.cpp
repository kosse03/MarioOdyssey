#include "Character/Boss/IceShardActor.h"
#include "Character/Boss/IceTileActor.h"
#include "MarioOdyssey/MarioCharacter.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"

AIceShardActor::AIceShardActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	Sphere->SetupAttachment(Root);
	Sphere->InitSphereRadius(28.f);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Sphere->SetCollisionObjectType(ECC_WorldDynamic);
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Sphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Sphere->SetGenerateOverlapEvents(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Sphere);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ProjectileMove = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMove"));
	ProjectileMove->UpdatedComponent = Sphere;
	ProjectileMove->InitialSpeed = 0.f;
	ProjectileMove->MaxSpeed = 6000.f;
	ProjectileMove->ProjectileGravityScale = GravityScale;
	ProjectileMove->Velocity = FVector(0.f, 0.f, -InitialDownSpeed);
	ProjectileMove->bShouldBounce = false;
	ProjectileMove->bRotationFollowsVelocity = true;

	Sphere->OnComponentBeginOverlap.AddDynamic(this, &AIceShardActor::OnBeginOverlap);
	ProjectileMove->OnProjectileStop.AddDynamic(this, &AIceShardActor::OnProjectileStop);

	InitialLifeSpan = LifeSeconds;
}

void AIceShardActor::InitShard(AActor* InOwnerBoss, float InDamage, TSubclassOf<AIceTileActor> InIceTileClass, float InIceTileZOffset)
{
	OwnerBoss = InOwnerBoss;
	Damage = InDamage;
	IceTileClass = InIceTileClass;
	IceTileZOffset = InIceTileZOffset;

	if (ProjectileMove)
	{
		ProjectileMove->ProjectileGravityScale = GravityScale;
		ProjectileMove->Velocity = FVector(0.f, 0.f, -InitialDownSpeed);
	}
}

void AIceShardActor::OnBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                                    bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this) return;
	if (bDamagedMario) return;

	AMarioCharacter* Mario = Cast<AMarioCharacter>(OtherActor);
	if (!Mario) return;

	UGameplayStatics::ApplyDamage(
		Mario,
		Damage,
		OwnerBoss.IsValid() ? OwnerBoss->GetInstigatorController() : nullptr,
		this,
		nullptr);

	bDamagedMario = true;
}

void AIceShardActor::OnProjectileStop(const FHitResult& ImpactResult)
{
	if (bStopped) return;
	bStopped = true;

	if (UWorld* World = GetWorld())
	{
		if (IceTileClass)
		{
			FVector SpawnLoc = ImpactResult.ImpactPoint;
			SpawnLoc.Z += IceTileZOffset;

			FActorSpawnParameters SP;
			SP.Owner = OwnerBoss.Get();

			World->SpawnActor<AIceTileActor>(IceTileClass, SpawnLoc, FRotator::ZeroRotator, SP);
		}
	}

	Destroy();
}
