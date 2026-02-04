#include "Character/Monster/MonsterCharacterBase.h"

#include "AIController.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "MarioOdyssey/MarioCharacter.h"
#include "Capture/CaptureComponent.h"

AMonsterCharacterBase::AMonsterCharacterBase()
{
	PrimaryActorTick.bCanEverTick = true;

	// Contact Sphere
	ContactSphere = CreateDefaultSubobject<USphereComponent>(TEXT("ContactSphere"));
	ContactSphere->SetupAttachment(RootComponent);
	ContactSphere->InitSphereRadius(70.f);

	// 프로젝트 프리셋 사용
	if (!ContactSphereProfileName.IsNone())
	{
		ContactSphere->SetCollisionProfileName(ContactSphereProfileName);
	}

	ContactSphere->SetGenerateOverlapEvents(true);
	ContactSphere->OnComponentBeginOverlap.AddDynamic(this, &AMonsterCharacterBase::OnContactBeginOverlap);
}

void AMonsterCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		DefaultMaxWalkSpeed = Move->MaxWalkSpeed;
		DefaultJumpZVelocity = Move->JumpZVelocity;
	}
}

void AMonsterCharacterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 이 함수는 “플레이어가 Possess했을 때”만 의미가 있음.
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (IA_Move)
		{
			EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &AMonsterCharacterBase::Input_Move);
		}

		if (IA_Look)
		{
			EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AMonsterCharacterBase::Input_Look);
		}

		// 캡쳐 해제(C키)
		if (IA_Crouch)
		{
			EIC->BindAction(IA_Crouch, ETriggerEvent::Started, this, &AMonsterCharacterBase::OnReleaseCapturePressed);
		}

		if (IA_Jump)
		{
			EIC->BindAction(IA_Jump, ETriggerEvent::Started, this, &AMonsterCharacterBase::Input_JumpStarted);
			EIC->BindAction(IA_Jump, ETriggerEvent::Completed, this, &AMonsterCharacterBase::Input_JumpCompleted);
		}

		if (IA_Run)
		{
			EIC->BindAction(IA_Run, ETriggerEvent::Started, this, &AMonsterCharacterBase::Input_RunStarted);
			EIC->BindAction(IA_Run, ETriggerEvent::Completed, this, &AMonsterCharacterBase::Input_RunCompleted);
		}
	}
}

void AMonsterCharacterBase::OnContactBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor) return;

	// 본인이 캡쳐중이면(플레이어 조종) 컨택 공격을 막는 옵션
	if (bDisableContactDamageWhileCaptured && bIsCaptured) return;

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!PlayerPawn || OtherActor != PlayerPawn) return;

	// Mario는 자신의 캡슐 Hit로 컨택 데미지를 처리하므로, 기본값은 중복을 막는다.
	if (!bContactDamageAffectsMario && PlayerPawn->IsA(AMarioCharacter::StaticClass()))
	{
		return;
	}
 // “플레이어가 조종 중인 Pawn”만 때림

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastContactDamageTime < ContactDamageCooldown) return;
	LastContactDamageTime = Now;

	UGameplayStatics::ApplyDamage(PlayerPawn, ContactDamage, GetController(), this, UDamageType::StaticClass());

	// 넉백(플레이어가 몬스터여도 ACharacter면 LaunchCharacter로 밀림)
	if (ACharacter* HitChar = Cast<ACharacter>(PlayerPawn))
	{
		FVector Dir = (HitChar->GetActorLocation() - GetActorLocation());
		Dir.Z = 0.f;
		Dir = Dir.GetSafeNormal();
		HitChar->LaunchCharacter(Dir * KnockbackStrength + FVector(0, 0, KnockbackUp), true, true);
	}
}

void AMonsterCharacterBase::OnReleaseCapturePressed(const FInputActionValue& Value)
{
	if (!bIsCaptured) return;

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC) return;

	// ViewTarget은 마리오 유지
	AMarioCharacter* Mario = Cast<AMarioCharacter>(PC->GetViewTarget());
	if (!Mario) return;

	if (UCaptureComponent* CapComp = Mario->FindComponentByClass<UCaptureComponent>())
	{
		CapComp->ReleaseCapture(ECaptureReleaseReason::Manual);
	}
}

void AMonsterCharacterBase::ApplyCapturedMoveParams()
{
	if (!bIsCaptured) return;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->MaxWalkSpeed = bRunHeld ? CapturedRunSpeed : CapturedWalkSpeed;
		Move->JumpZVelocity = CapturedJumpZVelocity;
	}
}

void AMonsterCharacterBase::StopAIMove()
{
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		AI->StopMovement();
	}
}

void AMonsterCharacterBase::Input_Move(const FInputActionValue& Value)
{
	if (bInputLocked) return;

	const FVector2D Axis = Value.Get<FVector2D>();
	if (!Controller) return;

	const FRotator ControlRot = Controller->GetControlRotation();
	const FRotator YawRot(0.f, ControlRot.Yaw, 0.f);

	const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);

	AddMovementInput(Forward, Axis.Y);
	AddMovementInput(Right, Axis.X);
}

void AMonsterCharacterBase::Input_RunStarted(const FInputActionValue& Value)
{
	if (bInputLocked) return;
	if (!Controller || !Controller->IsPlayerController()) return;

	bRunHeld = true;
	ApplyCapturedMoveParams();
}

void AMonsterCharacterBase::Input_RunCompleted(const FInputActionValue& Value)
{
	if (!Controller || !Controller->IsPlayerController()) return;

	bRunHeld = false;
	ApplyCapturedMoveParams();
}

void AMonsterCharacterBase::Input_JumpStarted(const FInputActionValue& Value)
{
	if (bInputLocked) return;
	if (!Controller || !Controller->IsPlayerController()) return;
	if (!bIsCaptured) return; // 캡쳐 중에만 점프 허용

	Jump();
}

void AMonsterCharacterBase::Input_JumpCompleted(const FInputActionValue& Value)
{
	if (!Controller || !Controller->IsPlayerController()) return;

	StopJumping();
}

void AMonsterCharacterBase::Input_Look(const FInputActionValue& Value)
{
	if (bInputLocked) return;
	if (!bIsCaptured) return;

	// ViewTarget이 마리오라서, Look 입력은 마리오에게 전달
	if (CapturingMario.IsValid())
	{
		CapturingMario->CaptureFeedLookInput(Value);
	}
}

void AMonsterCharacterBase::OnCaptured_Implementation(AController* Capturer, const FCaptureContext& Context)
{
	bIsCaptured = true;
	bRunHeld = false;
	bInputLocked = false;

	StopAIMove();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CapturedHitStunTimer);
	}

	// 캡쳐 시작 순간엔 Capturer의 Pawn이 마리오인 상태라 캐싱 가능
	CapturingMario = Capturer ? Cast<AMarioCharacter>(Capturer->GetPawn()) : nullptr;

	ApplyCapturedMoveParams();
	OnCapturedExtra(Capturer, Context);
}

void AMonsterCharacterBase::OnReleased_Implementation(const FCaptureReleaseContext& Context)
{
	bIsCaptured = false;
	bRunHeld = false;
	bInputLocked = false;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CapturedHitStunTimer);
	}

	// 이동 파라미터 원복
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		if (DefaultMaxWalkSpeed > 0.f) Move->MaxWalkSpeed = DefaultMaxWalkSpeed;
		if (DefaultJumpZVelocity > 0.f) Move->JumpZVelocity = DefaultJumpZVelocity;
	}

	CapturingMario.Reset();
	OnReleasedExtra(Context);
}

void AMonsterCharacterBase::OnCapturedPawnDamaged_Implementation(float Damage, AController* InstigatorController, AActor* DamageCauser)
{
	// 캡쳐 중에만 리액션(HP는 캡쳐 컴포넌트가 마리오로 라우팅)
	if (!bIsCaptured) return;

	FVector FromLoc = GetActorLocation() - GetActorForwardVector() * 100.f;
	if (DamageCauser)
	{
		FromLoc = DamageCauser->GetActorLocation();
	}
	else if (InstigatorController && InstigatorController->GetPawn())
	{
		FromLoc = InstigatorController->GetPawn()->GetActorLocation();
	}

	FVector Dir = (GetActorLocation() - FromLoc);
	Dir.Z = 0.f;
	Dir = Dir.GetSafeNormal();

	LaunchCharacter(Dir * CapturedHitKnockbackStrength + FVector(0, 0, CapturedHitKnockbackUp), true, true);

	// 잠시 입력 잠금
	bInputLocked = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CapturedHitStunTimer);
		World->GetTimerManager().SetTimer(CapturedHitStunTimer, this, &AMonsterCharacterBase::ClearCapturedHitStun, CapturedHitStunSeconds, false);
	}

	OnCapturedPawnDamagedExtra(Damage, InstigatorController, DamageCauser);
}

void AMonsterCharacterBase::ClearCapturedHitStun()
{
	bInputLocked = false;
}
