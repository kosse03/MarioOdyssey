#include "Audio/BgmManager.h"

#include "Audio/BgmZoneTrigger.h"

#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"

ABgmManager::ABgmManager()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	AudioA = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioA"));
	AudioA->SetupAttachment(Root);
	AudioA->bAutoActivate = false;
	AudioA->bAllowSpatialization = false;
	AudioA->bIsUISound = true;

	AudioB = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioB"));
	AudioB->SetupAttachment(Root);
	AudioB->bAutoActivate = false;
	AudioB->bAllowSpatialization = false;
	AudioB->bIsUISound = true;
}

void ABgmManager::BeginPlay()
{
	Super::BeginPlay();

	CacheZonesOnce();
	StartPollingIfEnabled();

	if (!bSuppressed && bAutoPlayDefaultOnBeginPlay && DefaultTrack)
	{
		// 시작은 바로 스냅(또는 DefaultFadeSeconds로 페이드 인)
		CrossFadeTo(DefaultTrack, 0.0f);
	}

	// 시작 시 한 번 즉시 판정해서 "시작 위치가 구역 내부"인 경우 바로 전환되게 함
	if (bUseLocationPolling)
	{
		PollZonesByLocation();
	}
}

void ABgmManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopPolling();
	Super::EndPlay(EndPlayReason);
}

void ABgmManager::RequestTrack(USoundBase* NewTrack, float FadeSeconds)
{
	if (bSuppressed)
	{
		// Suppressed 상태에서는 트랙을 재생하지 않는다.
		CurrentTrack = nullptr;
		return;
	}

// 수동 전환: 구역 스택을 비우고 이 트랙으로 고정
	ActiveZones.Reset();
	InsideZones.Reset();
	PruneInvalidZones();

	CrossFadeTo(NewTrack, FadeSeconds);
}

void ABgmManager::SetSuppressed(bool bInSuppressed, float FadeSeconds)
{
	if (bSuppressed == bInSuppressed)
	{
		return;
	}

	bSuppressed = bInSuppressed;

	if (bSuppressed)
	{
		// 즉시/페이드로 음을 끄고, 트랙 전환은 모두 막는다.
		if (AudioA && AudioA->IsPlaying())
		{
			if (FadeSeconds > 0.f) AudioA->FadeOut(FadeSeconds, 0.0f);
			else AudioA->Stop();
		}
		if (AudioB && AudioB->IsPlaying())
		{
			if (FadeSeconds > 0.f) AudioB->FadeOut(FadeSeconds, 0.0f);
			else AudioB->Stop();
		}
		CurrentTrack = nullptr;
		return;
	}

	// Suppress 해제: 현재 위치/구역 기준 트랙으로 재생
	UpdateDesiredTrack();
}

void ABgmManager::EnterZone(ABgmZoneTrigger* Zone)
{
	if (!IsValid(Zone)) return;
	if (!Zone->ZoneTrack) return;

	PruneInvalidZones();

	// 이미 들어와 있으면 EnterOrder만 갱신(=같은 우선순위일 때 최신 우선)
	for (FZoneEntry& E : ActiveZones)
	{
		if (E.Zone.Get() == Zone)
		{
			E.EnterOrder = ++EnterOrderCounter;
			E.Priority = Zone->Priority;
			UpdateDesiredTrack();
			return;
		}
	}

	FZoneEntry NewEntry;
	NewEntry.Zone = Zone;
	NewEntry.Priority = Zone->Priority;
	NewEntry.EnterOrder = ++EnterOrderCounter;
	ActiveZones.Add(NewEntry);

	UpdateDesiredTrack();
}

void ABgmManager::ExitZone(ABgmZoneTrigger* Zone)
{
	if (!IsValid(Zone)) return;

	PruneInvalidZones();

	ActiveZones.RemoveAll([Zone](const FZoneEntry& E)
	{
		return E.Zone.Get() == Zone;
	});

	UpdateDesiredTrack();
}

void ABgmManager::UpdateDesiredTrack()
{
	if (bSuppressed)
	{
		return;
	}

PruneInvalidZones();

	int32 BestPri = 0;
	float BestFade = DefaultFadeSeconds;
	ABgmZoneTrigger* BestZone = PickBestZone(BestPri, BestFade);

	if (BestZone && BestZone->ZoneTrack)
	{
		CrossFadeTo(BestZone->ZoneTrack, BestFade);
	}
	else
	{
		CrossFadeTo(DefaultTrack, DefaultFadeSeconds);
	}
}

ABgmZoneTrigger* ABgmManager::PickBestZone(int32& OutPriority, float& OutFadeSeconds) const
{
	ABgmZoneTrigger* Best = nullptr;
	int32 BestPri = INT32_MIN;
	int64 BestOrder = INT64_MIN;
	float BestFade = DefaultFadeSeconds;

	for (const FZoneEntry& E : ActiveZones)
	{
		ABgmZoneTrigger* Z = E.Zone.Get();
		if (!IsValid(Z)) continue;
		if (!Z->ZoneTrack) continue;

		const int32 Pri = Z->Priority;
		const int64 Order = E.EnterOrder;

		if (Pri > BestPri || (Pri == BestPri && Order > BestOrder))
		{
			Best = Z;
			BestPri = Pri;
			BestOrder = Order;
			BestFade = Z->FadeSeconds;
		}
	}

	OutPriority = (Best ? BestPri : 0);
	OutFadeSeconds = BestFade;
	return Best;
}

void ABgmManager::CrossFadeTo(USoundBase* NewTrack, float FadeSeconds)
{
	if (!NewTrack)
	{
		// 트랙이 없으면 둘 다 정지
		if (AudioA && AudioA->IsPlaying()) AudioA->FadeOut(FadeSeconds, 0.0f);
		if (AudioB && AudioB->IsPlaying()) AudioB->FadeOut(FadeSeconds, 0.0f);
		CurrentTrack = nullptr;
		return;
	}

	// 이미 같은 트랙이면 "재시작"하지 않는다
	if (CurrentTrack == NewTrack)
	{
		// 혹시 정지된 상태면 다시 재생(이 경우에만 처음부터 재생될 수 있음)
		UAudioComponent* Active = bUsingA ? AudioA : AudioB;
		if (Active && !Active->IsPlaying())
		{
			Active->SetSound(NewTrack);
			Active->Play(0.f);
		}
		return;
	}

	UAudioComponent* From = bUsingA ? AudioA : AudioB;
	UAudioComponent* To   = bUsingA ? AudioB : AudioA;

	if (!To || !From) return;

	To->SetSound(NewTrack);

	// To: 페이드 인
	To->Play(0.f);
	if (FadeSeconds > 0.f)
	{
		To->FadeIn(FadeSeconds, 1.0f, 0.0f);
	}
	else
	{
		To->SetVolumeMultiplier(1.0f);
	}

	// From: 페이드 아웃
	if (From->IsPlaying())
	{
		if (FadeSeconds > 0.f)
		{
			From->FadeOut(FadeSeconds, 0.0f);
		}
		else
		{
			From->Stop();
		}
	}

	bUsingA = !bUsingA;
	CurrentTrack = NewTrack;
}

void ABgmManager::PruneInvalidZones()
{
	ActiveZones.RemoveAll([](const FZoneEntry& E)
	{
		return !E.Zone.IsValid();
	});

	AllZones.RemoveAll([](const TWeakObjectPtr<ABgmZoneTrigger>& Z)
	{
		return !Z.IsValid();
	});

	// TSet에는 FilterByPredicate가 없어서 Iterator로 정리
	for (auto It = InsideZones.CreateIterator(); It; ++It)
	{
		const TWeakObjectPtr<ABgmZoneTrigger>& Z = *It;
		if (!Z.IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

void ABgmManager::CacheZonesOnce()
{
	AllZones.Reset();

	if (!GetWorld()) return;

	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABgmZoneTrigger::StaticClass(), Actors);

	for (AActor* A : Actors)
	{
		if (ABgmZoneTrigger* Z = Cast<ABgmZoneTrigger>(A))
		{
			AllZones.Add(Z);
		}
	}
}

void ABgmManager::StartPollingIfEnabled()
{
	if (!bUseLocationPolling) return;
	if (!GetWorld()) return;

	GetWorld()->GetTimerManager().ClearTimer(PollTimer);
	GetWorld()->GetTimerManager().SetTimer(PollTimer, this, &ABgmManager::PollZonesByLocation, PollIntervalSeconds, true);
}

void ABgmManager::StopPolling()
{
	if (!GetWorld()) return;
	GetWorld()->GetTimerManager().ClearTimer(PollTimer);
}

FVector ABgmManager::GetListenerLocation() const
{
	if (!GetWorld()) return FVector::ZeroVector;

	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC) return FVector::ZeroVector;

	// 캡쳐 정책상 ViewTarget은 Mario로 유지되므로, 위치는 ViewTarget이 가장 안정적
	if (AActor* VT = PC->GetViewTarget())
	{
		return VT->GetActorLocation();
	}
	if (APawn* P = PC->GetPawn())
	{
		return P->GetActorLocation();
	}
	return FVector::ZeroVector;
}

void ABgmManager::PollZonesByLocation()
{
	PruneInvalidZones();

	const FVector L = GetListenerLocation();

	// 새로 들어온/나간 구역 계산
	for (const TWeakObjectPtr<ABgmZoneTrigger>& WeakZ : AllZones)
	{
		ABgmZoneTrigger* Z = WeakZ.Get();
		if (!IsValid(Z)) continue;

		const bool bInside = Z->IsLocationInside(L);

		const bool bWasInside = InsideZones.Contains(WeakZ);

		if (bInside && !bWasInside)
		{
			InsideZones.Add(WeakZ);
			EnterZone(Z);
		}
		else if (!bInside && bWasInside)
		{
			InsideZones.Remove(WeakZ);
			// 이탈 시 복귀를 원치 않는 구역이면 Exit 처리 안 함
			if (Z->bRevertOnExit)
			{
				ExitZone(Z);
			}
		}
	}
}
