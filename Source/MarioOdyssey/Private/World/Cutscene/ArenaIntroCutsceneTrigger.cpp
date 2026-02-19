#include "World/Cutscene/ArenaIntroCutsceneTrigger.h"

#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"

#include "MarioOdyssey/MarioCharacter.h"
#include "Progress/MarioGameInstance.h"

AArenaIntroCutsceneTrigger::AArenaIntroCutsceneTrigger()
{
	PrimaryActorTick.bCanEverTick = false;

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	SetRootComponent(Trigger);

	Trigger->InitBoxExtent(FVector(220.f, 220.f, 120.f));

	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionObjectType(ECC_WorldDynamic);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
}

void AArenaIntroCutsceneTrigger::BeginPlay()
{
	Super::BeginPlay();

	if (Trigger)
	{
		Trigger->OnComponentBeginOverlap.AddDynamic(this, &AArenaIntroCutsceneTrigger::OnTriggerBeginOverlap);
	}
}

UMarioGameInstance* AArenaIntroCutsceneTrigger::GetGI() const
{
	return GetGameInstance<UMarioGameInstance>();
}

void AArenaIntroCutsceneTrigger::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                                      UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                                                      bool bFromSweep, const FHitResult& SweepResult)
{
	if (bPlaying) return;

	AMarioCharacter* Mario = Cast<AMarioCharacter>(OtherActor);
	if (!Mario) return;

	UMarioGameInstance* GI = GetGI();
	if (!GI) return;

	// 최초 1회만
	if (GI->IsArenaIntroCutscenePlayed())
	{
		return;
	}

	// 재생 시작 시점에 바로 마킹(도중에 죽거나 맵 리로드해도 재생 방지)
	GI->MarkArenaIntroCutscenePlayed();

	StartCutscene();
}

void AArenaIntroCutsceneTrigger::SetInputLocked(bool bLocked) const
{
	if (!bLockInputDuringCutscene) return;

	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
		{
			PC->SetIgnoreMoveInput(bLocked);
			PC->SetIgnoreLookInput(bLocked);
		}
	}
}

void AArenaIntroCutsceneTrigger::StartCutscene()
{
	bPlaying = true;

	SetInputLocked(true);

	if (IntroSequenceActor && IntroSequenceActor->GetSequencePlayer())
	{
		ULevelSequencePlayer* Player = IntroSequenceActor->GetSequencePlayer();
		Player->OnFinished.RemoveDynamic(this, &AArenaIntroCutsceneTrigger::OnCutsceneFinished);
		Player->OnFinished.AddDynamic(this, &AArenaIntroCutsceneTrigger::OnCutsceneFinished);

		Player->Stop();
		Player->Play();
	}
	else
	{
		// 시퀀스가 없으면 바로 종료 처리
		OnCutsceneFinished();
	}
}

void AArenaIntroCutsceneTrigger::OnCutsceneFinished()
{
	if (!bPlaying) return;
	bPlaying = false;

	SetInputLocked(false);
}
