#include "World/Collectibles/HeartPickup.h"

#include "MarioOdyssey/MarioCharacter.h"

void AHeartPickup::OnCollected(AActor* Collector)
{
	if (AMarioCharacter* Mario = Cast<AMarioCharacter>(Collector))
	{
		Mario->HealToFull();
	}
}
