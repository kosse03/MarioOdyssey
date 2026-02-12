#include "World/BossArenaController.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieSceneSequencePlayer.h"

#include "MarioOdyssey/MarioCharacter.h"

ABossArenaController::ABossArenaController()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	EncounterTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("EncounterTrigger"));
	EncounterTrigger->SetupAttachment(SceneRoot);
	EncounterTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	EncounterTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	EncounterTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	EncounterTrigger->SetBoxExtent(FVector(800.f, 800.f, 400.f));
}

void ABossArenaController::BeginPlay()
{
	Super::BeginPlay();

	if (EncounterTrigger)
	{
		EncounterTrigger->OnComponentBeginOverlap.AddDynamic(this, &ABossArenaController::OnEncounterTriggerBeginOverlap);
	}
}

void ABossArenaController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (ArenaState == EBossArenaState::Combat)
	{
		if (AMarioCharacter* Mario = GetMarioPlayer())
		{
			if (Mario->IsGameOverPublic())
			{
				HandlePlayerEliminated();
				return;
			}
		}

		if (ActiveBoss == nullptr || ActiveBoss->IsPendingKillPending())
		{
			ArenaState = EBossArenaState::Cleared;
			bEncounterSequenceRunning = false;
		}
	}
	else if (ArenaState == EBossArenaState::AwaitSpawnDelay || ArenaState == EBossArenaState::Cutscene)
	{
		if (AMarioCharacter* Mario = GetMarioPlayer())
		{
			if (Mario->IsGameOverPublic())
			{
				HandlePlayerEliminated();
				return;
			}
		}
	}
}

bool ABossArenaController::IsValidPlayer(AActor* OtherActor) const
{
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn) return false;
	return Pawn->IsPlayerControlled();
}

AMarioCharacter* ABossArenaController::GetMarioPlayer() const
{
	return Cast<AMarioCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
}

void ABossArenaController::OnEncounterTriggerBeginOverlap(
	UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!IsValidPlayer(OtherActor))
	{
		return;
	}

	if (ArenaState == EBossArenaState::Idle || ArenaState == EBossArenaState::AwaitReEntry)
	{
		StartEncounterSequence();
	}
}

void ABossArenaController::StartEncounterSequence()
{
	if (bEncounterSequenceRunning)
	{
		return;
	}

	bEncounterSequenceRunning = true;
	ArenaState = EBossArenaState::AwaitSpawnDelay;

	GetWorldTimerManager().ClearTimer(SpawnDelayTimerHandle);
	GetWorldTimerManager().SetTimer(
		SpawnDelayTimerHandle,
		this,
		&ABossArenaController::HandleSpawnDelayElapsed,
		FMath::Max(0.0f, BossSpawnDelaySeconds),
		false);
}

void ABossArenaController::HandleSpawnDelayElapsed()
{
	if (ArenaState != EBossArenaState::AwaitSpawnDelay)
	{
		return;
	}

	if (AMarioCharacter* Mario = GetMarioPlayer())
	{
		if (Mario->IsGameOverPublic())
		{
			HandlePlayerEliminated();
			return;
		}
	}

	SpawnOrActivateBoss();
	PlayCutsceneOrEnterCombat();
}

void ABossArenaController::SpawnOrActivateBoss()
{
	if (IsValid(ActiveBoss))
	{
		return;
	}

	if (!BossClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BossArenaController] BossClass is null."));
		return;
	}
	
	const FVector FixedSpawnLocation(0.0f, 0.0f, 1035.0f);
	const FRotator FixedSpawnRotation(0.0f, 180.0f, 0.0f);
	const FTransform SpawnTM(FixedSpawnRotation, FixedSpawnLocation, FVector(1.0f, 1.0f, 1.0f));

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ActiveBoss = GetWorld()->SpawnActor<AActor>(BossClass, SpawnTM, Params);
}

void ABossArenaController::PlayCutsceneOrEnterCombat()
{
	if (EncounterCutsceneActor)
	{
		if (ULevelSequencePlayer* SequencePlayer = EncounterCutsceneActor->GetSequencePlayer())
		{
			ArenaState = EBossArenaState::Cutscene;
			SequencePlayer->OnFinished.RemoveDynamic(this, &ABossArenaController::OnEncounterCutsceneFinished);
			SequencePlayer->OnFinished.AddDynamic(this, &ABossArenaController::OnEncounterCutsceneFinished);
			SequencePlayer->Play();
			return;
		}
	}

	EnterCombat();
}

void ABossArenaController::OnEncounterCutsceneFinished()
{
	if (EncounterCutsceneActor)
	{
		if (ULevelSequencePlayer* SequencePlayer = EncounterCutsceneActor->GetSequencePlayer())
		{
			SequencePlayer->OnFinished.RemoveDynamic(this, &ABossArenaController::OnEncounterCutsceneFinished);
		}
	}

	EnterCombat();
}

void ABossArenaController::EnterCombat()
{
	ArenaState = EBossArenaState::Combat;
	bEncounterSequenceRunning = false;
}

void ABossArenaController::HandlePlayerEliminated()
{
	GetWorldTimerManager().ClearTimer(SpawnDelayTimerHandle);

	if (EncounterCutsceneActor)
	{
		if (ULevelSequencePlayer* SequencePlayer = EncounterCutsceneActor->GetSequencePlayer())
		{
			SequencePlayer->OnFinished.RemoveDynamic(this, &ABossArenaController::OnEncounterCutsceneFinished);
			if (SequencePlayer->IsPlaying())
			{
				SequencePlayer->Stop();
			}
		}
	}

	ClearExistingBoss();

	ArenaState = EBossArenaState::AwaitReEntry;
	bEncounterSequenceRunning = false;
}

void ABossArenaController::ClearExistingBoss()
{
	if (IsValid(ActiveBoss))
	{
		ActiveBoss->Destroy();
	}
	ActiveBoss = nullptr;
}
