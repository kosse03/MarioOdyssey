#include "Character/Boss/IceTileActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"

AIceTileActor::AIceTileActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetupAttachment(Root);
	// 타일 충돌 정책:
	// - 보스/샤드/타일끼리 간섭 제거
	// - Pawn만 Overlap(향후 Hazard 처리용)
	// - Visibility는 라인트레이스 판정용으로 Block 유지
	Box->SetCollisionProfileName(TEXT("Boss_IceTile_Hazard"));
	Box->SetCollisionObjectType(ECC_WorldDynamic);
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Box->SetGenerateOverlapEvents(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Box);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Tags.Add(TEXT("IceTile"));
}
