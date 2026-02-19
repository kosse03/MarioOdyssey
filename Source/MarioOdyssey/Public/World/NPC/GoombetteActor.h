#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GoombetteActor.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPrimitiveComponent;
class ASuperMoonPickup;

/**
 * 단순 Goombette:
 * - 플레이어가 굼바(캡쳐 상태)로 조종 중이고,
 * - 굼바 스택 높이가 RequiredStackCount 이상이면,
 *   Goombette는 사라지고(Destroy), 그 자리에서 SuperMoon을 스폰한다.
 *
 * 연출/애니메이션 없음.
 */
UCLASS()
class MARIOODYSSEY_API AGoombetteActor : public AActor
{
	GENERATED_BODY()

public:
	AGoombetteActor();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category="Goombette")
	TObjectPtr<USceneComponent> Root = nullptr;

	UPROPERTY(VisibleAnywhere, Category="Goombette")
	TObjectPtr<UStaticMeshComponent> Mesh = nullptr;

	UPROPERTY(VisibleAnywhere, Category="Goombette")
	TObjectPtr<USphereComponent> Trigger = nullptr;

	// 요구 스택 높이(본인 포함)
	UPROPERTY(EditAnywhere, Category="Goombette", meta=(ClampMin="1", ClampMax="20"))
	int32 RequiredStackCount = 4;

	// 보상: 슈퍼문 클래스
	UPROPERTY(EditAnywhere, Category="Goombette")
	TSubclassOf<ASuperMoonPickup> RewardMoonClass;

	// 스폰 위치 오프셋(기본: 살짝 위)
	UPROPERTY(EditAnywhere, Category="Goombette")
	FVector RewardSpawnOffset = FVector(0.f, 0.f, 80.f);

	UPROPERTY(VisibleAnywhere, Category="Goombette")
	bool bConsumed = false;

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	                           UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	                           bool bFromSweep, const FHitResult& SweepResult);
};
