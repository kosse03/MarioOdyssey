#include "World/Start/MarioStartFlowActor.h"

#include "UI/MarioStartScreenWidget.h"
#include "UI/MarioHUDWidget.h"
#include "Audio/BgmManager.h"

#include "Kismet/GameplayStatics.h"
#include "Blueprint/WidgetBlueprintLibrary.h"

#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"

#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieSceneSequencePlayer.h"

#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AMarioStartFlowActor::AMarioStartFlowActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// 기본 위젯 경로(없으면 null 유지)
	static ConstructorHelpers::FClassFinder<UMarioStartScreenWidget> StartBP(TEXT("/Game/_BP/UI/WBP_StartScreen"));
	if (StartBP.Succeeded())
	{
		StartWidgetClass = StartBP.Class;
	}
}

void AMarioStartFlowActor::BeginPlay()
{
	Super::BeginPlay();

	// 레벨이 리로드되지 않는 리스폰 구조에서는, 액터가 살아있는 동안 1회만 실행하면 요구사항 충족
	if (bCompleted || bStarted)
	{
		return;
	}

	// 한 틱 뒤 시작(PC/HUD 생성 순서 안정화)
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().SetTimerForNextTick(this, &AMarioStartFlowActor::InitStartScreen);
	}
	else
	{
		InitStartScreen();
	}
}

void AMarioStartFlowActor::InitStartScreen()
{
	if (bCompleted || bStarted) return;
	bStarted = true;

	// HUD 숨김 + 입력 잠금
	ShowHudWidgets(false);
	LockGameplay(true);

	// 시작화면/컷신 동안 월드 BGM(BgmManager) 억제
	SetWorldBgmSuppressed(true);

	// Start UI
	if (StartWidgetClass)
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			StartWidget = CreateWidget<UMarioStartScreenWidget>(PC, StartWidgetClass);
			if (StartWidget)
			{
				StartWidget->AddToViewport(9999);

				// UIOnly로 키 입력을 위젯이 받게 한다.
				FInputModeUIOnly Mode;
				Mode.SetWidgetToFocus(StartWidget->TakeWidget());
				Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
				PC->SetInputMode(Mode);
				PC->bShowMouseCursor = false;

				StartWidget->SetKeyboardFocus();
				BindWidget();
			}
		}
	}

	PlayStartAudio();
}

void AMarioStartFlowActor::BindWidget()
{
	if (!StartWidget) return;
	StartWidget->OnStartRequested.AddDynamic(this, &AMarioStartFlowActor::HandleStartRequested);
}

void AMarioStartFlowActor::PlayStartAudio()
{
	if (StartBgm)
	{
		BgmComp = UGameplayStatics::SpawnSound2D(this, StartBgm, StartBgmVolume);
	}
	if (StartVoice)
	{
		VoiceComp = UGameplayStatics::SpawnSound2D(this, StartVoice, StartVoiceVolume);
	}
}

void AMarioStartFlowActor::FadeOutStartAudio()
{
	if (BgmComp)
	{
		BgmComp->FadeOut(StartAudioFadeOutSeconds, 0.0f);
	}
	if (VoiceComp)
	{
		VoiceComp->FadeOut(StartAudioFadeOutSeconds, 0.0f);
	}
}

void AMarioStartFlowActor::HandleStartRequested()
{
	if (bCompleted) return;

	// 입력 SFX
	if (StartKeySfx)
	{
		UGameplayStatics::PlaySound2D(this, StartKeySfx, StartKeySfxVolume);
	}

	// Start 오디오 정리
	FadeOutStartAudio();

	// UI 제거
	if (StartWidget)
	{
		StartWidget->RemoveFromParent();
		StartWidget = nullptr;
	}

	// 화면 페이드 아웃(블랙) -> 완료 후 컷신 시작
	StartScreenFade(0.0f, 1.0f, FadeOutSeconds, true);

	const float TotalDelay = FMath::Max(0.0f, FadeOutSeconds) + FMath::Max(0.0f, StartCutsceneDelay);

	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(StartCutsceneTimer);
		W->GetTimerManager().SetTimer(StartCutsceneTimer, this, &AMarioStartFlowActor::StartCutsceneOrFinish, TotalDelay, false);
	}
	else
	{
		StartCutsceneOrFinish();
	}
}

void AMarioStartFlowActor::StartCutsceneOrFinish()
{
	ALevelSequenceActor* SeqActor = ResolveIntroSequenceActor();
	if (!SeqActor)
	{
		HandleIntroFinished();
		return;
	}

	ULevelSequencePlayer* Player = SeqActor->GetSequencePlayer();
	if (!Player)
	{
		HandleIntroFinished();
		return;
	}

	// 컷신 시작: 시네마틱 모드(입력 잠금/플레이어 숨김)
	if (bUseCinematicModeDuringCutscene)
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			PC->SetCinematicMode(true, true, true, true, true);
		}
	}

	Player->OnFinished.RemoveAll(this);
	Player->OnFinished.AddDynamic(this, &AMarioStartFlowActor::HandleIntroFinished);
	Player->Play();

	// 컷신이 "블랙 상태"에서 시작되었으니, 여기서 페이드 인으로 컷신을 보여준다.
	StartScreenFade(1.0f, 0.0f, FadeInSeconds, false);
}

ALevelSequenceActor* AMarioStartFlowActor::ResolveIntroSequenceActor() const
{
	if (IntroSequenceActor)
	{
		return IntroSequenceActor;
	}

	if (!GetWorld()) return nullptr;

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ALevelSequenceActor::StaticClass(), Found);

	for (AActor* A : Found)
	{
		if (ALevelSequenceActor* L = Cast<ALevelSequenceActor>(A))
		{
			if (L->ActorHasTag(IntroSequenceTag))
			{
				return L;
			}
		}
	}

	// fallback: 첫 번째
	for (AActor* A : Found)
	{
		if (ALevelSequenceActor* L = Cast<ALevelSequenceActor>(A))
		{
			return L;
		}
	}

	return nullptr;
}

void AMarioStartFlowActor::HandleIntroFinished()
{
	if (bCompleted) return;
	bCompleted = true;

	// 컷신 종료: 먼저 페이드 아웃(블랙) -> 그 다음 카메라/입력/모드 복구 -> 페이드 인
	StartScreenFade(0.0f, 1.0f, FadeOutSeconds, true);

	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(FinishToGameplayTimer);
		W->GetTimerManager().SetTimer(FinishToGameplayTimer, this, &AMarioStartFlowActor::FinishAfterFadeOut, FMath::Max(0.0f, FadeOutSeconds), false);
	}
	else
	{
		FinishAfterFadeOut();
	}
}


void AMarioStartFlowActor::LockGameplay(bool bLock)
{
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->SetIgnoreMoveInput(bLock);
		PC->SetIgnoreLookInput(bLock);
	}
}

void AMarioStartFlowActor::ShowHudWidgets(bool bShow)
{
	if (!GetWorld()) return;

	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), Widgets, UMarioHUDWidget::StaticClass(), false);

	for (UUserWidget* W : Widgets)
	{
		if (!W) continue;
		W->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}
}


void AMarioStartFlowActor::FinishAfterFadeOut()
{
	// 월드 BGM 재개(컷신이 끝난 뒤부터)
	SetWorldBgmSuppressed(false);

	// 입력 모드 복구 + 잠금 해제 + 카메라 복귀
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (bUseCinematicModeDuringCutscene)
		{
			// CinematicMode를 켰던 경우, 여기서 확실히 해제(이동/회전 다시 활성화)
			PC->SetCinematicMode(false, false, false, false, false);
		}

		// 뷰 타겟을 플레이어로 강제 복귀(컷신 카메라가 남아있는 케이스 방지)
		if (APawn* P = PC->GetPawn())
		{
			PC->SetViewTargetWithBlend(P, 0.0f);
		}

		FInputModeGameOnly Mode;
		PC->SetInputMode(Mode);
		PC->bShowMouseCursor = false;

		// 혹시 남아있을 수 있는 입력 무시 플래그를 확실히 해제
		PC->SetIgnoreMoveInput(false);
		PC->SetIgnoreLookInput(false);
	}

	ShowHudWidgets(true);
	LockGameplay(false);

	// 마지막으로 페이드 인(게임 화면 노출)
	StartScreenFade(1.0f, 0.0f, FadeInSeconds, false);
}
ABgmManager* AMarioStartFlowActor::ResolveBgmManager()
{
	if (CachedBgmManager.IsValid())
	{
		return CachedBgmManager.Get();
	}

	if (!GetWorld()) return nullptr;

	ABgmManager* M = Cast<ABgmManager>(UGameplayStatics::GetActorOfClass(GetWorld(), ABgmManager::StaticClass()));
	CachedBgmManager = M;
	return M;
}

void AMarioStartFlowActor::SetWorldBgmSuppressed(bool bSuppress)
{
	if (ABgmManager* M = ResolveBgmManager())
	{
		M->SetSuppressed(bSuppress, 0.25f);
	}
}

void AMarioStartFlowActor::StartScreenFade(float FromAlpha, float ToAlpha, float Duration, bool bHoldWhenFinished)
{
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(FromAlpha, ToAlpha, Duration, FLinearColor::Black, false, bHoldWhenFinished);
		}
	}
}
