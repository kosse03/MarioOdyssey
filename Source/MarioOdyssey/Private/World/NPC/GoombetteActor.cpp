#include "World/NPC/GoombetteActor.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"

#include "Character/Monster/GoombaCharacter.h"
#include "World/Collectibles/SuperMoonPickup.h"

AGoombetteActor::AGoombetteActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->SetupAttachment(Root);
	Trigger->InitSphereRadius(120.f);
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);

	RewardMoonClass = ASuperMoonPickup::StaticClass();
}

void AGoombetteActor::BeginPlay()
{
	Super::BeginPlay();

	if (Trigger)
	{
		Trigger->OnComponentBeginOverlap.AddDynamic(this, &AGoombetteActor::OnTriggerBeginOverlap);
	}
}

void AGoombetteActor::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                            UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                                            bool bFromSweep, const FHitResult& SweepResult)
{
	if (bConsumed) return;
	if (!OtherActor || !GetWorld()) return;

	// "플레이어가 조종 중인 Pawn"만 인정 (캡쳐 시에는 플레이어 Pawn이 굼바가 됨)
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!PlayerPawn || OtherActor != PlayerPawn)
	{
		return;
	}

	AGoombaCharacter* PlayerGoomba = Cast<AGoombaCharacter>(PlayerPawn);
	if (!PlayerGoomba)
	{
		return; // 굼바 캡쳐 상태가 아니면 무시
	}

	const int32 StackCount = PlayerGoomba->GetStackCount();
	if (StackCount < RequiredStackCount)
	{
		return;
	}

	bConsumed = true;

	// 보상 스폰
	if (RewardMoonClass)
	{
		const FVector SpawnLoc = GetActorLocation() + RewardSpawnOffset;
		const FRotator SpawnRot = GetActorRotation();
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		GetWorld()->SpawnActor<ASuperMoonPickup>(RewardMoonClass, SpawnLoc, SpawnRot, Params);
	}

	// Goombette 제거
	Destroy();
}
