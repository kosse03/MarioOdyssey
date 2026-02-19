#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MarioHUDWidget.generated.h"

/**
 * Blueprint에서 구현하는 HUD 위젯 베이스.
 * C++는 이벤트만 호출하고, 실제 UI 갱신은 WBP_HUD에서 처리.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class MARIOODYSSEY_API UMarioHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// HP(현재/최대)
	UFUNCTION(BlueprintImplementableEvent, Category="Mario|HUD")
	void BP_OnHPChanged(float CurrentHP, float MaxHP);

	// 코인 카운트
	UFUNCTION(BlueprintImplementableEvent, Category="Mario|HUD")
	void BP_OnCoinsChanged(int32 Coins);

	// 슈퍼문 총 개수
	UFUNCTION(BlueprintImplementableEvent, Category="Mario|HUD")
	void BP_OnSuperMoonCountChanged(int32 SuperMoons);

	// 특정 슈퍼문을 새로 획득했을 때(연출용)
	UFUNCTION(BlueprintImplementableEvent, Category="Mario|HUD")
	void BP_OnSuperMoonCollected(FName MoonId, int32 TotalSuperMoons);
};
