#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BulletBillLauncher.generated.h"

class UArrowComponent;
class ABulletBillCharacter;

UCLASS()
class MARIOODYSSEY_API ABulletBillLauncher : public AActor
{
	GENERATED_BODY()

public:
	ABulletBillLauncher();

	virtual void BeginPlay() override;

	/** Start spawning (called automatically if bAutoStart=true). */
	UFUNCTION(BlueprintCallable, Category="BulletBillLauncher")
	void StartSpawning();

	/** Stop spawning. */
	UFUNCTION(BlueprintCallable, Category="BulletBillLauncher")
	void StopSpawning();

	/** Spawn one BulletBill immediately. */
	UFUNCTION(BlueprintCallable, Category="BulletBillLauncher")
	ABulletBillCharacter* SpawnOne();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UArrowComponent* Arrow;

	/** Bullet class to spawn (BP_BulletBillCharacter etc). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawner")
	TSubclassOf<ABulletBillCharacter> BulletBillClass;

	/** Seconds between shots. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawner", meta=(ClampMin="0.1"))
	float SpawnInterval = 8.0f;

	/** Forward offset from launcher to spawn location. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawner")
	float SpawnOffset = 120.f;

	/** Up offset for spawn location (optional). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawner")
	float SpawnUpOffset = 0.f;

	/** Collision disabled time right after spawn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawner", meta=(ClampMin="0.0"))
	float SpawnGraceSeconds = 0.15f;

	/** Ignore collision with the launcher permanently (prevents back-hit). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawner")
	bool bIgnoreLauncherCollision = true;

	/** Auto start on BeginPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawner")
	bool bAutoStart = true;

private:
	FTimerHandle SpawnTimer;
	void HandleSpawnTick();
};
