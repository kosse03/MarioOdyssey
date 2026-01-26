#include "MarioCapProjectile.h"

#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/RotatingMovementComponent.h"

AMarioCapProjectile::AMarioCapProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->InitSphereRadius(12.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Collision->SetNotifyRigidBodyCollision(true);
	
	CapMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CapMesh"));
	CapMesh->SetupAttachment(Collision);
	CapMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	
	ProjectileMove = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMove"));
	ProjectileMove->InitialSpeed = 700.f;
	ProjectileMove->MaxSpeed = 700.f;
	ProjectileMove->bRotationFollowsVelocity = false;
	ProjectileMove->ProjectileGravityScale = 0.f; // 직선 발사
	
	ProjectileMove->bIsHomingProjectile = false; // 호밍 끄기
	ProjectileMove->HomingAccelerationMagnitude = HomingAccel;
	ProjectileMove->OnProjectileStop.AddDynamic(this, &AMarioCapProjectile::OnProjectileStopped);
	
	RotMove = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("RotMove"));
	RotMove->RotationRate = FRotator(0.f, 800.f, 0.f); // 회전

	Collision->OnComponentHit.AddDynamic(this, &AMarioCapProjectile::OnCapHit);

	InitialLifeSpan = 10.0f; // 10초 뒤 자동 삭제
}

void AMarioCapProjectile::BeginPlay()
{
	Super::BeginPlay();
	
	OwnerActor = GetOwner();
	
	if (OwnerActor.IsValid()) // owner와 충돌 방지(던질때)
	{
		Collision->IgnoreActorWhenMoving(OwnerActor.Get(), true);
	}
	
	if (UWorld* World = GetWorld()) //일정 시간 후 귀환
	{
		World->GetTimerManager().SetTimer(ReturnTimer, this, &AMarioCapProjectile::StartReturn, OutgoingTime, false);
	}
}

void AMarioCapProjectile::FireInDirection(const FVector& Dir, float Speed)
{
	const FVector D = Dir.GetSafeNormal();
	ProjectileMove->Velocity = D * Speed;
}

void AMarioCapProjectile::OnCapHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	if (!bReturning)
	{
		StartReturn();
	}
}

void AMarioCapProjectile::StartReturn()
{
	if (bReturning) return;
	bReturning = true;

	// Owner가 없으면 그냥 사라지기
	if (!OwnerActor.IsValid())
	{
		Destroy();
		return;
	}
	//귀환 중엔 벽에 박히지 않게 충돌을 무시(또는 QueryOnly)
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	//Pawn만 Overlap로 캐치 연출에 도움
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	// 귀환 단계
	ProjectileMove->bIsHomingProjectile = true;
	ProjectileMove->HomingAccelerationMagnitude = HomingAccel;
	ProjectileMove->MaxSpeed = ReturnMaxSpeed;

	// Homing 타겟은 Owner의 RootComponent
	if (USceneComponent* TargetComp = OwnerActor->GetRootComponent())
	{
		ProjectileMove->HomingTargetComponent = TargetComp;
	}
	// 벽에 박혀서 Stop된 경우, movement를 다시 활성화 + 초기 속도 부여
	const FVector ToOwner = (OwnerActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
	ProjectileMove->Velocity = ToOwner * ReturnMaxSpeed;
	ProjectileMove->Activate(true);
}

void AMarioCapProjectile::OnProjectileStopped(const FHitResult& ImpactResult)
{
	if (!bReturning)
	{
		StartReturn();
	}
}

void AMarioCapProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	if (bReturning && OwnerActor.IsValid())// 귀환 중일때 잡기
	{
		const float Dist = FVector::Dist(GetActorLocation(), OwnerActor->GetActorLocation());
		if (Dist <= CatchDistance)
		{
			Destroy();
		}
	}
}

