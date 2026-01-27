#include "Character/GoombaCharacter.h"
#include "EnhancedInputComponent.h"
#include "MarioOdyssey/MarioCharacter.h"
#include "Capture/CaptureComponent.h"
AGoombaCharacter::AGoombaCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

}

void AGoombaCharacter::BeginPlay()
{
	Super::BeginPlay();
	
}

void AGoombaCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AGoombaCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC) return;

	if (IA_Crouch)
	{
		EIC->BindAction(IA_Crouch, ETriggerEvent::Started, this, &AGoombaCharacter::OnReleaseCapturePressed);
	}
}

void AGoombaCharacter::OnCaptured_Implementation(AController* Capturer, const FCaptureContext& Context)
{
	// 캡쳐 시작 시점엔 아직 PC가 마리오를 Possess 중이므로 여기서 마리오 캐시 가능
	if (Capturer)
	{
		CapturingMario = Cast<AMarioCharacter>(Capturer->GetPawn());
	}
}

void AGoombaCharacter::OnReleaseCapturePressed(const FInputActionValue& Value)
{
	if (!CapturingMario.IsValid()) return;

	if (UCaptureComponent* Cap = CapturingMario->GetCaptureComp())
	{
		if (Cap->IsCapturing())
		{
			Cap->ReleaseCapture(ECaptureReleaseReason::Manual);
		}
	}
}

