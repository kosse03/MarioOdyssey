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
	// 타일은 지형처럼 막히는 충돌
	Box->SetCollisionProfileName(TEXT("BlockAll"));
	Box->SetCollisionObjectType(ECC_WorldStatic);
	Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Box->SetGenerateOverlapEvents(false);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Box);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Tags.Add(TEXT("IceTile"));
}
