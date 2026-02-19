#include "UI/MarioStartScreenWidget.h"

#include "InputCoreTypes.h"

UMarioStartScreenWidget::UMarioStartScreenWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// bIsFocusable 직접 설정은 UE 5.4+에서 deprecated.
	// 키 입력을 받으려면 BP(WBP_StartScreen)에서 "Is Focusable"을 체크해야 한다.
}

FReply UMarioStartScreenWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();

	if (Key == StartKey)
	{
		BP_OnStartPressed();
		OnStartRequested.Broadcast();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
