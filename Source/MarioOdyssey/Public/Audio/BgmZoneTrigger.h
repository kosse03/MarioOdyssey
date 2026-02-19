#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BgmZoneTrigger.generated.h"

class UBoxComponent;
class USoundBase;

/**
 * BGM Zone Trigger (데이터 + 영역)
 * - ABgmManager가 이 액터들의 Box 영역을 기준으로 "플레이어 위치"를 폴링해서 구역 판정을 한다.
 *
 * 장점:
 * - 캡쳐(빙의)로 Pawn이 바뀌어도 ViewTarget(Mario) 위치 기준으로 안정적으로 동작
 * - Collision/Overlap 이벤트에 의존하지 않아서 WallDetector 같은 트레이스에 덜 휘둘림
 */
UCLASS()
class MARIOODYSSEY_API ABgmZoneTrigger : public AActor
{
	GENERATED_BODY()

public:
	ABgmZoneTrigger();

	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BGM|Components")
	USceneComponent* Root = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BGM|Components")
	UBoxComponent* Trigger = nullptr;

	/** 이 구역에서 재생할 BGM */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BGM")
	USoundBase* ZoneTrack = nullptr;

	/** 우선순위: 큰 값이 우선. (겹치는 구역 대비) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BGM")
	int32 Priority = 0;

	/** 이 구역으로 전환할 때 페이드 시간 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BGM", meta=(ClampMin="0.0"))
	float FadeSeconds = 1.0f;

	/** 구역에서 나가면 복귀 처리(권장 true) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BGM")
	bool bRevertOnExit = true;

	/** 월드 좌표가 이 박스 영역 안에 있는지(회전/스케일까지 고려) */
	UFUNCTION(BlueprintPure, Category="BGM")
	bool IsLocationInside(const FVector& WorldLocation) const;

private:
	FVector GetUnscaledExtent() const;
};
