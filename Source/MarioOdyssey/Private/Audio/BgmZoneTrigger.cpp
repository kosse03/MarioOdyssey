#include "Audio/BgmZoneTrigger.h"

#include "Components/BoxComponent.h"

ABgmZoneTrigger::ABgmZoneTrigger()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetupAttachment(Root);

	// 기본적으로는 "영역 데이터"로만 쓰므로, 충돌/오버랩은 사용자가 필요하면 BP에서 켜도 됨.
	Trigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Trigger->SetGenerateOverlapEvents(false);

	// 적당한 기본 크기
	Trigger->SetBoxExtent(FVector(400.f, 400.f, 200.f));
}

void ABgmZoneTrigger::BeginPlay()
{
	Super::BeginPlay();
}

FVector ABgmZoneTrigger::GetUnscaledExtent() const
{
	if (!Trigger) return FVector::ZeroVector;
	return Trigger->GetUnscaledBoxExtent();
}

bool ABgmZoneTrigger::IsLocationInside(const FVector& WorldLocation) const
{
	if (!Trigger) return false;

	// 박스 로컬 공간으로 변환
	const FTransform T = Trigger->GetComponentTransform();
	const FVector Local = T.InverseTransformPosition(WorldLocation);

	const FVector Ext = GetUnscaledExtent();

	return FMath::Abs(Local.X) <= Ext.X
		&& FMath::Abs(Local.Y) <= Ext.Y
		&& FMath::Abs(Local.Z) <= Ext.Z;
}
