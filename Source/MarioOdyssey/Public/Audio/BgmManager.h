#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BgmManager.generated.h"

class UAudioComponent;
class USoundBase;
class ABgmZoneTrigger;

UCLASS()
class MARIOODYSSEY_API ABgmManager : public AActor
{
	GENERATED_BODY()

public:
	ABgmManager();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 기본(레벨) BGM */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BGM")
	USoundBase* DefaultTrack = nullptr;

	/** 기본(레벨) BGM 페이드 시간 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BGM", meta=(ClampMin="0.0"))
	float DefaultFadeSeconds = 1.0f;

	/** BeginPlay에서 DefaultTrack을 자동 재생 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BGM")
	bool bAutoPlayDefaultOnBeginPlay = true;

	/** 폴링 기반 구역 판정 사용(캡쳐 대응). 기본 true 권장 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BGM|Zones")
	bool bUseLocationPolling = true;

	/** 폴링 간격(초). 값이 너무 크면 경계에서 반응이 늦고, 너무 작으면 불필요하게 자주 검사함 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BGM|Zones", meta=(ClampMin="0.02"))
	float PollIntervalSeconds = 0.10f;

	/** 직접 트랙을 전환(구역 로직 무시). 디버그/특수 연출용 */
	UFUNCTION(BlueprintCallable, Category="BGM")
	void RequestTrack(USoundBase* NewTrack, float FadeSeconds = 1.0f);

	/** (내부) 구역 진입 */
	void EnterZone(ABgmZoneTrigger* Zone);

	/** (내부) 구역 이탈 */
	void ExitZone(ABgmZoneTrigger* Zone);

	/** 현재 재생 중인 트랙(없으면 nullptr) */
	UFUNCTION(BlueprintPure, Category="BGM")
	USoundBase* GetCurrentTrack() const { return CurrentTrack; }


	/** true면 BGM을 재생/전환하지 않는다(컷신/시작화면 등에서 사용). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BGM")
	bool bSuppressed = false;

	/** BGM 음소거/재생 재개. Suppress 해제 시 현재 구역 기준 트랙을 다시 선정해서 재생한다. */
	UFUNCTION(BlueprintCallable, Category="BGM")
	void SetSuppressed(bool bInSuppressed, float FadeSeconds = 0.25f);


protected:
	UPROPERTY(VisibleAnywhere, Category="BGM|Components")
	USceneComponent* Root = nullptr;

	UPROPERTY(VisibleAnywhere, Category="BGM|Components")
	UAudioComponent* AudioA = nullptr;

	UPROPERTY(VisibleAnywhere, Category="BGM|Components")
	UAudioComponent* AudioB = nullptr;

private:
	USoundBase* CurrentTrack = nullptr;
	bool bUsingA = true;

	// 구역 스택(겹침 대비). Priority 큰 게 우선. 같으면 "마지막으로 들어온" 구역 우선.
	struct FZoneEntry
	{
		TWeakObjectPtr<ABgmZoneTrigger> Zone;
		int32 Priority = 0;
		int64 EnterOrder = 0;
	};

	TArray<FZoneEntry> ActiveZones;
	int64 EnterOrderCounter = 0;

	// 폴링용 전체 구역 + 현재 내부 구역(중복 진입/이탈 방지)
	TArray<TWeakObjectPtr<ABgmZoneTrigger>> AllZones;
	TSet<TWeakObjectPtr<ABgmZoneTrigger>> InsideZones;

	FTimerHandle PollTimer;

	void UpdateDesiredTrack();
	ABgmZoneTrigger* PickBestZone(int32& OutPriority, float& OutFadeSeconds) const;

	void CrossFadeTo(USoundBase* NewTrack, float FadeSeconds);

	void PruneInvalidZones();

	void CacheZonesOnce();
	void StartPollingIfEnabled();
	void StopPolling();

	void PollZonesByLocation();

	FVector GetListenerLocation() const;
};
