#include "Character/Monster/BulletBillCharacter.h"

#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

#include "MarioOdyssey/MarioCharacter.h"
#include "Capture/CapturableInterface.h"
#include "Capture/CaptureComponent.h"

ABulletBillCharacter::ABulletBillCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// 캡쳐 가능
	bCapturable = true;

	// BulletBill은 ContactSphere 컨택 데미지(Overlap) 끔(겹치면 틱마다 맞는 문제)
	if (ContactSphere)
	{
		ContactSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ContactSphere->SetGenerateOverlapEvents(false);
	}

	// 캡슐은 스윕 이동으로 충돌 판정
	if (UCapsuleComponent* Cap = GetCapsuleComponent())
	{
		Cap->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Cap->SetCollisionResponseToAllChannels(ECR_Block);
		Cap->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		Cap->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap); // Monster channel
		Cap->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		Cap->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}

	// ===== gravity off =====
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw   = false;
	bUseControllerRotationRoll  = false;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_Flying);
		Move->GravityScale = 0.f;
		Move->bOrientRotationToMovement = false;
		Move->bUseControllerDesiredRotation = false;
		Move->Velocity = FVector::ZeroVector;
	}

	LifeRemain = MaxLifeSeconds;
}

void ABulletBillCharacter::BeginPlay()
{
	Super::BeginPlay();

	LifeRemain = MaxLifeSeconds;

	// pitch/roll은 항상 0으로 고정 (카메라/컨트롤러 회전 영향 제거)
	FRotator R = GetActorRotation();
	R.Pitch = 0.f;
	R.Roll  = 0.f;
	SetActorRotation(R);
}

void ABulletBillCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bExploded) return;

	// 수명 처리
	LifeRemain -= DeltaSeconds;
	if (LifeRemain <= 0.f)
	{
		RequestReleaseIfCapturedAndExplode();
		return;
	}

	// ===== 회전 갱신 =====
	FRotator R = GetActorRotation();
	R.Pitch = 0.f; // BulletBill은 위/아래 조종 없음
	R.Roll  = 0.f;

	if (bIsCaptured)
	{
		// 조향: 좌/우(Yaw)만
		R.Yaw += SteerInput.X * SteerYawRate * DeltaSeconds;
		SetActorRotation(R);
	}
	else if (bHomingToMario)
	{
		if (AMarioCharacter* Mario = GetMarioViewTarget())
		{
			const FVector To = (Mario->GetActorLocation() - GetActorLocation());
			const FRotator Want = To.Rotation();

			R.Yaw = FMath::FixedTurn(R.Yaw, Want.Yaw, HomingYawRate * DeltaSeconds);
			SetActorRotation(R);
		}
	}

	MoveAndHandleHit(DeltaSeconds);
}

void ABulletBillCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// MonsterCharacterBase::SetupPlayerInputComponent()는 IA_Move를 AddMovementInput으로 바인딩함.
	// BulletBill은 AddActorWorldOffset 기반 비행체라서 베이스 바인딩을 건너뛰고, 여기서만 직접 바인딩한다.
	ACharacter::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// IA_Move만 조향으로 사용 (W/S는 무시, A/D만 Yaw)
		if (IA_Move)
		{
			EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &ABulletBillCharacter::Input_Steer);
			EIC->BindAction(IA_Move, ETriggerEvent::Completed, this, &ABulletBillCharacter::Input_Steer);
		}

		// 카메라 회전(마우스 룩) - 캡쳐 중에도 Mario처럼 자유롭게 회전
		if (IA_Look)
		{
			EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &ABulletBillCharacter::Input_LookCamera);
		}


		// 캡쳐 해제(C키)
		if (IA_Crouch)
		{
			EIC->BindAction(IA_Crouch, ETriggerEvent::Started, this, &ABulletBillCharacter::Input_ReleaseCapture_Passthrough);
		}

		// Run/Jump는 베이스 행동 유지(필요하면 제거)
		if (IA_Run)
		{
			EIC->BindAction(IA_Run, ETriggerEvent::Started, this, &ABulletBillCharacter::Input_RunStarted_Passthrough);
			EIC->BindAction(IA_Run, ETriggerEvent::Completed, this, &ABulletBillCharacter::Input_RunCompleted_Passthrough);
		}
		if (IA_Jump)
		{
			EIC->BindAction(IA_Jump, ETriggerEvent::Started, this, &ABulletBillCharacter::Input_JumpStarted_Passthrough);
			EIC->BindAction(IA_Jump, ETriggerEvent::Completed, this, &ABulletBillCharacter::Input_JumpCompleted_Passthrough);
		}
	}
}

void ABulletBillCharacter::Input_Steer(const FInputActionValue& Value)
{
	// IA_Move: FVector2D  (프로젝트 기준: X=Right(A/D), Y=Forward(W/S))
	const FVector2D Axis = Value.Get<FVector2D>();

	SteerInput.X = Axis.X;   // 좌/우만 사용
	SteerInput.Y = 0.f;      // 위/아래 조종 금지
}

void ABulletBillCharacter::Input_LookCamera(const FInputActionValue& Value)
{
	if (bInputLocked) return;
	if (!bIsCaptured) return;

	// ViewTarget이 마리오라서, Look 입력은 Mario(TargetControlRotation)에게 전달해야 한다.
	// Controller Rotation을 직접 건드리면 CaptureTickCamera()가 다음 틱에 원래 값으로 되돌려 "휙 돌아옴/고정" 증상이 발생한다.
	if (CapturingMario.IsValid())
	{
		CapturingMario->CaptureFeedLookInput(Value);
	}
}

void ABulletBillCharacter::Input_ReleaseCapture_Passthrough(const FInputActionValue& Value)
{
	OnReleaseCapturePressed(Value);
}

void ABulletBillCharacter::Input_RunStarted_Passthrough(const FInputActionValue& Value)
{
	Input_RunStarted(Value);
}

void ABulletBillCharacter::Input_RunCompleted_Passthrough(const FInputActionValue& Value)
{
	Input_RunCompleted(Value);
}

void ABulletBillCharacter::Input_JumpStarted_Passthrough(const FInputActionValue& Value)
{
	Input_JumpStarted(Value);
}

void ABulletBillCharacter::Input_JumpCompleted_Passthrough(const FInputActionValue& Value)
{
	Input_JumpCompleted(Value);
}

AMarioCharacter* ABulletBillCharacter::GetMarioViewTarget() const
{
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
		{
			// 정책상 ViewTarget이 Mario라 이게 제일 안정적
			return Cast<AMarioCharacter>(PC->GetViewTarget());
		}
	}
	return nullptr;
}

void ABulletBillCharacter::MoveAndHandleHit(float Dt)
{
	const float Speed = bIsCaptured ? CapturedFlySpeed : FlySpeed;
	const FVector Delta = GetActorForwardVector() * Speed * Dt;

	FHitResult Hit;
	AddActorWorldOffset(Delta, true, &Hit);

	if (Hit.bBlockingHit)
	{
		AActor* HitActor = Hit.GetActor();
		ApplyImpactDamage(HitActor);

		// 캡쳐 중이면 "마리오 위치 기준 해제"가 먼저 일어나야 함
		RequestReleaseIfCapturedAndExplode();
	}
}

void ABulletBillCharacter::ApplyImpactDamage(AActor* HitActor)
{
	if (!HitActor) return;

	const bool bPlayerControlled = (Controller && Controller->IsPlayerController());

	// 캡쳐 중(플레이어 조종): 몬스터에 데미지
	if (bPlayerControlled)
	{
		// 마리오는 무시(원하면 맞게 바꿀 수 있음)
		if (HitActor->IsA(AMarioCharacter::StaticClass())) return;

		const bool bIsMonster =
			HitActor->IsA(AMonsterCharacterBase::StaticClass()) ||
			HitActor->Implements<UCapturableInterface>();

		if (bIsMonster)
		{
			UGameplayStatics::ApplyDamage(HitActor, ImpactDamageToMonstersWhenCaptured, GetController(), this, UDamageType::StaticClass());
		}
		return;
	}

	// 미캡쳐(적): 마리오만 데미지
	if (AMarioCharacter* Mario = Cast<AMarioCharacter>(HitActor))
	{
		UGameplayStatics::ApplyDamage(Mario, ImpactDamageToMario, GetController(), this, UDamageType::StaticClass());
	}
}

void ABulletBillCharacter::RequestReleaseIfCapturedAndExplode()
{
	if (bExploded) return;

	if (bIsCaptured)
	{
		if (AMarioCharacter* Mario = GetMarioViewTarget())
		{
			if (UCaptureComponent* CapComp = Mario->FindComponentByClass<UCaptureComponent>())
			{
				CapComp->ReleaseCapture(ECaptureReleaseReason::Manual);
			}
		}
	}

	ExplodeInternal();
}

void ABulletBillCharacter::ExplodeInternal()
{
	if (bExploded) return;
	bExploded = true;

	// 충돌/가시성 정리
	if (UCapsuleComponent* Cap = GetCapsuleComponent())
	{
		Cap->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	SetActorHiddenInGame(true);

	// BP에서 FX/사운드 처리
	BP_OnExplode();

	// 약간의 딜레이 후 삭제
	SetLifeSpan(0.05f);
}

void ABulletBillCharacter::OnCapturedExtra(AController* Capturer, const FCaptureContext& Context)
{
	Super::OnCapturedExtra(Capturer, Context);
	if (Capturer)
	{
		Capturer->SetIgnoreLookInput(false);
	}

	// captured: manual steering only (no pitch)
	bHomingToMario = false;
	SteerInput = FVector2D::ZeroVector;

	FRotator R = GetActorRotation();
	R.Pitch = 0.f;
	R.Roll  = 0.f;
	SetActorRotation(R);
}

void ABulletBillCharacter::OnReleasedExtra(const FCaptureReleaseContext& Context)
{
	Super::OnReleasedExtra(Context);

	// released: restore homing
	bHomingToMario = true;
	SteerInput = FVector2D::ZeroVector;

	FRotator R = GetActorRotation();
	R.Pitch = 0.f;
	R.Roll  = 0.f;
	SetActorRotation(R);
}

void ABulletBillCharacter::StartSpawnGrace(float GraceSeconds)
{
	if (bExploded) return;

	if (GraceSeconds < 0.f)
	{
		GraceSeconds = DefaultSpawnGraceSeconds;
	}

	if (GraceSeconds <= 0.f)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpawnGraceTimer);
	}

	bSpawnGraceActive = true;

	if (UCapsuleComponent* Cap = GetCapsuleComponent())
	{
		Cap->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// grace 종료 후 충돌 다시 켜기
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(SpawnGraceTimer, this, &ABulletBillCharacter::EndSpawnGrace, GraceSeconds, false);
	}
}

void ABulletBillCharacter::EndSpawnGrace()
{
	bSpawnGraceActive = false;

	if (bExploded) return;

	if (UCapsuleComponent* Cap = GetCapsuleComponent())
	{
		Cap->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		// Response는 ctor에서 이미 설정되어 있으므로 Enabled만 복구
	}
}
