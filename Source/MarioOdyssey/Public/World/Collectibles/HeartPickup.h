#pragma once

#include "CoreMinimal.h"
#include "World/Collectibles/MarioCollectibleBase.h"
#include "HeartPickup.generated.h"

UCLASS()
class MARIOODYSSEY_API AHeartPickup : public AMarioCollectibleBase
{
	GENERATED_BODY()

protected:
	virtual void OnCollected(AActor* Collector) override;
};
