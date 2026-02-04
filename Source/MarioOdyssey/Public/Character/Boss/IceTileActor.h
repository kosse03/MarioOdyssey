#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IceTileActor.generated.h"

UCLASS()
class MARIOODYSSEY_API AIceTileActor : public AActor
{
	GENERATED_BODY()

public:
	AIceTileActor();

protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root = nullptr;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<class UBoxComponent> Box = nullptr;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<class UStaticMeshComponent> Mesh = nullptr;
};
