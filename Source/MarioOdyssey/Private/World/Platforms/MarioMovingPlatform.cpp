#include "World/Platforms/MarioMovingPlatform.h"

#include "Components/StaticMeshComponent.h"
#include "Progress/MarioGameInstance.h"

AMarioMovingPlatform::AMarioMovingPlatform()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);

	// Moving platform must be movable.
	Mesh->SetMobility(EComponentMobility::Movable);

	// Default collision: blocks pawn. You can override in BP/instance if needed.
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	Mesh->SetGenerateOverlapEvents(false);

	RequiredMoonId = NAME_None;
}

void AMarioMovingPlatform::BeginPlay()
{
	Super::BeginPlay();

	RefreshEndpoints();

	CachedGI = GetGameInstance<UMarioGameInstance>();
	if (CachedGI)
	{
		// Count + specific moon support (둘 중 하나만 써도 됨)
		CachedGI->OnSuperMoonCountChanged.AddDynamic(this, &AMarioMovingPlatform::OnMoonCountChanged);
		CachedGI->OnSuperMoonCollected.AddDynamic(this, &AMarioMovingPlatform::OnMoonCollected);
	}

	TryActivateFromProgress();
}

void AMarioMovingPlatform::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (CachedGI)
	{
		CachedGI->OnSuperMoonCountChanged.RemoveDynamic(this, &AMarioMovingPlatform::OnMoonCountChanged);
		CachedGI->OnSuperMoonCollected.RemoveDynamic(this, &AMarioMovingPlatform::OnMoonCollected);
		CachedGI = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void AMarioMovingPlatform::RefreshEndpoints()
{
	StartLocation = GetActorLocation();
	StartRotation = GetActorRotation();

	if (bUseLocalOffset)
	{
		OffsetWorld = StartRotation.RotateVector(TargetOffset);
	}
	else
	{
		OffsetWorld = TargetOffset;
	}

	EndLocation = StartLocation + OffsetWorld;
}

bool AMarioMovingPlatform::IsRequirementMet() const
{
	// Requirement이 아예 없다면 true 반환은 여기서 하지 않음.
	// (TryActivateFromProgress에서 bStartActive 처리)
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

void AMarioMovingPlatform::TryActivateFromProgress()
{
	if (bTriggerOnlyOnce && bTriggered) return;

	const bool bHasAnyRequirement = (RequiredSuperMoonCount > 0) || (!RequiredMoonId.IsNone());

	if (bHasAnyRequirement)
	{
		if (!IsRequirementMet()) return;

		bTriggered = true;

		// Progress 이벤트(문 획득/카운트 변경)는 "활성화 시점"을 알려주기 위한 용도.
		// 이미 움직이고 있는 플랫폼(bActive=true)에 대해 매번 ResetPhase/RefreshEndpoints를 하면
		// StartLocation이 "현재 위치"로 갱신되어 이동 범위/위치가 틀어지는 문제가 발생한다.
		// 따라서 "비활성 -> 활성" 전환일 때만 ResetPhase를 수행한다.
		const bool bShouldResetPhase = !bActive;
		ActivateMovement(bShouldResetPhase);
		return;
	}

	// No requirements: start immediately if desired.
	if (bStartActive)
	{
		// 요구조건이 없는 플랫폼은 게임 진행(문 획득 등) 이벤트로 재설정하면 안 됨.
		// BeginPlay에서 1회 활성화만 하고, 이후 이벤트에서는 아무 것도 하지 않는다.
		if (!bActive)
		{
			ActivateMovement(true);
		}
	}
}

void AMarioMovingPlatform::ActivateMovement(bool bResetPhase)
{
	if (bActive)
	{
		// Already active; optionally reset phase/endpoints for safety.
		if (bResetPhase)
		{
			RefreshEndpoints();
			MoveElapsed = 0.f;
		}
		return;
	}

	RefreshEndpoints();

	bActive = true;

	if (bResetPhase)
	{
		MoveElapsed = 0.f;
		if (bRandomStartPhase)
		{
			const float Cycle = bPingPong ? (2.f * MoveDuration) : MoveDuration;
			MoveElapsed = FMath::FRandRange(0.f, FMath::Max(Cycle, KINDA_SMALL_NUMBER));
		}
	}

	SetActorTickEnabled(true);
}

void AMarioMovingPlatform::DeactivateMovement()
{
	if (!bActive) return;

	bActive = false;
	SetActorTickEnabled(false);

	// Optional: snap back to start when deactivated (현재는 유지)
	// SetActorLocation(StartLocation);
}

float AMarioMovingPlatform::CalcAlpha(float Elapsed) const
{
	const float Dur = FMath::Max(MoveDuration, 0.01f);

	if (bPingPong)
	{
		const float Period = 2.f * Dur;
		float T = FMath::Fmod(Elapsed, Period);
		if (T < 0.f) T += Period;

		float SegAlpha = (T <= Dur) ? (T / Dur) : (1.f - (T - Dur) / Dur);
		SegAlpha = FMath::Clamp(SegAlpha, 0.f, 1.f);

		return bEaseInOut ? FMath::SmoothStep(0.f, 1.f, SegAlpha) : SegAlpha;
	}
	else
	{
		float T = FMath::Fmod(Elapsed, Dur);
		if (T < 0.f) T += Dur;

		float SegAlpha = FMath::Clamp(T / Dur, 0.f, 1.f);
		return bEaseInOut ? FMath::SmoothStep(0.f, 1.f, SegAlpha) : SegAlpha;
	}
}

void AMarioMovingPlatform::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bActive) return;

	MoveElapsed += DeltaSeconds;

	const float Alpha = CalcAlpha(MoveElapsed);
	const FVector NewLoc = FMath::Lerp(StartLocation, EndLocation, Alpha);
	SetActorLocation(NewLoc);
}

void AMarioMovingPlatform::OnMoonCountChanged(int32 NewTotal)
{
	TryActivateFromProgress();
}

void AMarioMovingPlatform::OnMoonCollected(FName MoonId, int32 NewTotal)
{
	// If RequiredMoonId is specified, we can early-out to reduce calls.
	if (!RequiredMoonId.IsNone() && MoonId != RequiredMoonId)
	{
		// Still might be count-gated only; if RequiredMoonId is set, mismatch means not met.
		// But if count is also used, count change will arrive via OnMoonCountChanged anyway.
		return;
	}

	TryActivateFromProgress();
}
