#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Character/Boss/AttrenashinTypes.h"
#include "AttrenashinBossAnimInstance.generated.h"

UCLASS()
class MARIOODYSSEY_API UAttrenashinBossAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	UPROPERTY(BlueprintReadOnly, Category="Boss|Ref")
	TObjectPtr<class AAttrenashinBoss> Boss = nullptr;

	UPROPERTY(BlueprintReadOnly, Category="Boss|State")
	EAttrenashinPhase Phase = EAttrenashinPhase::Phase1;

	UPROPERTY(BlueprintReadOnly, Category="Boss|State")
	bool bPhase0 = false;

	UPROPERTY(BlueprintReadOnly, Category="Boss|State")
	bool bPhase1 = true;

	UPROPERTY(BlueprintReadOnly, Category="Boss|State")
	bool bPhase2 = false;

	UPROPERTY(BlueprintReadOnly, Category="Boss|State")
	bool bPhase3 = false;
};
