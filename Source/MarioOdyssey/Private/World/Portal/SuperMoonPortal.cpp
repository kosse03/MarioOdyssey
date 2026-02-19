#include "World/Portal/SuperMoonPortal.h"

#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "TimerManager.h"

#include "MarioOdyssey/MarioCharacter.h"
#include "Progress/MarioGameInstance.h"

ASuperMoonPortal::ASuperMoonPortal()
{
	PrimaryActorTick.bCanEverTick = false;

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	SetRootComponent(Trigger);

	// 기본 사이즈(에디터에서 조정 가능)
	Trigger->InitBoxExtent(FVector(120.f, 120.f, 140.f));

	Trigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Trigger->SetCollisionObjectType(ECC_WorldDynamic);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
}

void ASuperMoonPortal::BeginPlay()
{
	Super::BeginPlay();

	if (UMarioGameInstance* GI = GetGI())
	{
		// 슈퍼문 변화에 따라 활성화/비활성화 갱신
		GI->OnSuperMoonCountChanged.AddDynamic(this, &ASuperMoonPortal::HandleMoonCountChanged);
	}

	UpdateEnabledState();

	if (Trigger)
	{
		Trigger->OnComponentBeginOverlap.AddDynamic(this, &ASuperMoonPortal::OnTriggerBeginOverlap);
	}
}

UMarioGameInstance* ASuperMoonPortal::GetGI() const
{
	return GetGameInstance<UMarioGameInstance>();
}

void ASuperMoonPortal::HandleMoonCountChanged(int32 NewTotalMoons)
{
	UpdateEnabledState();
}

void ASuperMoonPortal::UpdateEnabledState()
{
	const bool bWasEnabled = bPortalEnabled;

	bPortalEnabled = false;
	if (UMarioGameInstance* GI = GetGI())
	{
		bPortalEnabled = GI->HasAtLeastSuperMoons(RequiredSuperMoons);
	}

	if (!Trigger) return;

	if (bPortalEnabled)
	{
		Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
	else
	{
		Trigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// travel 중에는 강제로 OFF (중복 이동 방지)
	if (bTravelInProgress)
	{
		Trigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// 상태 변화가 있을 때 추가 로직(사운드/이펙트)은 BP로 확장 가능
	(void)bWasEnabled;
}

void ASuperMoonPortal::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                            UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                                            bool bFromSweep, const FHitResult& SweepResult)
{
	if (bTravelInProgress) return;
	if (!bPortalEnabled) return;

	AMarioCharacter* Mario = Cast<AMarioCharacter>(OtherActor);
	if (!Mario) return;

	StartTravel(Mario);
}

void ASuperMoonPortal::StartBlackFade(float FromAlpha, float ToAlpha, float DurationSeconds, bool bHoldWhenFinished) const
{
	UWorld* World = GetWorld();
	if (!World) return;

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (!PC || !PC->PlayerCameraManager) return;

	PC->PlayerCameraManager->StartCameraFade(
		FromAlpha,
		ToAlpha,
		FMath::Max(0.f, DurationSeconds),
		FLinearColor::Black,
		false,
		bHoldWhenFinished
	);
}

void ASuperMoonPortal::StartTravel(AMarioCharacter* Mario)
{
	if (bTravelInProgress) return;
	if (!Mario) return;
	if (TargetLevelName.IsNone()) return;

	UMarioGameInstance* GI = GetGI();
	if (!GI) return;

	bTravelInProgress = true;
	UpdateEnabledState();

	// 이동 후 스폰 좌표/페이드아웃 요청 저장
	const FTransform SpawnTM(TargetSpawnRotation, TargetSpawnLocation, FVector(1.f));
	GI->SetPendingSpawnTransform(SpawnTM);
	GI->SetPendingFadeOut(FadeOutSeconds, bLockInputUntilFadeOut);

	// 현재 맵에서 입력 잠금(페이드 중 이동 방지)
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
		{
			PC->SetIgnoreMoveInput(true);
			PC->SetIgnoreLookInput(true);
		}
	}

	// 페이드 인(검정) -> 유지 -> OpenLevel
	StartBlackFade(0.f, 1.f, FadeInSeconds, true);

	if (UWorld* World = GetWorld())
	{
		const float Wait = FMath::Max(0.f, FadeInSeconds) + FMath::Max(0.f, BlackHoldSeconds);
		World->GetTimerManager().ClearTimer(TravelTimer);

		if (Wait <= KINDA_SMALL_NUMBER)
		{
			OpenTargetLevel();
		}
		else
		{
			World->GetTimerManager().SetTimer(TravelTimer, this, &ASuperMoonPortal::OpenTargetLevel, Wait, false);
		}
	}
}

void ASuperMoonPortal::OpenTargetLevel()
{
	UWorld* World = GetWorld();
	if (!World) return;

	UGameplayStatics::OpenLevel(World, TargetLevelName);
}
