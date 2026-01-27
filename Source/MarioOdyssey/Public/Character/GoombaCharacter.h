// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Capture/CapturableInterface.h"
#include "InputAction.h"
#include "GoombaCharacter.generated.h"


struct FInputActionValue;
class AMarioCharacter;

UCLASS()
class MARIOODYSSEY_API AGoombaCharacter : public ACharacter, public ICapturableInterface
{
	GENERATED_BODY()

public:
	AGoombaCharacter();

protected:
	virtual void BeginPlay() override;
	
	// C(IA_Crouch)를 여기서 "캡쳐 해제"로 사용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input")
	UInputAction* IA_Crouch = nullptr;

	void OnReleaseCapturePressed(const FInputActionValue& Value);

	// ICapturableInterface
	virtual bool CanBeCaptured_Implementation(const FCaptureContext& Context) const override { return true; }
	virtual APawn* GetCapturePawn_Implementation() const override { return const_cast<AGoombaCharacter*>(this); }
	virtual void OnCaptured_Implementation(AController* Capturer, const FCaptureContext& Context) override;
	virtual void OnReleased_Implementation(const FCaptureReleaseContext& Context) override {}
	virtual void OnCapturedPawnDamaged_Implementation(float Damage, AController* InstigatorController, AActor* DamageCauser) override {}
public:	
	virtual void Tick(float DeltaTime) override;
	
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
private:
	TWeakObjectPtr<AMarioCharacter> CapturingMario;
};
