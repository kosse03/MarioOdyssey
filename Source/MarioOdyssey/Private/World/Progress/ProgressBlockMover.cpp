#include "World/Progress/ProgressBlockMover.h"

#include "Components/StaticMeshComponent.h"
#include "Progress/MarioGameInstance.h"

AProgressBlockMover::AProgressBlockMover()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetMobility(EComponentMobility::Movable);
}

void AProgressBlockMover::BeginPlay()
{
	Super::BeginPlay();

	StartLocation = GetActorLocation();
	EndLocation = StartLocation + TargetOffset;

	CachedGI = GetGameInstance<UMarioGameInstance>();
	if (CachedGI)
	{
		CachedGI->OnSuperMoonCountChanged.AddDynamic(this, &AProgressBlockMover::OnMoonCountChanged);
		CachedGI->OnSuperMoonCollected.AddDynamic(this, &AProgressBlockMover::OnMoonCollected);
	}

	TryTrigger();
}

void AProgressBlockMover::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (CachedGI)
	{
		CachedGI->OnSuperMoonCountChanged.RemoveDynamic(this, &AProgressBlockMover::OnMoonCountChanged);
		CachedGI->OnSuperMoonCollected.RemoveDynamic(this, &AProgressBlockMover::OnMoonCollected);
	}

	Super::EndPlay(EndPlayReason);
}

void AProgressBlockMover::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bMoving) return;

	MoveElapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(MoveElapsed / MoveDuration, 0.f, 1.f);
	const float Smooth = FMath::SmoothStep(0.f, 1.f, Alpha);
	SetActorLocation(FMath::Lerp(StartLocation, EndLocation, Smooth));

	if (Alpha >= 1.f - KINDA_SMALL_NUMBER)
	{
		bMoving = false;
		SetActorTickEnabled(false);
	}
}

bool AProgressBlockMover::IsRequirementMet() const
{
	if (!CachedGI) return false;

	bool bCountOk = true;
	if (RequiredSuperMoonCount > 0)
	{
		bCountOk = CachedGI->HasAtLeastSuperMoons(RequiredSuperMoonCount);
	}

	bool bIdOk = true;
	if (!RequiredMoonId.IsNone())
	{
		bIdOk = CachedGI->HasSuperMoon(RequiredMoonId);
	}

	return bCountOk && bIdOk;
}

void AProgressBlockMover::TryTrigger()
{
	if (bTriggerOnlyOnce && bTriggered) return;
	if (!IsRequirementMet()) return;

	bTriggered = true;
	StartMove();
}

void AProgressBlockMover::StartMove()
{
	if (bMoving) return;
	bMoving = true;
	MoveElapsed = 0.f;

	// 지금 위치가 Start가 아닐 수도 있으니 갱신
	StartLocation = GetActorLocation();
	EndLocation = StartLocation + TargetOffset;

	SetActorTickEnabled(true);
}

void AProgressBlockMover::OnMoonCountChanged(int32 NewTotal)
{
	TryTrigger();
}

void AProgressBlockMover::OnMoonCollected(FName MoonId, int32 NewTotal)
{
	TryTrigger();
}
