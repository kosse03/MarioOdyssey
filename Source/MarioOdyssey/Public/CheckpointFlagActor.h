#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CheckpointFlagActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class USphereComponent;
class AMarioCharacter;

UCLASS(Blueprintable)
class MARIOODYSSEY_API ACheckpointFlagActor : public AActor
{
	GENERATED_BODY()

public:
	ACheckpointFlagActor();

	UFUNCTION(BlueprintPure, Category="Checkpoint")
	bool IsActivated() const { return bActivated; }

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnActivateOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnActivateHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

	bool TryActivateFromActor(AActor* SourceActor);
	AMarioCharacter* ResolveMarioFromActor(AActor* SourceActor) const;

	UFUNCTION(BlueprintNativeEvent, Category="Checkpoint")
	void OnCheckpointActivated(AMarioCharacter* Mario);
	virtual void OnCheckpointActivated_Implementation(AMarioCharacter* Mario);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Checkpoint")
	USceneComponent* Root = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Checkpoint")
	UStaticMeshComponent* FlagMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Checkpoint")
	USphereComponent* ActivateSphere = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Checkpoint")
	USceneComponent* RespawnPoint = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Checkpoint")
	float ActivateRadius = 140.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Checkpoint")
	bool bActivateOnlyOnce = true;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Checkpoint")
	bool bActivated = false;
};
