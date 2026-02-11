#include "CheckpointFlagActor.h"

#include "MarioOdyssey/MarioCharacter.h"
#include "MarioCapProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/EngineTypes.h"

ACheckpointFlagActor::ACheckpointFlagActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	FlagMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FlagMesh"));
	FlagMesh->SetupAttachment(Root);
	FlagMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	FlagMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	FlagMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	FlagMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	FlagMesh->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Overlap); // CapProjectile
	FlagMesh->SetGenerateOverlapEvents(true);
	FlagMesh->SetNotifyRigidBodyCollision(true);

	ActivateSphere = CreateDefaultSubobject<USphereComponent>(TEXT("ActivateSphere"));
	ActivateSphere->SetupAttachment(Root);
	ActivateSphere->InitSphereRadius(ActivateRadius);
	ActivateSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ActivateSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	ActivateSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ActivateSphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	ActivateSphere->SetCollisionResponseToChannel(ECC_GameTraceChannel2, ECR_Overlap); // CapProjectile
	ActivateSphere->SetGenerateOverlapEvents(true);

	RespawnPoint = CreateDefaultSubobject<USceneComponent>(TEXT("RespawnPoint"));
	RespawnPoint->SetupAttachment(Root);
}

void ACheckpointFlagActor::BeginPlay()
{
	Super::BeginPlay();

	ActivateSphere->SetSphereRadius(ActivateRadius);

	if (ActivateSphere)
	{
		ActivateSphere->OnComponentBeginOverlap.AddDynamic(this, &ACheckpointFlagActor::OnActivateOverlap);
	}

	if (FlagMesh)
	{
		FlagMesh->OnComponentBeginOverlap.AddDynamic(this, &ACheckpointFlagActor::OnActivateOverlap);
		FlagMesh->OnComponentHit.AddDynamic(this, &ACheckpointFlagActor::OnActivateHit);
	}
}

void ACheckpointFlagActor::OnActivateOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	TryActivateFromActor(OtherActor);
}

void ACheckpointFlagActor::OnActivateHit(UPrimitiveComponent* HitComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	TryActivateFromActor(OtherActor);
}

bool ACheckpointFlagActor::TryActivateFromActor(AActor* SourceActor)
{
	if (!SourceActor)
	{
		return false;
	}
	if (bActivated && bActivateOnlyOnce)
	{
		return false;
	}

	AMarioCharacter* Mario = ResolveMarioFromActor(SourceActor);
	if (!Mario)
	{
		return false;
	}

	bActivated = true;

	const FTransform CheckpointTransform = RespawnPoint ? RespawnPoint->GetComponentTransform() : GetActorTransform();
	Mario->SetCheckpointTransform(CheckpointTransform);

	OnCheckpointActivated(Mario);
	return true;
}

AMarioCharacter* ACheckpointFlagActor::ResolveMarioFromActor(AActor* SourceActor) const
{
	if (!SourceActor)
	{
		return nullptr;
	}

	if (AMarioCharacter* Mario = Cast<AMarioCharacter>(SourceActor))
	{
		return Mario;
	}

	if (const AMarioCapProjectile* Cap = Cast<AMarioCapProjectile>(SourceActor))
	{
		if (AActor* OwnerActor = Cap->GetOwner())
		{
			if (AMarioCharacter* Mario = Cast<AMarioCharacter>(OwnerActor))
			{
				return Mario;
			}
		}
	}

	if (AActor* OwnerActor = SourceActor->GetOwner())
	{
		return Cast<AMarioCharacter>(OwnerActor);
	}

	return nullptr;
}

void ACheckpointFlagActor::OnCheckpointActivated_Implementation(AMarioCharacter* Mario)
{
	// BP에서 연출/이펙트/사운드 확장용
}
