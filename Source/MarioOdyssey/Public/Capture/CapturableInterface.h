#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Capture/CaptureTypes.h"
#include "CapturableInterface.generated.h"

UINTERFACE(BlueprintType)
class MARIOODYSSEY_API UCapturableInterface : public UInterface
{
	GENERATED_BODY()
};

class MARIOODYSSEY_API ICapturableInterface
{
	GENERATED_BODY()

public:
	// 캡쳐 가능 여부(불가면 데미지 처리 쪽으로)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Capture")
	bool CanBeCaptured(const FCaptureContext& Context) const;

	// 실제로 Possess할 Pawn (자기 자신)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Capture")
	APawn* GetCapturePawn() const;

	// 캡쳐 성공 시점(모자 표시/AI 정지 등)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Capture")
	void OnCaptured(AController* Capturer, const FCaptureContext& Context);

	// 캡쳐 해제 시점(3초 스턴 후 탐지 복귀 등)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Capture")
	void OnReleased(const FCaptureReleaseContext& Context);

	// 캡쳐 중 데미지 이벤트 발생 시(리액션/넉백/스턴 진입용)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Capture")
	void OnCapturedPawnDamaged(float Damage, AController* InstigatorController, AActor* DamageCauser);
};
