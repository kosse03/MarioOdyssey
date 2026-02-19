#pragma once

#include "CoreMinimal.h"
#include "World/Collectibles/MarioCollectibleBase.h"
#include "CoinPickup.generated.h"

UCLASS()
class MARIOODYSSEY_API ACoinPickup : public AMarioCollectibleBase
{
	GENERATED_BODY()

public:
	ACoinPickup();

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Coin")
	int32 CoinValue = 1;

	virtual void OnCollected(AActor* Collector) override;
};
