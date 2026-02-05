#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Character/Boss/AttrenashinTypes.h"
#include "Character/Boss/AttrenashinFist.h"
#include "AttrenashinFistAnimInstance.generated.h"

UCLASS()
class MARIOODYSSEY_API UAttrenashinFistAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	UPROPERTY(BlueprintReadOnly, Category="Fist|Ref")
	TObjectPtr<AAttrenashinFist> Fist = nullptr;

	UPROPERTY(BlueprintReadOnly, Category="Fist|State")
	EFistState FistState = EFistState::Idle;

	UPROPERTY(BlueprintReadOnly, Category="Fist|State")
	EAttrenashinSlamSeq SlamSeq = EAttrenashinSlamSeq::None;

	UPROPERTY(BlueprintReadOnly, Category="Fist|State")
	float SlamSeqTime = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Fist|State")
	bool bIsIdle = true;

	UPROPERTY(BlueprintReadOnly, Category="Fist|State")
	bool bIsSlamSequence = false;

	UPROPERTY(BlueprintReadOnly, Category="Fist|State")
	bool bIsIceRainSlam = false;

	UPROPERTY(BlueprintReadOnly, Category="Fist|State")
	bool bIsStunned = false;

	UPROPERTY(BlueprintReadOnly, Category="Fist|State")
	bool bIsCapturedDrive = false;

	UPROPERTY(BlueprintReadOnly, Category="Fist|State")
	bool bIsReturning = false;
};
