#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BossArenaController.generated.h"

class UBoxComponent;
class UPrimitiveComponent;
struct FHitResult;
class AAttrenashinBoss;
class AAttrenashinFist;
class AIceShardActor;
class AIceTileActor;
class AMarioCharacter;
class ALevelSequenceActor;
class USoundBase;
class UAudioComponent;
struct FTimerHandle;

UCLASS()
class MARIOODYSSEY_API ABossArenaController : public AActor
{
    GENERATED_BODY()

public:
    ABossArenaController();

    virtual void Tick(float DeltaSeconds) override;

    /** 시퀀서/블루프린트에서 호출할 보스전 BGM 시작(네이티브) */
    UFUNCTION(BlueprintCallable, Category="Boss|Audio")
    void StartBossBGMNative();

    /** 시퀀서/블루프린트에서 호출할 보스전 BGM 정지(네이티브) */
    UFUNCTION(BlueprintCallable, Category="Boss|Audio")
    void StopBossBGMNative();

    UFUNCTION(BlueprintPure, Category="Boss")
    bool IsEncounterActive() const { return BossActor.IsValid() || bIsCutscenePlaying || bWaitingForBossSpawn || bWaitingForEncounterDelay; }

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Boss|Encounter")
    TObjectPtr<UBoxComponent> EncounterTrigger;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Encounter")
    TSubclassOf<AAttrenashinBoss> BossClass;

    /** 컷신에 사용할 Level Sequence Actor(레벨에 배치한 액터 지정) */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Boss|Cutscene")
    TObjectPtr<ALevelSequenceActor> EncounterCutsceneActor = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Cutscene")
    bool bAutoPlayEncounterCutscene = true;

    /** 컷신 시작 시 시퀀서 카메라로 블렌드 시간 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Cutscene", meta=(ClampMin="0.0"))
    float CutsceneCameraBlendInSeconds = 0.20f;

    /** 컷신 종료 후 플레이어 카메라 복귀 블렌드 시간 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Cutscene", meta=(ClampMin="0.0"))
    float CutsceneToPlayerCameraBlendSeconds = 0.65f;

    /** 컷신용 프록시(머리/좌주먹/우주먹 등) 액터들 */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Boss|Cutscene")
    TArray<TObjectPtr<AActor>> CutsceneProxyActors;

    /** 명시적 프록시 참조(배열에 없어도 동작) */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Boss|Cutscene|ProxyRefs")
    TObjectPtr<AActor> HeadProxyActor = nullptr;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Boss|Cutscene|ProxyRefs")
    TObjectPtr<AActor> RightHandProxyActor = nullptr;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Boss|Cutscene|ProxyRefs")
    TObjectPtr<AActor> LeftHandProxyActor = nullptr;

    /** 프록시 고정 원위치(요구사항 반영) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Cutscene|ProxyTransforms")
    FVector HeadProxyLocation = FVector(0.0f, 0.0f, 9085.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Cutscene|ProxyTransforms")
    FRotator HeadProxyRotation = FRotator(15.0f, -5.0f, 0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Cutscene|ProxyTransforms")
    FVector HandProxyLocation = FVector(0.0f, 0.0f, 10000.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Cutscene|ProxyTransforms")
    FRotator HandProxyRotation = FRotator(0.0f, 0.0f, 90.0f);

    /** 프록시가 보일 때 충돌 활성화 여부(기본 false 권장) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Cutscene")
    bool bEnableProxyCollisionWhenVisible = false;

    /** 이전 요구사항 호환: 보스 스폰 기준 위치/회전 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Spawn")
    bool bUseFixedBossSpawnTransform = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Spawn", meta=(EditCondition="bUseFixedBossSpawnTransform"))
    FVector FixedBossSpawnLocation = FVector(0.0f, 0.0f, 1035.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Spawn", meta=(EditCondition="bUseFixedBossSpawnTransform"))
    FRotator FixedBossSpawnRotation = FRotator(0.0f, 180.0f, 0.0f);

    /** 고정 스폰 미사용 시 사용 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Spawn", meta=(EditCondition="!bUseFixedBossSpawnTransform"))
    FVector BossSpawnLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Spawn", meta=(EditCondition="!bUseFixedBossSpawnTransform"))
    FRotator BossSpawnRotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Encounter", meta=(ClampMin="1"))
    int32 HeadHitsToClear = 3;

    /** 아레나 진입 후 컷신 시작까지 대기 시간(요구사항: 2초) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Encounter", meta=(ClampMin="0.0"))
    float EncounterCutsceneStartDelaySeconds = 2.0f;

    // ===== Audio =====
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Audio")
    TObjectPtr<USoundBase> BossBattleBGM = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Audio", meta=(ClampMin="0.0"))
    float BossBGMVolume = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Audio", meta=(ClampMin="0.0"))
    float BossBGMFadeInSeconds = 0.10f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Boss|Audio", meta=(ClampMin="0.0"))
    float BossBGMFadeOutSeconds = 0.25f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Boss|Audio", meta=(AllowPrivateAccess="true"))
    TObjectPtr<UAudioComponent> BossBgmComp = nullptr;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Boss|Audio", meta=(AllowPrivateAccess="true"))
    bool bBossBGMStarted = false;

private:
    TWeakObjectPtr<AMarioCharacter> CachedMario;
    TWeakObjectPtr<AAttrenashinBoss> BossActor;

    bool bHasEncounterStarted = false;
    bool bWaitingForEncounterDelay = false;
    bool bIsCutscenePlaying = false;
    bool bWaitingForBossSpawn = false;
    bool bBossDefeated = false;

    int32 LastHeadHitCount = 0;

    /** 아레나 진입 -> 컷신 시작 지연 타이머 */
    FTimerHandle EncounterDelayTimerHandle;

    /** 프록시 원래 상태 캐시(액터 포인터 기준) */
    TArray<TWeakObjectPtr<AActor>> CutsceneProxyCachedActors;
    TArray<FTransform> CutsceneProxyInitialTransforms;
    TArray<bool> CutsceneProxyInitialHiddenStates;
    TArray<bool> CutsceneProxyInitialCollisionStates;
    TArray<bool> CutsceneProxyInitialTickStates;

private:
    UFUNCTION()
    void OnEncounterTriggerBeginOverlap(
        UPrimitiveComponent* OverlappedComponent,
        AActor* OtherActor,
        UPrimitiveComponent* OtherComp,
        int32 OtherBodyIndex,
        bool bFromSweep,
        const FHitResult& SweepResult);

    UFUNCTION()
    void OnEncounterCutsceneFinished();

    UFUNCTION()
    void OnEncounterStartDelayElapsed();

    void BeginEncounterCutscene();
    void StartEncounterCutsceneWithDelay();
    void CancelEncounterDelay();
    void SpawnBossNow();

    void UpdateBossProgress();
    void HandleBossDefeated();
    void HandlePlayerEliminated();

    void CleanupLiveBossActors();

    void CacheCutsceneProxyInitialTransforms();
    void RestoreCutsceneProxyTransforms();
    void SetCutsceneProxyVisible(bool bVisible);
    int32 FindCachedProxyIndex(const AActor* Proxy) const;
    void CollectAllProxyActors(TArray<AActor*>& OutProxies) const;
    bool WarpProxyToAuthoredTransform(AActor* Proxy) const;

    void TransitionToPlayerCamera(float BlendSeconds);
};
