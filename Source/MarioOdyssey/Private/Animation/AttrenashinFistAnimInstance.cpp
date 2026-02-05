#include "Animation/AttrenashinFistAnimInstance.h"
#include "GameFramework/Pawn.h"

void UAttrenashinFistAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	APawn* PawnOwner = TryGetPawnOwner();
	Fist = Cast<AAttrenashinFist>(PawnOwner);
}

void UAttrenashinFistAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!Fist)
	{
		APawn* PawnOwner = TryGetPawnOwner();
		Fist = Cast<AAttrenashinFist>(PawnOwner);
		if (!Fist) return;
	}

	FistState = Fist->GetFistState();
	SlamSeq = Fist->GetSlamSeq();
	SlamSeqTime = Fist->GetSlamSeqTime();

	bIsIdle = (FistState == EFistState::Idle);
	bIsSlamSequence = (FistState == EFistState::SlamSequence);
	bIsIceRainSlam = (FistState == EFistState::IceRainSlam);
	bIsStunned = (FistState == EFistState::Stunned);
	bIsCapturedDrive = (FistState == EFistState::CapturedDrive);
	bIsReturning = (FistState == EFistState::ReturnToAnchor);
}
