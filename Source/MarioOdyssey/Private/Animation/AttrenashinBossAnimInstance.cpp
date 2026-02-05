#include "Animation/AttrenashinBossAnimInstance.h"
#include "Character/Boss/AttrenashinBoss.h"
#include "GameFramework/Pawn.h"

void UAttrenashinBossAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	APawn* PawnOwner = TryGetPawnOwner();
	Boss = Cast<AAttrenashinBoss>(PawnOwner);
}

void UAttrenashinBossAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!Boss)
	{
		APawn* PawnOwner = TryGetPawnOwner();
		Boss = Cast<AAttrenashinBoss>(PawnOwner);
		if (!Boss) return;
	}

	Phase = Boss->GetPhase();
	bPhase0 = (Phase == EAttrenashinPhase::Phase0);
	bPhase1 = (Phase == EAttrenashinPhase::Phase1);
	bPhase2 = (Phase == EAttrenashinPhase::Phase2);
	bPhase3 = (Phase == EAttrenashinPhase::Phase3);
}
