#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MarioStartScreenWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStartRequested);

/**
 * Start Screen Widget Base (UMG)
 * - BP에서 배경 이미지/텍스트/애니메이션을 구성
 * - C 키를 누르면 OnStartRequested 를 브로드캐스트
 *
 * 중요:
 * - UE 5.4+에서 UUserWidget::bIsFocusable 직접 접근은 deprecated.
 * - 이 위젯이 키 입력을 받으려면, BP(WBP_StartScreen)에서 "Is Focusable"을 반드시 체크해야 한다.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class MARIOODYSSEY_API UMarioStartScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UMarioStartScreenWidget(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(BlueprintAssignable, Category="Start")
	FOnStartRequested OnStartRequested;

	/** BP에서 "C 눌림" 연출을 하고 싶을 때(선택) */
	UFUNCTION(BlueprintImplementableEvent, Category="Start")
	void BP_OnStartPressed();

protected:
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	/** 시작 키(기본: C) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Start")
	FKey StartKey = EKeys::C;
};
