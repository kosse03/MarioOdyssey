#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "MarioGameInstance.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMarioHPChanged, float, CurrentHP, float, MaxHP);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMarioCoinsChanged, int32, NewCoins);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMarioSuperMoonCountChanged, int32, NewTotalMoons);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMarioSuperMoonCollected, FName, MoonId, int32, NewTotalMoons);

UCLASS()
class MARIOODYSSEY_API UMarioGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	// ===== Delegates (UI/HUD가 여기 구독하면 됨) =====
	UPROPERTY(BlueprintAssignable, Category="Progress")
	FOnMarioHPChanged OnHPChanged;

	UPROPERTY(BlueprintAssignable, Category="Progress")
	FOnMarioCoinsChanged OnCoinsChanged;

	UPROPERTY(BlueprintAssignable, Category="Progress")
	FOnMarioSuperMoonCountChanged OnSuperMoonCountChanged;

	UPROPERTY(BlueprintAssignable, Category="Progress")
	FOnMarioSuperMoonCollected OnSuperMoonCollected;

	// ===== HP =====
	UFUNCTION(BlueprintCallable, Category="Progress|HP")
	void SetHP(float InCurrentHP, float InMaxHP);

	UFUNCTION(BlueprintCallable, Category="Progress|HP")
	float GetCurrentHP() const { return CurrentHP; }

	UFUNCTION(BlueprintCallable, Category="Progress|HP")
	float GetMaxHP() const { return MaxHP; }

	UFUNCTION(BlueprintCallable, Category="Progress|HP")
	bool HasSavedHP() const { return bHasHP; }

	// ===== Coins =====
	UFUNCTION(BlueprintCallable, Category="Progress|Coin")
	void AddCoin(int32 Amount = 1);

	UFUNCTION(BlueprintCallable, Category="Progress|Coin")
	int32 GetCoins() const { return Coins; }

	// ===== SuperMoon =====
	UFUNCTION(BlueprintCallable, Category="Progress|SuperMoon")
	bool CollectSuperMoon(FName MoonId, int32 MoonValue = 1);

	UFUNCTION(BlueprintCallable, Category="Progress|SuperMoon")
	bool HasSuperMoon(FName MoonId) const;

	UFUNCTION(BlueprintCallable, Category="Progress|SuperMoon")
	int32 GetSuperMoonCount() const { return SuperMoons; }

	UFUNCTION(BlueprintCallable, Category="Progress|SuperMoon")
	bool HasAtLeastSuperMoons(int32 RequiredCount) const { return SuperMoons >= RequiredCount; }

	// ===== Level Travel (Portal) =====
	// 포탈 이동 시, 다음 맵에서 적용할 스폰 트랜스폼과 페이드 아웃 요청을 GameInstance에 임시 저장한다.
	UFUNCTION(BlueprintCallable, Category="Progress|Travel")
	void SetPendingSpawnTransform(const FTransform& InTransform);

	// Consume: 한 번 읽으면 내부 값이 지워진다.
	UFUNCTION(BlueprintCallable, Category="Progress|Travel")
	bool ConsumePendingSpawnTransform(FTransform& OutTransform);

	UFUNCTION(BlueprintCallable, Category="Progress|Travel")
	void SetPendingFadeOut(float InFadeOutSeconds, bool bLockInputUntilFadeOut = true);

	UFUNCTION(BlueprintCallable, Category="Progress|Travel")
	bool ConsumePendingFadeOut(float& OutFadeOutSeconds, bool& bOutLockInputUntilFadeOut);

	// ===== Arena Intro Cutscene (1회만) =====
	UFUNCTION(BlueprintCallable, Category="Progress|Cutscene")
	bool IsArenaIntroCutscenePlayed() const { return bArenaIntroCutscenePlayed; }

	UFUNCTION(BlueprintCallable, Category="Progress|Cutscene")
	void MarkArenaIntroCutscenePlayed() { bArenaIntroCutscenePlayed = true; }


private:
	// -1이면 미저장 상태로 취급
	UPROPERTY()
	float CurrentHP = -1.f;

	UPROPERTY()
	float MaxHP = 3.f;

	UPROPERTY()
	bool bHasHP = false;

	UPROPERTY()
	int32 Coins = 0;

	UPROPERTY()
	int32 SuperMoons = 0;

	// 런타임 중 중복 획득 방지용 (세이브까지는 아직)
	TSet<FName> CollectedMoonIds;


	// ===== Travel Pending Data =====
	UPROPERTY()
	bool bHasPendingSpawnTransform = false;

	UPROPERTY()
	FTransform PendingSpawnTransform;

	UPROPERTY()
	bool bHasPendingFadeOut = false;

	UPROPERTY()
	float PendingFadeOutSeconds = 0.35f;

	UPROPERTY()
	bool bPendingLockInputUntilFadeOut = true;

	// ===== Arena Intro Cutscene (session 1회) =====
	UPROPERTY()
	bool bArenaIntroCutscenePlayed = false;

};
