#include "Character/Monster/VolteDaCharacter.h"

#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"

AVolteDaCharacter::AVolteDaCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	bCapturable = true;

	// 볼테다는 컨택 데미지/넉백이 불필요한 타입
	if (ContactSphere)
	{
		ContactSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ContactSphere->SetGenerateOverlapEvents(false);
	}
}

void AVolteDaCharacter::BeginPlay()
{
	Super::BeginPlay();

	CacheRevealActors();
	
	SetGlassesOn(false);
}

void AVolteDaCharacter::OnCapturedExtra(AController* Capturer, const FCaptureContext& Context)
{
	// 캡쳐 시작: 기본은 선글라스 ON(느림 + 숨김 오브젝트 보이기)
	CacheRevealActors();
	SetGlassesOn(true);
}

void AVolteDaCharacter::OnReleasedExtra(const FCaptureReleaseContext& Context)
{
	// 캡쳐 해제되면 안경 OFF + 숨긴 오브젝트 원복
	SetGlassesOn(false);
}


void AVolteDaCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 베이스는 Run을 "빨라짐" 용도로 쓰지만,
	// 볼테다는 Run 홀드 = 안경 OFF(빠름), Run 릴리즈 = 안경 ON(느림+보임)로 바꿔치기
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (IA_Run)
		{
			EIC->BindAction(IA_Run, ETriggerEvent::Started, this, &AVolteDaCharacter::Input_RunStarted_Proxy);
			EIC->BindAction(IA_Run, ETriggerEvent::Completed, this, &AVolteDaCharacter::Input_RunCompleted_Proxy);
		}

		// 캡쳐 해제(C키)
		if (IA_Crouch)
		{
			EIC->BindAction(IA_Crouch, ETriggerEvent::Started, this, &AVolteDaCharacter::Input_ReleaseCapture_Passthrough);
		}
	}
}

void AVolteDaCharacter::Input_RunStarted_Proxy(const FInputActionValue& Value)
{
	// Run 홀드 = 안경 OFF (빠르게)
	SetGlassesOn(false);

	// 베이스 상태 
	Input_RunStarted(Value);
}

void AVolteDaCharacter::Input_RunCompleted_Proxy(const FInputActionValue& Value)
{
	// Run 뗌 = 안경 ON (느리게 + 보이게)
	SetGlassesOn(true);

	Input_RunCompleted(Value);
}

void AVolteDaCharacter::Input_ReleaseCapture_Passthrough(const FInputActionValue& Value)
{
	OnReleaseCapturePressed(Value);
}

void AVolteDaCharacter::CacheRevealActors()
{
	RevealActors.Reset();

	UWorld* World = GetWorld();
	if (!World) return;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (!A) continue;

		if (A->ActorHasTag(RevealActorTag))
		{
			RevealActors.Add(A);
		}
	}
}

void AVolteDaCharacter::SetGlassesOn(bool bOn)
{
	bGlassesOn = bOn;

	// 캡쳐 중일 때만 “보이게” 토글
	if (bIsCaptured)
	{
		for (TWeakObjectPtr<AActor>& W : RevealActors)
		{
			if (AActor* A = W.Get())
			{
				// HiddenInGame만 토글 (collision에는 영향 없음)
				A->SetActorHiddenInGame(!bGlassesOn);
			}
		}
	}

	ApplyCapturedSpeed();
}

void AVolteDaCharacter::ApplyCapturedSpeed()
{
	if (!bIsCaptured) return;

	if (UCharacterMovementComponent* M = GetCharacterMovement())
	{
		M->MaxWalkSpeed = bGlassesOn ? CapturedSpeed_GlassesOn : CapturedSpeed_GlassesOff;
	}
}
