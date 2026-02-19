#include "World/Collectibles/CoinPickup.h"

#include "Progress/MarioGameInstance.h"

ACoinPickup::ACoinPickup()
{
	CoinValue = 1;
}

void ACoinPickup::OnCollected(AActor* Collector)
{
	if (UMarioGameInstance* GI = GetGameInstance<UMarioGameInstance>())
	{
		GI->AddCoin(CoinValue);
	}
}
