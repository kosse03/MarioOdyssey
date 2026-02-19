#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaIntroCutsceneTrigger.generated.h"

class UBoxComponent;
class ALevelSequenceActor;
class ULevelSequencePlayer;
class UMarioGameInstance;

/**
 * ArenaIntroCutsceneTrigger
 * - Arena 맵 바닥에 두는 1회성 컷신 트리거(맵 설명 컷신)
 * - 플레이어(마리오)가 닿으면 지정된 LevelSequenceActor 재생
 * - 최초 1회만 동작(죽어서 리스폰해도 재생 X) : GameInstance 플래그로 관리
 */
UCLASS()
class MARIOODYSSEY_API AArenaIntroCutsceneTrigger : public AActor
{
	GENERATED_BODY()

public:
	AArenaIntroCutsceneTrigger();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category="Cutscene")
	TObjectPtr<UBoxComponent> Trigger = nullptr;

	// 이 트리거가 재생할 LevelSequenceActor(씬에 배치된 액터를 지정)
	UPROPERTY(EditInstanceOnly, Category="Cutscene")
	TObjectPtr<ALevelSequenceActor> IntroSequenceActor = nullptr;

	UPROPERTY(EditAnywhere, Category="Cutscene")
	bool bLockInputDuringCutscene = true;

private:
	bool bPlaying = false;

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	                          UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	                          bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnCutsceneFinished();

	void StartCutscene();
	void SetInputLocked(bool bLocked) const;

	UMarioGameInstance* GetGI() const;
};
