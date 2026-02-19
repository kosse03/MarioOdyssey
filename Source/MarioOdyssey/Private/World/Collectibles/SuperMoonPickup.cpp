#include "World/Collectibles/SuperMoonPickup.h"

#include "Progress/MarioGameInstance.h"

ASuperMoonPickup::ASuperMoonPickup()
{
	MoonValue = 1;
}

void ASuperMoonPickup::BeginPlay()
{
	Super::BeginPlay();

	// 자동 ID(권장: 에디터에서 MoonId를 직접 넣기)
	if (MoonId.IsNone())
	{
		// 레벨 내 고유 문자열(세션 중 중복 방지용)
		MoonId = FName(*GetPathName());
	}

	// 이미 먹은 문이면 시작하자마자 제거
	if (UMarioGameInstance* GI = GetGameInstance<UMarioGameInstance>())
	{
		if (GI->HasSuperMoon(MoonId))
		{
			Destroy();
		}
	}
}

void ASuperMoonPickup::OnCollected(AActor* Collector)
{
	if (UMarioGameInstance* GI = GetGameInstance<UMarioGameInstance>())
	{
		GI->CollectSuperMoon(MoonId, MoonValue);
	}
}
