#include "MarioCapProjectile.h"

#include "Kismet/GameplayStatics.h"
#include "Capture/CaptureComponent.h"
#include "Capture/CapturableInterface.h"
#include "GameFramework/Pawn.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "TimerManager.h"

AMarioCapProjectile::AMarioCapProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	SetRootComponent(Collision);
	Collision->InitSphereRadius(12.f);

	CapMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CapMesh"));
	CapMesh->SetupAttachment(Collision);
	CapMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ProjectileMove = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMove"));
	ProjectileMove->InitialSpeed = 700.f;
	ProjectileMove->MaxSpeed = 700.f;
	ProjectileMove->bRotationFollowsVelocity = false;
	ProjectileMove->ProjectileGravityScale = 0.f;

	ProjectileMove->bIsHomingProjectile = false;
	ProjectileMove->HomingAccelerationMagnitude = HomingAccel;
	ProjectileMove->OnProjectileStop.AddDynamic(this, &AMarioCapProjectile::OnProjectileStopped);

	RotMove = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("RotMove"));
	RotMove->RotationRate = FRotator(0.f, 1000.f, 0.f);

	Collision->OnComponentHit.AddDynamic(this, &AMarioCapProjectile::OnCapHit);
	Collision->OnComponentBeginOverlap.AddDynamic(this, &AMarioCapProjectile::OnCapBeginOverlap);
	//몇 초 뒤 자동 삭제 금지
	InitialLifeSpan = 0.0f;
}

void AMarioCapProjectile::BeginPlay()
{
	Super::BeginPlay();

	OwnerActor = GetOwner();
	
	Phase = ECapPhase::Outgoing;
	bHoldReleased = false;
	bMinHoverPassed = false;

	// 0.1초 전진 후 Hover
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(OutgoingTimer, this, &AMarioCapProjectile::EnterHover, OutgoingDuration, false);
	}
}

void AMarioCapProjectile::FireInDirection(const FVector& Dir, float Speed)
{
	const FVector D = Dir.GetSafeNormal();
	
	ProjectileMove->MaxSpeed = FMath::Max(ProjectileMove->MaxSpeed, Speed);
	ProjectileMove->Velocity = D * Speed;
}

void AMarioCapProjectile::NotifyHoldReleased()
{
	bHoldReleased = true;

	// Hover 중이고 최소 체공이 이미 지났으면 즉시 귀환
	if (Phase == ECapPhase::Hover && bMinHoverPassed)
	{
		BeginReturn();
	}
	// Outgoing 중이면 Hover 진입 후(최소 체공 조건) 처리되도록 플래그만 유지
}

void AMarioCapProjectile::EnterHover()
{
	if (Phase == ECapPhase::Returning) return; // Returning 중엔 건드리지 않음

	Phase = ECapPhase::Hover;

	//기존 타이머/OutgoingTimer 정리해서 꼬임 방지
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(OutgoingTimer);
		World->GetTimerManager().ClearTimer(MinHoverTimer);
		World->GetTimerManager().ClearTimer(MaxHoverTimer);
	}

	// StopSimulating으로 UpdatedComponent가 끊겼을 수 있으니 Hover에서 복구
	if (ProjectileMove && Collision)
	{
		ProjectileMove->SetUpdatedComponent(Collision);
		ProjectileMove->SetComponentTickEnabled(true);
		ProjectileMove->StopMovementImmediately();
		ProjectileMove->bIsHomingProjectile = false;
	}

	// Hover 타이머 재설정
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(MinHoverTimer, this, &AMarioCapProjectile::OnMinHoverReached, MinHoverTime, false);
		World->GetTimerManager().SetTimer(MaxHoverTimer, this, &AMarioCapProjectile::OnMaxHoverReached, MaxHoverTime, false);
	}
}

void AMarioCapProjectile::OnMinHoverReached()
{
	bMinHoverPassed = true;

	// 키를 이미 뗐다면, 최소 체공 지난 즉시 귀환
	if (bHoldReleased && Phase == ECapPhase::Hover)
	{
		BeginReturn();
	}
}

void AMarioCapProjectile::OnMaxHoverReached()
{
	// 홀드를 계속 누르고 있어도 최대 3초면 무조건 귀환
	if (Phase == ECapPhase::Hover)
	{
		BeginReturn();
	}
}

void AMarioCapProjectile::BeginReturn()
{
	if (Phase == ECapPhase::Returning) return;
	Phase = ECapPhase::Returning;
	
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(OutgoingTimer);
		World->GetTimerManager().ClearTimer(MinHoverTimer);
		World->GetTimerManager().ClearTimer(MaxHoverTimer);
	}

	if (!OwnerActor.IsValid())
	{
		Destroy();
		return;
	}
	if (!ProjectileMove || !Collision)
	{
		Destroy();
		return;
	}
	
	ProjectileMove->SetUpdatedComponent(Collision);
	ProjectileMove->SetComponentTickEnabled(true);
	ProjectileMove->StopMovementImmediately();
	ProjectileMove->Deactivate();
	ProjectileMove->bIsHomingProjectile = false;
	ProjectileMove->HomingTargetComponent = nullptr;
	
	const FVector ToOwner = (OwnerActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
	const FVector TestDelta = ToOwner * 8.f; // 6~12 튜닝

	FHitResult Hit;
	ProjectileMove->SafeMoveUpdatedComponent(TestDelta, GetActorRotation(), true, Hit);

	if (Hit.IsValidBlockingHit() && Hit.Time <= KINDA_SMALL_NUMBER)
	{
		FHitResult PushHit;
		ProjectileMove->SafeMoveUpdatedComponent(Hit.Normal * 8.f, GetActorRotation(), true, PushHit);
	}
}

void AMarioCapProjectile::OnCapHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{

	bHasLastBlockNormal = Hit.bBlockingHit;
	LastBlockNormal = Hit.Normal;

	// 기존 동작 유지: 벽/몬스터에 닿으면 Hover로 들어감
	EnterHover();
}

void AMarioCapProjectile::OnCapBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || !OwnerActor.IsValid()) return;
	if (OtherActor == OwnerActor.Get()) return;

	// 캡쳐 대상(인터페이스 있는 몬스터)만 처리
	if (!OtherActor->GetClass()->ImplementsInterface(UCapturableInterface::StaticClass()))
	{
		return;
	}

	FCaptureContext Ctx;
	Ctx.SourceActor = this;
	Ctx.HitLocation = bFromSweep ? FVector(SweepResult.ImpactPoint) : OtherActor->GetActorLocation();

	if (APawn* OwnerPawn = Cast<APawn>(OwnerActor.Get()))
	{
		Ctx.InstigatorController = OwnerPawn->GetController();
	}

	// Phase 상관없이(Outgoing/Hover/Returning 전부) 캡쳐 시도
	if (UCaptureComponent* CapComp = OwnerActor->FindComponentByClass<UCaptureComponent>())
	{
		if (CapComp->TryCapture(OtherActor, Ctx))
		{
			Destroy();
			return;
		}
	}

	// 캡쳐 실패 시 데미지(기존 정책 유지)
	UGameplayStatics::ApplyDamage(
		OtherActor,
		FailedCaptureDamage,
		Ctx.InstigatorController,
		this,
		UDamageType::StaticClass()
	);

	// 기존 동작 유지: Outgoing일 때만 Hover로 전환
	if (Phase == ECapPhase::Outgoing)
	{
		EnterHover();
	}
}

void AMarioCapProjectile::OnProjectileStopped(const FHitResult& ImpactResult)
{
	bHasLastBlockNormal = ImpactResult.bBlockingHit;
	LastBlockNormal = ImpactResult.Normal;
	if (Phase != ECapPhase::Returning)
	{
		EnterHover();
	}
	else
	{
		// Returning 중인데 Stop이 떠버리면, 강제로 Return 재시작
		BeginReturn();
	}
}

void AMarioCapProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Returning이면 캐치 거리에서 종료(=마리오에게 돌아옴)
	if (Phase == ECapPhase::Returning && OwnerActor.IsValid())
	{
		// 캐치 판정
		const float Dist = FVector::Dist(GetActorLocation(), OwnerActor->GetActorLocation());
		if (Dist <= CatchDistance)
		{
			Destroy();
			return;
		}

		// Return 이동(벽 관통 없이)
		if (!ProjectileMove || !Collision) { Destroy(); return; }

		const FVector ToOwner = OwnerActor->GetActorLocation() - GetActorLocation();
		FVector Dir = ToOwner.GetSafeNormal();

		const FVector Delta = Dir * ReturnSpeed * DeltaTime;

		FHitResult Hit;
		ProjectileMove->SafeMoveUpdatedComponent(Delta, GetActorRotation(), true, Hit);

		if (Hit.IsValidBlockingHit())
		{
			if (Hit.Time <= KINDA_SMALL_NUMBER)
			{
				// 벽에 "딱 붙어서" 막힌 상태면 먼저 떼고 다시 슬라이드 계산
				FHitResult PushHit;
				ProjectileMove->SafeMoveUpdatedComponent(Hit.Normal * 8.f, GetActorRotation(), true, PushHit);
			}
			// 벽에 막히면: 벽면을 따라 미끄러지기(슬라이딩)
			const float Remaining = 1.f - Hit.Time;

			// 기본 슬라이드(벡터를 벽면 평면으로)
			FVector SlideDelta = FVector::VectorPlaneProject(Delta, Hit.Normal) * Remaining;

			// SlideDelta가 너무 작으면(모서리/정면 박힘) 탄젠트 보정: 좌/우 중 Owner 쪽으로 더 유리한 방향 선택
			if (SlideDelta.SizeSquared() < 1.0f)
			{
				const FVector Up = FVector::UpVector;
				FVector Tangent = FVector::CrossProduct(Hit.Normal, Up).GetSafeNormal();
				if (Tangent.IsNearlyZero())
				{
					Tangent = FVector::CrossProduct(Hit.Normal, FVector::RightVector).GetSafeNormal();
				}

				const FVector T1 = Tangent;
				const FVector T2 = -Tangent;

				const float Dot1 = FVector::DotProduct(ToOwner, T1);
				const float Dot2 = FVector::DotProduct(ToOwner, T2);

				const FVector Best = (Dot1 >= Dot2) ? T1 : T2;
				SlideDelta = Best * (ReturnSpeed * DeltaTime * Remaining);
			}

			FHitResult Hit2;
			ProjectileMove->SafeMoveUpdatedComponent(SlideDelta, GetActorRotation(), true, Hit2);
		}
	}
}
