#include "Capture/CaptureComponent.h"

#include "Capture/CapturableInterface.h"


#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "MarioOdyssey/MarioCharacter.h"

UCaptureComponent::UCaptureComponent()
{
	PrimaryComponentTick.bCanEverTick = true; // 캡쳐 중 ViewTarget 유지용(최적화는 나중)
}

void UCaptureComponent::BeginPlay()
{
	Super::BeginPlay();

	OriginalMario = Cast<AMarioCharacter>(GetOwner());
	CachePlayerControllerIfNeeded();
}

void UCaptureComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsCapturing) return;

	CachePlayerControllerIfNeeded();
	if (CachedPC.IsValid() && OriginalMario.IsValid())
	{
		// ViewTarget은 항상 마리오 유지
		if (CachedPC->GetViewTarget() != OriginalMario.Get())
		{
			CachedPC->SetViewTarget(OriginalMario.Get());
		}
		// Possess로 인해 자동 카메라 타겟 관리가 다시 켜질 수 있으니 방어
		CachedPC->bAutoManageActiveCameraTarget = false;
	}
	
	if (OriginalMario.IsValid() && CachedPC.IsValid())
	{
		OriginalMario->CaptureTickCamera(DeltaTime, CachedPC.Get());
	}
	
}

void UCaptureComponent::CachePlayerControllerIfNeeded()
{
	if (CachedPC.IsValid()) return;

	if (UWorld* World = GetWorld())
	{
		CachedPC = World->GetFirstPlayerController();
	}
}

bool UCaptureComponent::TryCapture(AActor* TargetActor, const FCaptureContext& Context)
{
	if (bIsCapturing) return false;
	if (!OriginalMario.IsValid()) OriginalMario = Cast<AMarioCharacter>(GetOwner());
	if (!OriginalMario.IsValid()) return false;

	CachePlayerControllerIfNeeded();
	if (!CachedPC.IsValid()) return false;

	if (!TargetActor) return false;
	if (!TargetActor->GetClass()->ImplementsInterface(UCapturableInterface::StaticClass())) return false;

	const bool bCan = ICapturableInterface::Execute_CanBeCaptured(TargetActor, Context);
	if (!bCan) return false;

	APawn* NewPawn = ICapturableInterface::Execute_GetCapturePawn(TargetActor);
	if (!NewPawn)
	{
		NewPawn = Cast<APawn>(TargetActor);
	}
	if (!NewPawn) return false;

	// 상태 저장
	CapturedActor = TargetActor;
	CapturedPawn = NewPawn;

	// 데미지 라우팅: "맞는 건 몬스터, HP는 마리오"
	NewPawn->OnTakeAnyDamage.AddDynamic(this, &UCaptureComponent::HandleCapturedPawnAnyDamage);

	// 몬스터 측 캡쳐 진입 콜백(모자 표시/AI 정지 등)
	ICapturableInterface::Execute_OnCaptured(TargetActor, CachedPC.Get(), Context);
	
	OriginalMario->OnCaptureBegin();//상태 정리
	
	// 마리오 숨김/비활성 + Attach
	ApplyMarioCaptureHide(true);
	AttachMarioToCapturedPawn();
	
	//카메라
	OriginalMario->SetCaptureControlRotationOverride(true);
	OriginalMario->CaptureSyncTargetControlRotation(CachedPC->GetControlRotation());
	OriginalMario->SetCaptureControlRotation(CachedPC->GetControlRotation());
	// 컨트롤: 몬스터 Possess, 카메라: 마리오 유지
	bPrevAutoManageCameraTarget = CachedPC->bAutoManageActiveCameraTarget;
	CachedPC->bAutoManageActiveCameraTarget = false;

	CachedPC->Possess(NewPawn);
	CachedPC->SetViewTarget(OriginalMario.Get());

	bIsCapturing = true;
	return true;
}

bool UCaptureComponent::ReleaseCapture(ECaptureReleaseReason Reason)
{
	if (!bIsCapturing) return false;
	if (!OriginalMario.IsValid()) return false;
	CachePlayerControllerIfNeeded();
	if (!CachedPC.IsValid()) return false;

	APawn* CPawn = CapturedPawn.Get();
	AActor* CActor = CapturedActor.Get();

	// 데미지 델리게이트 해제
	if (CPawn)
	{
		CPawn->OnTakeAnyDamage.RemoveDynamic(this, &UCaptureComponent::HandleCapturedPawnAnyDamage);
	}

	// 마리오 출구 위치 계산(정면+위, 막히면 NavMesh 보정)
	FVector ExitLoc = OriginalMario->GetActorLocation();
	ComputeExitLocation(ExitLoc);

	// 마리오 Detach 후 복귀
	DetachMario();
	OriginalMario->SetActorLocation(ExitLoc, false, nullptr, ETeleportType::TeleportPhysics);
	ApplyMarioCaptureHide(false);

	// 컨트롤 복귀 + ViewTarget 유지(마리오)
	CachedPC->Possess(OriginalMario.Get());
	CachedPC->SetViewTarget(OriginalMario.Get());
	CachedPC->bAutoManageActiveCameraTarget = bPrevAutoManageCameraTarget;
	
	OriginalMario->OnCaptureEnd();//마리오 상태 정리
	
	if (OriginalMario.IsValid())
	{
		OriginalMario->SetCaptureControlRotationOverride(false);
	}
	
	// 굼바(캡쳐 대상)도 AI 컨트롤러 재생성/재포제스
	if (CPawn && CPawn->GetController() == nullptr)
	{
		CPawn->SpawnDefaultController();
	}
	// 몬스터 측 해제 콜백(3초 스턴/탐지 복귀 등)
	if (CActor && CActor->GetClass()->ImplementsInterface(UCapturableInterface::StaticClass()))
	{
		FCaptureReleaseContext Rctx;
		Rctx.Reason = Reason;
		Rctx.ReleasedBy = OriginalMario.Get();
		Rctx.ExitLocation = ExitLoc;
		
		ICapturableInterface::Execute_OnReleased(CActor, Rctx);
	}

	CapturedActor.Reset();
	CapturedPawn.Reset();
	bIsCapturing = false;

	return true;
}

void UCaptureComponent::ForceReleaseForGameOver()
{
	ReleaseCapture(ECaptureReleaseReason::GameOver);
}

void UCaptureComponent::ApplyMarioCaptureHide(bool bHide)
{
	if (!OriginalMario.IsValid()) return;

	// 정책: Hidden + NoCollision + MovementOff, Tick은 유지
	OriginalMario->SetActorHiddenInGame(bHide);
	OriginalMario->SetActorEnableCollision(!bHide);

	if (UCharacterMovementComponent* Move = OriginalMario->GetCharacterMovement())
	{
		if (bHide)
		{
			Move->StopMovementImmediately();
			Move->SetMovementMode(MOVE_None);
		}
		else
		{
			Move->SetMovementMode(MOVE_Walking);
		}
	}
}

void UCaptureComponent::AttachMarioToCapturedPawn()
{
	if (!OriginalMario.IsValid()) return;
	if (!CapturedPawn.IsValid()) return;

	// 정책: 캡쳐 중 마리오는 캡쳐된 몬스터에 Attach
	OriginalMario->AttachToActor(CapturedPawn.Get(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
}

void UCaptureComponent::DetachMario()
{
	if (!OriginalMario.IsValid()) return;
	OriginalMario->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
}

bool UCaptureComponent::ComputeExitLocation(FVector& OutExitLocation) const
{
	if (!CapturedPawn.IsValid()) return false;

	const FVector Origin = CapturedPawn->GetActorLocation();
	const FVector Fwd = CapturedPawn->GetActorForwardVector();

	const FVector Desired = Origin + Fwd * ExitForwardOffset + FVector(0, 0, ExitUpOffset);

	// 1) 우선 공간이 비어있으면 사용
	if (OriginalMario.IsValid())
	{
		const UCapsuleComponent* Cap = OriginalMario->GetCapsuleComponent();
		if (Cap)
		{
			const float R = Cap->GetScaledCapsuleRadius();
			const float HH = Cap->GetScaledCapsuleHalfHeight();
			const FCollisionShape Shape = FCollisionShape::MakeCapsule(R, HH);

			const bool bBlocked = GetWorld()->OverlapBlockingTestByChannel(
				Desired, FQuat::Identity, ECC_Pawn, Shape
			);

			if (!bBlocked)
			{
				OutExitLocation = Desired;
				return true;
			}
		}
	}

	// 2) 막히면 NavMesh로 보정(가까운 포인트)
	if (UWorld* World = GetWorld())
	{
		if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
		{
			FNavLocation Projected;
			if (NavSys->ProjectPointToNavigation(Desired, Projected, FVector(ExitNavProjectRadius)))
			{
				OutExitLocation = Projected.Location;
				return true;
			}
		}
	}

	// 3) 최후 fallback
	OutExitLocation = Origin + FVector(0, 0, 200.f);
	return true;
}

void UCaptureComponent::HandleCapturedPawnAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
                                                   AController* InstigatedBy, AActor* DamageCauser)
{
	if (!bIsCapturing) return;
	if (!CapturedPawn.IsValid()) return;
	if (DamagedActor != CapturedPawn.Get()) return;
	if (!OriginalMario.IsValid()) return;

	// 정책: 캡쳐 중 받은 모든 데미지는 마리오 HP로 라우팅 (캡쳐 해제 X)
	const TSubclassOf<UDamageType> DT = DamageType ? DamageType->GetClass() : UDamageType::StaticClass();
	UGameplayStatics::ApplyDamage(OriginalMario.Get(), Damage, InstigatedBy, DamageCauser, DT);

	// 몬스터 피격 리액션(HP는 깎지 않더라도 넉백/스턴/애니 재생은 여기서 시작 가능)
	if (CapturedActor.IsValid() && CapturedActor->GetClass()->ImplementsInterface(UCapturableInterface::StaticClass()))
	{
		ICapturableInterface::Execute_OnCapturedPawnDamaged(CapturedActor.Get(), Damage, InstigatedBy, DamageCauser);
	}
}
