#include "Progress/MarioGameInstance.h"

void UMarioGameInstance::SetHP(float InCurrentHP, float InMaxHP)
{
	MaxHP = FMath::Max(0.f, InMaxHP);
	CurrentHP = FMath::Clamp(InCurrentHP, 0.f, MaxHP);
	bHasHP = true;

	OnHPChanged.Broadcast(CurrentHP, MaxHP);
}

void UMarioGameInstance::AddCoin(int32 Amount)
{
	if (Amount <= 0) return;
	Coins = FMath::Max(0, Coins + Amount);
	OnCoinsChanged.Broadcast(Coins);
}

bool UMarioGameInstance::CollectSuperMoon(FName MoonId, int32 MoonValue)
{
	if (MoonId.IsNone())
	{
		// None은 중복 방지 불가 -> 그냥 수량만 올림
		MoonValue = FMath::Max(1, MoonValue);
		SuperMoons += MoonValue;
		OnSuperMoonCountChanged.Broadcast(SuperMoons);
		OnSuperMoonCollected.Broadcast(MoonId, SuperMoons);
		return true;
	}

	if (CollectedMoonIds.Contains(MoonId))
	{
		return false;
	}

	CollectedMoonIds.Add(MoonId);
	MoonValue = FMath::Max(1, MoonValue);
	SuperMoons += MoonValue;

	OnSuperMoonCountChanged.Broadcast(SuperMoons);
	OnSuperMoonCollected.Broadcast(MoonId, SuperMoons);
	return true;
}

bool UMarioGameInstance::HasSuperMoon(FName MoonId) const
{
	if (MoonId.IsNone()) return false;
	return CollectedMoonIds.Contains(MoonId);
}


void UMarioGameInstance::SetPendingSpawnTransform(const FTransform& InTransform)
{
	PendingSpawnTransform = InTransform;
	bHasPendingSpawnTransform = true;
}

bool UMarioGameInstance::ConsumePendingSpawnTransform(FTransform& OutTransform)
{
	if (!bHasPendingSpawnTransform)
	{
		return false;
	}

	OutTransform = PendingSpawnTransform;
	bHasPendingSpawnTransform = false;
	return true;
}

void UMarioGameInstance::SetPendingFadeOut(float InFadeOutSeconds, bool bLockInputUntilFadeOut)
{
	PendingFadeOutSeconds = FMath::Max(0.f, InFadeOutSeconds);
	bPendingLockInputUntilFadeOut = bLockInputUntilFadeOut;
	bHasPendingFadeOut = true;
}

bool UMarioGameInstance::ConsumePendingFadeOut(float& OutFadeOutSeconds, bool& bOutLockInputUntilFadeOut)
{
	if (!bHasPendingFadeOut)
	{
		return false;
	}

	OutFadeOutSeconds = PendingFadeOutSeconds;
	bOutLockInputUntilFadeOut = bPendingLockInputUntilFadeOut;
	bHasPendingFadeOut = false;
	return true;
}
