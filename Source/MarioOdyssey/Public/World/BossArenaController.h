#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BossArenaController.generated.h"

class USceneComponent;
class UBoxComponent;
class AMarioCharacter;
class ALevelSequenceActor;
class AActor;

UENUM(BlueprintType)
enum class EBossArenaState : uint8
{
	Idle UMETA(DisplayName="Idle"),
	AwaitSpawnDelay UMETA(DisplayName="AwaitSpawnDelay"),
	Cutscene UMETA(DisplayName="Cutscene"),
	Combat UMETA(DisplayName="Combat"),
	AwaitReEntry UMETA(DisplayName="AwaitReEntry"),
	Cleared UMETA(DisplayName="Cleared")
};

UCLASS()
class MARIOODYSSEY_API ABossArenaController : public AActor
{
	GENERATED_BODY()

public:
	ABossArenaController();
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Arena")
	USceneComponent* SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Arena")
	UBoxComponent* EncounterTrigger = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arena")
	TSubclassOf<AActor> BossClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arena")
	AActor* BossSpawnPoint = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arena")
	ALevelSequenceActor* EncounterCutsceneActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arena", meta=(ClampMin="0.0"))
	float BossSpawnDelaySeconds = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arena", meta=(ClampMin="1"))
	int32 BossClearHeadHitCount = 3;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Arena")
	EBossArenaState ArenaState = EBossArenaState::Idle;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Arena")
	AActor* ActiveBoss = nullptr;

	bool bEncounterSequenceRunning = false;
	FTimerHandle SpawnDelayTimerHandle;

	UFUNCTION()
	void OnEncounterTriggerBeginOverlap(
		UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	void StartEncounterSequence();

	UFUNCTION()
	void HandleSpawnDelayElapsed();

	void SpawnOrActivateBoss();
	void PlayCutsceneOrEnterCombat();

	UFUNCTION()
	void OnEncounterCutsceneFinished();

	void EnterCombat();
	void HandlePlayerEliminated();
	void ClearExistingBoss();

	bool IsValidPlayer(AActor* OtherActor) const;
	AMarioCharacter* GetMarioPlayer() const;
};
