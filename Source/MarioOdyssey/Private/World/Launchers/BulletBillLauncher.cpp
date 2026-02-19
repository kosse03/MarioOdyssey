#include "World/Launchers/BulletBillLauncher.h"

#include "Components/ArrowComponent.h"
#include "Components/CapsuleComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"

#include "Character/Monster/BulletBillCharacter.h"

ABulletBillLauncher::ABulletBillLauncher()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Arrow = CreateDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	Arrow->SetupAttachment(Root);
}

void ABulletBillLauncher::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoStart)
	{
		StartSpawning();
	}
}

void ABulletBillLauncher::StartSpawning()
{
	if (!GetWorld()) return;

	GetWorld()->GetTimerManager().ClearTimer(SpawnTimer);
	GetWorld()->GetTimerManager().SetTimer(SpawnTimer, this, &ABulletBillLauncher::HandleSpawnTick, SpawnInterval, true, 0.f);
}

void ABulletBillLauncher::StopSpawning()
{
	if (!GetWorld()) return;
	GetWorld()->GetTimerManager().ClearTimer(SpawnTimer);
}

void ABulletBillLauncher::HandleSpawnTick()
{
	SpawnOne();
}

ABulletBillCharacter* ABulletBillLauncher::SpawnOne()
{
	if (!GetWorld() || !BulletBillClass)
	{
		return nullptr;
	}

	const FVector SpawnLoc =
		GetActorLocation()
		+ GetActorForwardVector() * SpawnOffset
		+ GetActorUpVector() * SpawnUpOffset;

	const FRotator SpawnRot = GetActorRotation();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	Params.Owner = this;

	ABulletBillCharacter* Spawned = GetWorld()->SpawnActor<ABulletBillCharacter>(BulletBillClass, SpawnLoc, SpawnRot, Params);
	if (!Spawned)
	{
		return nullptr;
	}

	// Launcher 자체와는 충돌 무시(선택)
	if (bIgnoreLauncherCollision)
	{
		if (UCapsuleComponent* Cap = Spawned->GetCapsuleComponent())
		{
			Cap->IgnoreActorWhenMoving(this, true);
		}
	}

	// Spawn grace: 겹쳐 스폰되자마자 터지는 현상 방지
	if (SpawnGraceSeconds > 0.f)
	{
		Spawned->StartSpawnGrace(SpawnGraceSeconds);
	}

	return Spawned;
}
