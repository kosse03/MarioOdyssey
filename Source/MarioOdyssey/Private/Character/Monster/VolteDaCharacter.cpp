#include "Character/Monster/VolteDaCharacter.h"

#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

AVolteDaCharacter::AVolteDaCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	

bUseControllerRotationYaw = false;
if (UCharacterMovementComponent* Move = GetCharacterMovement())
{
    Move->bOrientRotationToMovement = true;
}

// Keep base captured move params in sync (so Input_RunStarted / Completed sets the intended speeds)
CapturedWalkSpeed = CapturedSpeed_GlassesOn;
CapturedRunSpeed  = CapturedSpeed_GlassesOff;
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


	// 마리오 상태(비캡쳐)에서는 절대 보이면 안됨: 시작 시 강제로 숨김
	ApplyRevealVisibility(false);
	// Default to "walk mode" effect (visible) until the run input toggles it off.
	SetGlassesOn(true);
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

	bRevealVisibleApplied = false;
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

void AVolteDaCharacter::ApplyRevealVisibility(bool bVisible)
{
	// 마리오 상태에서는 절대 보이면 안됨:
	// bVisible은 (bIsCaptured && bGlassesOn)일 때만 true로 들어오게 설계한다.
	if (bRevealVisibleApplied == bVisible)
	{
		return;
	}

	bRevealVisibleApplied = bVisible;

	for (TWeakObjectPtr<AActor>& W : RevealActors)
	{
		if (AActor* A = W.Get())
		{
			A->SetActorHiddenInGame(!bVisible);
		}
	}
}


void AVolteDaCharacter::SetGlassesOn(bool bOn)
{
	bGlassesOn = bOn;

	// "보이게"는 캡쳐 중 + 안경 ON일 때만 허용
	ApplyRevealVisibility(bIsCaptured && bGlassesOn);

	// 속도도 즉시 반영
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



void AVolteDaCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // ===== Reveal control while captured =====
    if (bIsCaptured && bRevealOnlyWhileWalking)
    {
        const bool bDesiredReveal = (!bRunHeld);
        SetGlassesOn(bDesiredReveal);
    }

    // ===== Simple flee AI (when NOT captured) =====
    if (!bIsCaptured && bEnableFleeAI)
    {
        AActor* Player = UGameplayStatics::GetPlayerPawn(this, 0);
        if (!Player) return;

        const FVector SelfLoc = GetActorLocation();
        const FVector ToPlayer = Player->GetActorLocation() - SelfLoc;

        const float Dist2D = FVector(ToPlayer.X, ToPlayer.Y, 0.f).Size();
        if (Dist2D <= KINDA_SMALL_NUMBER) return;

        const FVector DirToPlayer2D = FVector(ToPlayer.X, ToPlayer.Y, 0.f).GetSafeNormal();
        const FVector Fwd2D = FVector(GetActorForwardVector().X, GetActorForwardVector().Y, 0.f).GetSafeNormal();

        const float FacingDot = FVector::DotProduct(Fwd2D, DirToPlayer2D);

                const FVector PlayerLoc = Player->GetActorLocation();
                const FVector PlayerFwd2D = FVector(Player->GetActorForwardVector().X, Player->GetActorForwardVector().Y, 0.f).GetSafeNormal();
                const FVector DirFromPlayerToSelf2D = FVector(SelfLoc.X - PlayerLoc.X, SelfLoc.Y - PlayerLoc.Y, 0.f).GetSafeNormal();
                const float PlayerLookDot = FVector::DotProduct(PlayerFwd2D, DirFromPlayerToSelf2D);

        if (Dist2D <= FleeDetectDistance && (FacingDot >= FleeSeeDotThreshold || PlayerLookDot >= FleeSeeDotThreshold))
        {
            FleeRemain = FleePersistSeconds;
        }

        if (FleeRemain > 0.f)
        {
            FleeRemain -= DeltaSeconds;

            // Move away from player
            const FVector AwayDir = -DirToPlayer2D;
            AddMovementInput(AwayDir, 1.0f);

            // If we are not captured, keep normal speed (run) while fleeing
            if (UCharacterMovementComponent* Move = GetCharacterMovement())
            {
                Move->MaxWalkSpeed = CapturedSpeed_GlassesOff;
            }
        }
    }
}

