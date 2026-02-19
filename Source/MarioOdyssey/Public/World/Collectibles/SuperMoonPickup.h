#pragma once

#include "CoreMinimal.h"
#include "World/Collectibles/MarioCollectibleBase.h"
#include "SuperMoonPickup.generated.h"

UCLASS()
class MARIOODYSSEY_API ASuperMoonPickup : public AMarioCollectibleBase
{
	GENERATED_BODY()

public:
	ASuperMoonPickup();

protected:
	virtual void BeginPlay() override;
	virtual void OnCollected(AActor* Collector) override;
	
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="SuperMoon")
	FName MoonId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="SuperMoon")
	int32 MoonValue = 1;
};
