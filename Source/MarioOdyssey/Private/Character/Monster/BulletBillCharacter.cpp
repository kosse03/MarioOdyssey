#include "Character/Monster/BulletBillCharacter.h"

#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/DamageType.h"
#include "TimerManager.h"

#include "MarioOdyssey/MarioCharacter.h"
#include "Capture/CaptureComponent.h"

ABulletBillCharacter::ABulletBillCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// 캡쳐 가능
	bCapturable = true;

	// BulletBill은 ContactSphere로 컨택 데미지 처리하면 "겹쳐서 계속 맞는" 문제가 생기므로 끔.
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
		Cap->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
		Cap->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}

	LifeRemain = MaxLifeSeconds;
}

void ABulletBillCharacter::BeginPlay()
{
	Super::BeginPlay();
	LifeRemain = MaxLifeSeconds;
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

	// 회전 갱신(캡쳐=플레이어 조종 / 미캡쳐=유도)
	FRotator R = GetActorRotation();

	if (bIsCaptured)
	{
		// 조향: X=Right(Yaw), Y=Up(Pitch) 가정 (필요하면 반대로 바꿔)
		R.Yaw   += SteerInput.X * SteerYawRate   * DeltaSeconds;
		R.Pitch += SteerInput.Y * SteerPitchRate * DeltaSeconds;
		R.Pitch = FMath::ClampAngle(R.Pitch, PitchMin, PitchMax);
		SetActorRotation(R);
	}
	else if (bHomingToMario)
	{
		if (AMarioCharacter* Mario = GetMarioViewTarget())
		{
			const FVector To = (Mario->GetActorLocation() - GetActorLocation());
			const FRotator Want = To.Rotation();

			const float NewYaw = FMath::FixedTurn(R.Yaw, Want.Yaw, HomingYawRate * DeltaSeconds);
			R.Yaw = NewYaw;
			R.Pitch = 0.f; // 적 상태는 pitch 고정(원하면 Want.Pitch로 유도 가능)
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
		// Move/Look를 조향으로 사용
		if (IA_Move)
		{
			EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &ABulletBillCharacter::Input_Steer);
			EIC->BindAction(IA_Move, ETriggerEvent::Completed, this, &ABulletBillCharacter::Input_Steer);
		}
		if (IA_Look)
		{
			EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &ABulletBillCharacter::Input_LookForSteer);
			EIC->BindAction(IA_Look, ETriggerEvent::Completed, this, &ABulletBillCharacter::Input_LookForSteer);
		}

		// 캡쳐 해제(C키)
		if (IA_Crouch)
		{
			EIC->BindAction(IA_Crouch, ETriggerEvent::Started, this, &ABulletBillCharacter::Input_ReleaseCapture_Passthrough);
		}

		// Run/Jump는 베이스 행동을 유지(필요하면 제거 가능)
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
	// IA_Move: FVector2D
	const FVector2D Axis = Value.Get<FVector2D>();
	SteerInput = Axis;
}

void ABulletBillCharacter::Input_LookForSteer(const FInputActionValue& Value)
{
	// IA_Look도 FVector2D라면 조향에 합산
	const FVector2D Axis = Value.Get<FVector2D>();
	SteerInput = Axis;
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
			return Cast<AMarioCharacter>(PC->GetViewTarget()); // 정책상 ViewTarget이 Mario라 이게 제일 안정적
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

		// 캡쳐 중이면 마리오 위치 기준 해제가 먼저 일어나야 함(캡쳐 폰을 그냥 Destroy하면 꼬일 수 있음)
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

		// "몬스터 판별 추천형태": 인터페이스/베이스로 판별
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
