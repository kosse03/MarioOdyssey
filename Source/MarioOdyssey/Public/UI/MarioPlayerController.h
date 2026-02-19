#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "MarioPlayerController.generated.h"

class UMarioHUDWidget;
class UMarioGameInstance;

/**
 * HUD 위젯 생성 + GameInstance(진행도/HP/코인/슈퍼문) 변경 -> HUD로 전달
 * - 캡쳐 중에는 몬스터를 Possess 하더라도 HUD는 계속 Mario 기준으로 유지되어야 하므로
 *   Character가 아니라 GameInstance에 바인딩한다.
 */
UCLASS()
class MARIOODYSSEY_API AMarioPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AMarioPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// WBP_HUD (UMarioHUDWidget 파생)을 지정(에디터에서 바꿀 수 있음)
	UPROPERTY(EditDefaultsOnly, Category="Mario|HUD")
	TSubclassOf<UMarioHUDWidget> HUDWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UMarioHUDWidget> HUDWidget = nullptr;

private:
	TWeakObjectPtr<UMarioGameInstance> BoundGI;

	void CreateHUDIfNeeded();
	void BindToGameInstance(UMarioGameInstance* GI);
	void UnbindFromGameInstance();
	void PushAllToHUD(UMarioGameInstance* GI);

	UFUNCTION()
	void HandleHPChanged(float CurrentHP, float MaxHP);

	UFUNCTION()
	void HandleCoinsChanged(int32 Coins);

	UFUNCTION()
	void HandleSuperMoonCountChanged(int32 SuperMoons);

	UFUNCTION()
	void HandleSuperMoonCollected(FName MoonId, int32 TotalSuperMoons);
};
