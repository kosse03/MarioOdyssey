#include "Animation/AttrenashinBossAnimInstance.h"
#include "Character/Boss/AttrenashinBoss.h"
#include "GameFramework/Actor.h"

void UAttrenashinBossAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	Boss = Cast<AAttrenashinBoss>(GetOwningActor());
}

void UAttrenashinBossAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!Boss)
	{
		Boss = Cast<AAttrenashinBoss>(GetOwningActor());
		if (!Boss) return;
	}

	Phase = Boss->GetPhase();
	bPhase0 = (Phase == EAttrenashinPhase::Phase0);
	bPhase1 = (Phase == EAttrenashinPhase::Phase1);
	bPhase2 = (Phase == EAttrenashinPhase::Phase2);
	bPhase3 = (Phase == EAttrenashinPhase::Phase3);
}
