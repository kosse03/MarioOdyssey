#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MarioStartFlowActor.generated.h"

class UMarioStartScreenWidget;
class UAudioComponent;
class USoundBase;
class ALevelSequenceActor;
class ABgmManager;

/**
 * 게임 시작 플로우:
 * 1) 시작 화면(배경 이미지 위젯) 표시 + 시작 BGM/보이스 재생
 * 2) C 키 누르면 입력 SFX 재생 + 화면 페이드 아웃
 * 3) 맵 소개 컷신(LevelSequence) 재생(컷신 동안 월드 BGM 억제)
 * 4) 컷신 종료 후 화면 페이드 인 + 월드 BGM 재개 + 플레이 시작
 *
 * 요구사항:
 * - 시작화면에서는 월드 BGM(BgmManager)이 들리면 안 됨
 * - 컷신 도중에도 월드 BGM이 들리면 안 됨
 * - C키 입력 시 페이드 인/아웃
 * - 리스폰해도 이 플로우는 다시 실행되지 않음(레벨 리로드 전까지)
 */
UCLASS()
class MARIOODYSSEY_API AMarioStartFlowActor : public AActor
{
	GENERATED_BODY()

public:
	AMarioStartFlowActor();

protected:
	virtual void BeginPlay() override;

	// ===== Start Screen =====
	UPROPERTY(EditAnywhere, Category="Start|UI")
	TSubclassOf<UMarioStartScreenWidget> StartWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UMarioStartScreenWidget> StartWidget = nullptr;

	// ===== Audio (Start Screen Only) =====
	UPROPERTY(EditAnywhere, Category="Start|Audio")
	USoundBase* StartBgm = nullptr;

	UPROPERTY(EditAnywhere, Category="Start|Audio", meta=(ClampMin="0.0"))
	float StartBgmVolume = 1.0f;

	UPROPERTY(EditAnywhere, Category="Start|Audio")
	USoundBase* StartVoice = nullptr;

	UPROPERTY(EditAnywhere, Category="Start|Audio", meta=(ClampMin="0.0"))
	float StartVoiceVolume = 1.0f;

	UPROPERTY(EditAnywhere, Category="Start|Audio")
	USoundBase* StartKeySfx = nullptr;

	UPROPERTY(EditAnywhere, Category="Start|Audio", meta=(ClampMin="0.0"))
	float StartKeySfxVolume = 1.0f;

	UPROPERTY(EditAnywhere, Category="Start|Audio", meta=(ClampMin="0.0"))
	float StartAudioFadeOutSeconds = 0.25f;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> BgmComp = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> VoiceComp = nullptr;

	// ===== Cutscene =====
	/** 맵에 배치된 LevelSequenceActor를 지정(권장) */
	UPROPERTY(EditInstanceOnly, Category="Start|Cutscene")
	TObjectPtr<ALevelSequenceActor> IntroSequenceActor = nullptr;

	/** IntroSequenceActor를 못 찾을 때, 이 Tag로 월드에서 검색 */
	UPROPERTY(EditAnywhere, Category="Start|Cutscene")
	FName IntroSequenceTag = FName(TEXT("MapIntro"));

	/** 컷신 중에는 시네마틱 모드(입력 잠금/플레이어 숨김) */
	UPROPERTY(EditAnywhere, Category="Start|Cutscene")
	bool bUseCinematicModeDuringCutscene = true;

	/** C키 누른 뒤 컷신 플레이 전 추가 지연(원하면 0으로) */
	UPROPERTY(EditAnywhere, Category="Start|Cutscene", meta=(ClampMin="0.0"))
	float StartCutsceneDelay = 0.0f;

	// ===== Fade =====
	/** C키 누를 때 화면 페이드 아웃(블랙) */
	UPROPERTY(EditAnywhere, Category="Start|Fade", meta=(ClampMin="0.0"))
	float FadeOutSeconds = 0.35f;

	/** 컷신 끝날 때 화면 페이드 인(블랙 해제) */
	UPROPERTY(EditAnywhere, Category="Start|Fade", meta=(ClampMin="0.0"))
	float FadeInSeconds = 0.35f;

private:
	bool bStarted = false;
	bool bCompleted = false;

	TWeakObjectPtr<ABgmManager> CachedBgmManager;

	FTimerHandle StartCutsceneTimer;
	FTimerHandle FinishToGameplayTimer;

	void InitStartScreen();
	void BindWidget();
	void LockGameplay(bool bLock);

	void PlayStartAudio();
	void FadeOutStartAudio();

	void StartCutsceneOrFinish();
	ALevelSequenceActor* ResolveIntroSequenceActor() const;

	UFUNCTION()
	void HandleStartRequested();

	UFUNCTION()
	void HandleIntroFinished();

	void ShowHudWidgets(bool bShow);
	void FinishAfterFadeOut();

	// ===== World BGM (BgmManager) =====
	void SetWorldBgmSuppressed(bool bSuppress);
	ABgmManager* ResolveBgmManager();

	// ===== Screen Fade =====
	void StartScreenFade(float FromAlpha, float ToAlpha, float Duration, bool bHoldWhenFinished);
};
