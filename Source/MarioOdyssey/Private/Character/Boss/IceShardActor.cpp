#include "Character/Boss/IceShardActor.h"

#include "Character/Boss/AttrenashinBoss.h"
#include "Character/Boss/AttrenashinFist.h"
#include "Character/Boss/IceTileActor.h"
#include "MarioOdyssey/MarioCharacter.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
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

	Mode = EIceShardMode::RainTile;
	bDamageMarioEnabled = true;
	bSpawnTileOnStop = true;
	BarrageCapturedFist.Reset();

	// 일반 얼음비: 월드 충돌 Block, Mario만 Overlap 데미지
	if (Sphere)
	{
		Sphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
		Sphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		Sphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
		Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	}

	if (ProjectileMove)
	{
		ProjectileMove->ProjectileGravityScale = GravityScale;
		ProjectileMove->Velocity = FVector(0.f, 0.f, -InitialDownSpeed);
	}
}

void AIceShardActor::InitBarrageShard(AActor* InOwnerBoss, AAttrenashinFist* InCapturedFist, const FVector& InVelocity)
{
	OwnerBoss = InOwnerBoss;
	Damage = 0.f;
	IceTileClass = nullptr;
	IceTileZOffset = 0.f;

	Mode = EIceShardMode::CaptureBarrage;
	bDamageMarioEnabled = false;
	bSpawnTileOnStop = false;
	BarrageCapturedFist = InCapturedFist;

	// 캡쳐 카운터 샤드:
	// - 월드 충돌 Block
	// - Mario/주먹에는 Overlap(피격 이벤트용)
	if (Sphere)
	{
		Sphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
		Sphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		Sphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
		Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

		if (InCapturedFist && InCapturedFist->GetCapsuleComponent())
		{
			const ECollisionChannel FistObjType = InCapturedFist->GetCapsuleComponent()->GetCollisionObjectType();
			Sphere->SetCollisionResponseToChannel(FistObjType, ECR_Overlap);
		}
	}

	if (ProjectileMove)
	{
		ProjectileMove->ProjectileGravityScale = 0.f;
		ProjectileMove->Velocity = InVelocity;
	}
}

void AIceShardActor::OnBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                                    bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this) return;

	// 캡쳐 카운터 샤드: 캡쳐된 주먹 적중 체크 우선
	if (Mode == EIceShardMode::CaptureBarrage)
	{
		if (AAttrenashinFist* HitFist = Cast<AAttrenashinFist>(OtherActor))
		{
			if (BarrageCapturedFist.IsValid() && HitFist == BarrageCapturedFist.Get())
			{
				if (AAttrenashinBoss* Boss = Cast<AAttrenashinBoss>(OwnerBoss.Get()))
				{
					const FVector V = ProjectileMove ? ProjectileMove->Velocity : GetVelocity();
					Boss->NotifyBarrageShardHitCapturedFist(HitFist, V);
				}
				Destroy();
				return;
			}
			// 던지는 손(반대손) 등 다른 주먹은 무시
			return;
		}

		// 플레이어는 맞아도 데미지 없음
		if (Cast<AMarioCharacter>(OtherActor))
		{
			Destroy();
			return;
		}

		return;
	}

	// 일반 얼음비 샤드: Mario 데미지 1회
	if (bDamagedMario) return;
	if (!bDamageMarioEnabled) return;

	if (AMarioCharacter* Mario = Cast<AMarioCharacter>(OtherActor))
	{
		UGameplayStatics::ApplyDamage(
			Mario,
			Damage,
			OwnerBoss.IsValid() ? OwnerBoss->GetInstigatorController() : nullptr,
			this,
			nullptr);

		bDamagedMario = true;
	}
}

void AIceShardActor::OnProjectileStop(const FHitResult& ImpactResult)
{
	if (bStopped) return;
	bStopped = true;

	// 안전장치: 모드가 CaptureBarrage인데 Block으로 맞은 경우에도 카운트 처리
	if (Mode == EIceShardMode::CaptureBarrage)
	{
		if (AAttrenashinFist* HitFist = Cast<AAttrenashinFist>(ImpactResult.GetActor()))
		{
			if (BarrageCapturedFist.IsValid() && HitFist == BarrageCapturedFist.Get())
			{
				if (AAttrenashinBoss* Boss = Cast<AAttrenashinBoss>(OwnerBoss.Get()))
				{
					const FVector V = ProjectileMove ? ProjectileMove->Velocity : GetVelocity();
					Boss->NotifyBarrageShardHitCapturedFist(HitFist, V);
				}
			}
		}

		Destroy();
		return;
	}

	if (bSpawnTileOnStop)
	{
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
	}

	Destroy();
}
