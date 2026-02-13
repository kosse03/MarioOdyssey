#include "World/BossArenaController.h"

#include "Components/AudioComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

#include "MarioOdyssey/MarioCharacter.h"
#include "Character/Boss/AttrenashinBoss.h"
#include "Character/Boss/AttrenashinFist.h"
#include "Character/Boss/IceShardActor.h"
#include "Character/Boss/IceTileActor.h"

#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieSceneSequencePlayer.h"

ABossArenaController::ABossArenaController()
{
    PrimaryActorTick.bCanEverTick = true;

    EncounterTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("EncounterTrigger"));
    RootComponent = EncounterTrigger;

    EncounterTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    EncounterTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
    EncounterTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    EncounterTrigger->SetGenerateOverlapEvents(true);
}

void ABossArenaController::BeginPlay()
{
    Super::BeginPlay();

    if (EncounterTrigger)
    {
        EncounterTrigger->OnComponentBeginOverlap.AddDynamic(this, &ABossArenaController::OnEncounterTriggerBeginOverlap);
    }

    CachedMario = Cast<AMarioCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));

    CacheCutsceneProxyInitialTransforms();
    // 기본 대기 상태: 프록시 표시
    RestoreCutsceneProxyTransforms();
    SetCutsceneProxyVisible(true);
}

void ABossArenaController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    const bool bMarioGameOver = CachedMario.IsValid() && CachedMario->IsGameOverPublic();

    // 사망 시점에 컷신/지연/보스 상태를 즉시 정리해서
    // 프록시 원복/표시/재도전 루프가 확실히 동작하도록 처리
    if (bMarioGameOver)
    {
        if (BossActor.IsValid() || bHasEncounterStarted || bIsCutscenePlaying || bWaitingForBossSpawn || bWaitingForEncounterDelay)
        {
            HandlePlayerEliminated();
        }
        return;
    }

    // 보스가 살아있는 동안 진행도 갱신
    if (BossActor.IsValid())
    {
        UpdateBossProgress();
        return;
    }

    // 컷신/스폰/지연 대기 중이면 대기
    if (bIsCutscenePlaying || bWaitingForBossSpawn || bWaitingForEncounterDelay)
    {
        return;
    }

    // 조우 시작 후 보스 미존재면 컷신 시작
    if (bHasEncounterStarted && !bBossDefeated)
    {
        BeginEncounterCutscene();
    }
}

void ABossArenaController::OnEncounterTriggerBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (!OtherActor || OtherActor != CachedMario.Get())
    {
        return;
    }

    if (BossActor.IsValid() || bIsCutscenePlaying || bWaitingForBossSpawn || bWaitingForEncounterDelay)
    {
        return;
    }

    bHasEncounterStarted = true;
    bBossDefeated = false;

    // 요구사항: 아레나 진입 후 2초 대기 -> 컷신 시작
    StartEncounterCutsceneWithDelay();
}

void ABossArenaController::StartEncounterCutsceneWithDelay()
{
    if (!bHasEncounterStarted || bBossDefeated || bIsCutscenePlaying || bWaitingForBossSpawn)
    {
        return;
    }

    CancelEncounterDelay();

    if (EncounterCutsceneStartDelaySeconds <= KINDA_SMALL_NUMBER)
    {
        OnEncounterStartDelayElapsed();
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        // 월드가 없으면 즉시 진행
        OnEncounterStartDelayElapsed();
        return;
    }

    bWaitingForEncounterDelay = true;
    World->GetTimerManager().SetTimer(
        EncounterDelayTimerHandle,
        this,
        &ABossArenaController::OnEncounterStartDelayElapsed,
        EncounterCutsceneStartDelaySeconds,
        false);
}

void ABossArenaController::CancelEncounterDelay()
{
    bWaitingForEncounterDelay = false;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(EncounterDelayTimerHandle);
    }
}

void ABossArenaController::OnEncounterStartDelayElapsed()
{
    CancelEncounterDelay();

    if (!bHasEncounterStarted || bBossDefeated)
    {
        return;
    }

    if (CachedMario.IsValid() && CachedMario->IsGameOverPublic())
    {
        return;
    }

    BeginEncounterCutscene();
}

void ABossArenaController::BeginEncounterCutscene()
{
    if (!bHasEncounterStarted || bIsCutscenePlaying || bWaitingForBossSpawn || bWaitingForEncounterDelay)
    {
        return;
    }

    if (CachedMario.IsValid() && CachedMario->IsGameOverPublic())
    {
        return;
    }

    bIsCutscenePlaying = true;

    // 컷신 직전: 기존 보스/주먹/샤드/타일 잔존물 제거
    CleanupLiveBossActors();

    // 컷신 프록시를 원위치/가시화
    RestoreCutsceneProxyTransforms();
    SetCutsceneProxyVisible(true);

    // 컷신 시작 전 카메라 블렌드
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        if (EncounterCutsceneActor)
        {
            PC->SetViewTargetWithBlend(EncounterCutsceneActor, CutsceneCameraBlendInSeconds);
        }
    }

    if (bAutoPlayEncounterCutscene && EncounterCutsceneActor && EncounterCutsceneActor->GetSequencePlayer())
    {
        ULevelSequencePlayer* SeqPlayer = EncounterCutsceneActor->GetSequencePlayer();
        SeqPlayer->OnFinished.RemoveDynamic(this, &ABossArenaController::OnEncounterCutsceneFinished);
        SeqPlayer->OnFinished.AddDynamic(this, &ABossArenaController::OnEncounterCutsceneFinished);

        SeqPlayer->Stop();
        SeqPlayer->Play();
    }
    else
    {
        // 컷신 미사용일 때 바로 다음 단계
        OnEncounterCutsceneFinished();
    }
}

void ABossArenaController::OnEncounterCutsceneFinished()
{
    if (!bIsCutscenePlaying)
    {
        return;
    }

    bIsCutscenePlaying = false;

    // 요구사항: 컷신 종료 시 프록시를 원위치로 워프한 뒤 숨김
    RestoreCutsceneProxyTransforms();
    SetCutsceneProxyVisible(false);

    // 플레이어 카메라로 부드럽게 복귀
    TransitionToPlayerCamera(CutsceneToPlayerCameraBlendSeconds);

    // 요구사항: 컷신 종료 즉시 보스 스폰
    SpawnBossNow();
}

void ABossArenaController::SpawnBossNow()
{
    if (bWaitingForBossSpawn || BossActor.IsValid() || !BossClass)
    {
        return;
    }

    bWaitingForBossSpawn = true;

    const FVector SpawnLoc = bUseFixedBossSpawnTransform ? FixedBossSpawnLocation : BossSpawnLocation;
    const FRotator SpawnRot = bUseFixedBossSpawnTransform ? FixedBossSpawnRotation : BossSpawnRotation;

    FActorSpawnParameters SP;
    SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SP.Owner = this;

    AAttrenashinBoss* SpawnedBoss = GetWorld()->SpawnActor<AAttrenashinBoss>(BossClass, SpawnLoc, SpawnRot, SP);
    if (SpawnedBoss)
    {
        BossActor = SpawnedBoss;
        LastHeadHitCount = SpawnedBoss->GetHeadHitCount();
        bBossDefeated = false;
    }

    bWaitingForBossSpawn = false;
}

void ABossArenaController::UpdateBossProgress()
{
    if (!BossActor.IsValid())
    {
        return;
    }

    const int32 CurrentHeadHits = BossActor->GetHeadHitCount();
    if (CurrentHeadHits != LastHeadHitCount)
    {
        LastHeadHitCount = CurrentHeadHits;
    }

    if (CurrentHeadHits >= HeadHitsToClear)
    {
        HandleBossDefeated();
    }
}

void ABossArenaController::HandleBossDefeated()
{
    if (bBossDefeated)
    {
        return;
    }

    bBossDefeated = true;
    bHasEncounterStarted = false;
    bIsCutscenePlaying = false;
    bWaitingForBossSpawn = false;

    CancelEncounterDelay();

    StopBossBGMNative();

    if (EncounterCutsceneActor && EncounterCutsceneActor->GetSequencePlayer())
    {
        EncounterCutsceneActor->GetSequencePlayer()->OnFinished.RemoveDynamic(this, &ABossArenaController::OnEncounterCutsceneFinished);
    }

    CleanupLiveBossActors();

    // 클리어 후 프록시는 기본적으로 숨김 유지
    SetCutsceneProxyVisible(false);
}

void ABossArenaController::HandlePlayerEliminated()
{
    // 사망 시: BGM 중지, 실보스/잔존물 정리, 프록시 원복 후 표시
    StopBossBGMNative();

    CancelEncounterDelay();

    bIsCutscenePlaying = false;
    bWaitingForBossSpawn = false;

    if (EncounterCutsceneActor && EncounterCutsceneActor->GetSequencePlayer())
    {
        ULevelSequencePlayer* SeqPlayer = EncounterCutsceneActor->GetSequencePlayer();
        SeqPlayer->OnFinished.RemoveDynamic(this, &ABossArenaController::OnEncounterCutsceneFinished);
        SeqPlayer->Stop();
    }

    CleanupLiveBossActors();

    bHasEncounterStarted = false;
    bBossDefeated = false;
    LastHeadHitCount = 0;

    RestoreCutsceneProxyTransforms();
    SetCutsceneProxyVisible(true);

    // 플레이어 쪽 카메라로 복귀(0초로 즉시)
    TransitionToPlayerCamera(0.0f);
}

void ABossArenaController::CleanupLiveBossActors()
{
    // 보스 본체 제거
    if (BossActor.IsValid())
    {
        BossActor->Destroy();
        BossActor.Reset();
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    // 잔존 주먹 제거(플레이어 사망 후 주먹이 남는 문제 해결)
    for (TActorIterator<AAttrenashinFist> It(World); It; ++It)
    {
        AAttrenashinFist* Fist = *It;
        if (IsValid(Fist))
        {
            Fist->Destroy();
        }
    }

    // 전투 잔존 샤드/타일 정리(재도전 시 상태 리셋)
    for (TActorIterator<AIceShardActor> It(World); It; ++It)
    {
        AIceShardActor* Shard = *It;
        if (IsValid(Shard))
        {
            Shard->Destroy();
        }
    }

    for (TActorIterator<AIceTileActor> It(World); It; ++It)
    {
        AIceTileActor* Tile = *It;
        if (IsValid(Tile))
        {
            Tile->Destroy();
        }
    }
}

int32 ABossArenaController::FindCachedProxyIndex(const AActor* Proxy) const
{
    if (!IsValid(Proxy))
    {
        return INDEX_NONE;
    }

    for (int32 i = 0; i < CutsceneProxyCachedActors.Num(); ++i)
    {
        if (CutsceneProxyCachedActors[i].Get() == Proxy)
        {
            return i;
        }
    }

    return INDEX_NONE;
}

void ABossArenaController::CollectAllProxyActors(TArray<AActor*>& OutProxies) const
{
    OutProxies.Reset();

    auto AddProxy = [&OutProxies](AActor* Proxy)
    {
        if (IsValid(Proxy))
        {
            OutProxies.AddUnique(Proxy);
        }
    };

    AddProxy(HeadProxyActor.Get());
    AddProxy(RightHandProxyActor.Get());
    AddProxy(LeftHandProxyActor.Get());

    for (AActor* Proxy : CutsceneProxyActors)
    {
        AddProxy(Proxy);
    }
}

bool ABossArenaController::WarpProxyToAuthoredTransform(AActor* Proxy) const
{
    if (!IsValid(Proxy))
    {
        return false;
    }

    bool bHasAuthoredTransform = false;
    FVector TargetLocation = FVector::ZeroVector;
    FRotator TargetRotation = FRotator::ZeroRotator;

    if (Proxy == HeadProxyActor.Get())
    {
        bHasAuthoredTransform = true;
        TargetLocation = HeadProxyLocation;
        TargetRotation = HeadProxyRotation;
    }
    else if (Proxy == RightHandProxyActor.Get() || Proxy == LeftHandProxyActor.Get())
    {
        bHasAuthoredTransform = true;
        TargetLocation = HandProxyLocation;
        TargetRotation = HandProxyRotation;
    }

    if (!bHasAuthoredTransform)
    {
        return false;
    }

    // 스케일은 유지하고 위치/회전만 강제 적용
    FTransform NewTransform = Proxy->GetActorTransform();
    NewTransform.SetLocation(TargetLocation);
    NewTransform.SetRotation(TargetRotation.Quaternion());

    Proxy->SetActorTransform(NewTransform, false, nullptr, ETeleportType::TeleportPhysics);
    return true;
}

void ABossArenaController::CacheCutsceneProxyInitialTransforms()
{
    CutsceneProxyCachedActors.Reset();
    CutsceneProxyInitialTransforms.Reset();
    CutsceneProxyInitialHiddenStates.Reset();
    CutsceneProxyInitialCollisionStates.Reset();
    CutsceneProxyInitialTickStates.Reset();

    TArray<AActor*> AllProxies;
    CollectAllProxyActors(AllProxies);

    CutsceneProxyCachedActors.Reserve(AllProxies.Num());
    CutsceneProxyInitialTransforms.Reserve(AllProxies.Num());
    CutsceneProxyInitialHiddenStates.Reserve(AllProxies.Num());
    CutsceneProxyInitialCollisionStates.Reserve(AllProxies.Num());
    CutsceneProxyInitialTickStates.Reserve(AllProxies.Num());

    for (AActor* Proxy : AllProxies)
    {
        if (!IsValid(Proxy))
        {
            continue;
        }

        CutsceneProxyCachedActors.Add(Proxy);
        CutsceneProxyInitialTransforms.Add(Proxy->GetActorTransform());
        CutsceneProxyInitialHiddenStates.Add(Proxy->IsHidden());
        CutsceneProxyInitialCollisionStates.Add(Proxy->GetActorEnableCollision());
        CutsceneProxyInitialTickStates.Add(Proxy->IsActorTickEnabled());
    }
}

void ABossArenaController::RestoreCutsceneProxyTransforms()
{
    TArray<AActor*> AllProxies;
    CollectAllProxyActors(AllProxies);

    for (AActor* Proxy : AllProxies)
    {
        if (!IsValid(Proxy))
        {
            continue;
        }

        int32 CachedIndex = FindCachedProxyIndex(Proxy);
        if (CachedIndex == INDEX_NONE)
        {
            // BeginPlay 이후 배열이 바뀌었거나 늦게 할당된 프록시를 안전하게 캐시에 편입
            CutsceneProxyCachedActors.Add(Proxy);
            CutsceneProxyInitialTransforms.Add(Proxy->GetActorTransform());
            CutsceneProxyInitialHiddenStates.Add(Proxy->IsHidden());
            CutsceneProxyInitialCollisionStates.Add(Proxy->GetActorEnableCollision());
            CutsceneProxyInitialTickStates.Add(Proxy->IsActorTickEnabled());
            CachedIndex = CutsceneProxyCachedActors.Num() - 1;
        }

        // 1) 머리/손 프록시는 사용자 지정 고정 좌표/회전으로 항상 워프
        const bool bWarpedToAuthored = WarpProxyToAuthoredTransform(Proxy);

        // 2) 명시되지 않은 추가 프록시는 BeginPlay 당시 초기 트랜스폼으로 복원
        if (!bWarpedToAuthored && CutsceneProxyInitialTransforms.IsValidIndex(CachedIndex))
        {
            const FTransform& T = CutsceneProxyInitialTransforms[CachedIndex];
            Proxy->SetActorTransform(T, false, nullptr, ETeleportType::TeleportPhysics);
        }

        // 상태는 SetCutsceneProxyVisible()에서 최종 결정되므로 여기선 복원만 수행
        if (CutsceneProxyInitialCollisionStates.IsValidIndex(CachedIndex) && !bEnableProxyCollisionWhenVisible)
        {
            Proxy->SetActorEnableCollision(CutsceneProxyInitialCollisionStates[CachedIndex]);
        }

        if (CutsceneProxyInitialTickStates.IsValidIndex(CachedIndex))
        {
            Proxy->SetActorTickEnabled(CutsceneProxyInitialTickStates[CachedIndex]);
        }
    }
}

void ABossArenaController::SetCutsceneProxyVisible(bool bVisible)
{
    TArray<AActor*> AllProxies;
    CollectAllProxyActors(AllProxies);

    for (AActor* Proxy : AllProxies)
    {
        if (!IsValid(Proxy))
        {
            continue;
        }

        Proxy->SetActorHiddenInGame(!bVisible);
        Proxy->SetActorEnableCollision(bVisible && bEnableProxyCollisionWhenVisible);
        Proxy->SetActorTickEnabled(bVisible);
    }
}

void ABossArenaController::TransitionToPlayerCamera(float BlendSeconds)
{
    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC || !CachedMario.IsValid())
    {
        return;
    }

    PC->SetViewTargetWithBlend(CachedMario.Get(), FMath::Max(0.0f, BlendSeconds));
}

void ABossArenaController::StartBossBGMNative()
{
    if (bBossBGMStarted || !BossBattleBGM)
    {
        return;
    }

    // 기존 컴포넌트가 없으면 생성
    if (!BossBgmComp)
    {
        UAudioComponent* NewComp = UGameplayStatics::SpawnSound2D(
            this,
            BossBattleBGM,
            BossBGMVolume,
            1.0f,
            0.0f,
            nullptr,
            true,   // persist across level transition
            false   // auto destroy
        );

        BossBgmComp = NewComp;
    }

    if (BossBgmComp)
    {
        BossBgmComp->SetVolumeMultiplier(BossBGMVolume);
        BossBgmComp->FadeIn(BossBGMFadeInSeconds, BossBGMVolume);
        bBossBGMStarted = true;
    }
}

void ABossArenaController::StopBossBGMNative()
{
    if (!BossBgmComp)
    {
        bBossBGMStarted = false;
        return;
    }

    if (BossBGMFadeOutSeconds <= KINDA_SMALL_NUMBER)
    {
        BossBgmComp->Stop();
    }
    else
    {
        BossBgmComp->FadeOut(BossBGMFadeOutSeconds, 0.0f);
    }

    bBossBGMStarted = false;
}
