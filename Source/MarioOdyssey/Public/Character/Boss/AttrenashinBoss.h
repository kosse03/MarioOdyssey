#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "Character/Boss/AttrenashinTypes.h"
#include "AttrenashinBoss.generated.h"

class UPrimitiveComponent;

UCLASS()
class MARIOODYSSEY_API AAttrenashinBoss : public AActor
{
	GENERATED_BODY()

public:
	AAttrenashinBoss();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category="Boss|Anim")
	EAttrenashinPhase GetPhase() const { return Phase; }

	UFUNCTION(BlueprintPure, Category="Boss|Anim")
	bool IsInFear() const { return bIsInFear; }

	UFUNCTION(BlueprintPure, Category="Boss|Anim")
	int32 GetHeadHitCount() const { return HeadHitCount; }

	UFUNCTION(BlueprintPure, Category="Boss|Anim")
	class AAttrenashinFist* GetLeftFist() const { return LeftFist.Get(); }

	UFUNCTION(BlueprintPure, Category="Boss|Anim")
	class AAttrenashinFist* GetRightFist() const { return RightFist.Get(); }

	UFUNCTION(BlueprintPure, Category="Boss|Anim")
	bool IsPhase2FistSpinWindow() const;

	UFUNCTION(BlueprintPure, Category="Boss|Phase3")
	bool IsPhase3ClapWindow() const;

	UFUNCTION(BlueprintCallable, Category="Boss|IceShard")
	void PerformGroundSlam_IceShardRain();

	UFUNCTION(BlueprintCallable, Category="Boss|IceShard")
	void StartIceRainByFist(bool bUseLeftFist);

	UFUNCTION(BlueprintCallable, Category="Boss|IceShard")
	void StartIceRainByBothFists();

	// Fist 콜백(캡쳐 시작/해제)
	void NotifyFistCaptured(class AAttrenashinFist* CapturedFist);
	void NotifyFistReleased(class AAttrenashinFist* ReleasedFist);

	// 카운터 샤드가 캡쳐된 주먹에 적중했을 때
	void NotifyBarrageShardHitCapturedFist(class AAttrenashinFist* HitFist, const FVector& ShardVelocity);

protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root = nullptr;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> FistAnchorL = nullptr;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> FistAnchorR = nullptr;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<class USphereComponent> HeadHitSphere = nullptr;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Spawn")
	TSubclassOf<class AAttrenashinFist> FistClass;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase1")
	float SlamAttemptInterval = 6.0f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase1", meta=(ClampMin="0.1"))
	float Phase1CaptureWindowSeconds = 8.0f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase1", meta=(ClampMin="0.0"))
	float Phase1PostFailCooldownSeconds = 0.8f;

	// 캡쳐 후 보스를 때리기 전에 캡쳐가 풀렸을 때(어떠한 이유로든)
	// 일정 시간 뒤 보스/손을 맵 중앙으로 복귀시키는 딜레이
	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase1", meta=(ClampMin="0.0"))
	float Phase1CaptureFailReturnDelaySeconds = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase1|Retreat", meta=(ClampMin="0.0"))
	float Phase1RetreatDistance = 2000.f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase1|Retreat", meta=(ClampMin="0.01"))
	float Phase1RetreatSeconds = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Head")
	bool bRequireCapturedFistForHeadHit = true;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Head")
	float HeadContactDamage = 1.f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Head")
	float HeadContactDamageCooldown = 0.4f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|IceShard")
	TSubclassOf<class AIceShardActor> IceShardClass;

	UPROPERTY(EditDefaultsOnly, Category="Boss|IceShard")
	TSubclassOf<class AIceTileActor> IceTileClass;

	UPROPERTY(EditDefaultsOnly, Category="Boss|IceShard", meta=(ClampMin="1"))
	int32 IceShardCount = 12;

	UPROPERTY(EditDefaultsOnly, Category="Boss|IceShard", meta=(ClampMin="0.0"))
	float IceShardRadius = 1000.f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|IceShard")
	float IceShardSpawnHeight = 1800.f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|IceShard")
	float IceShardSpawnHeightJitter = 220.f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|IceShard")
	float IceShardDamage = 1.f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|IceShard")
	bool bIceShardCenterOnPlayer = true;

	UPROPERTY(EditDefaultsOnly, Category="Boss|IceShard")
	float IceTileSpawnZOffset = 0.f;

	// 캡쳐 중 반대손 카운터 샤드
	UPROPERTY(EditDefaultsOnly, Category="Boss|CaptureBarrage", meta=(ClampMin="0.1"))
	float CaptureBarrageIntervalSeconds = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|CaptureBarrage", meta=(ClampMin="1.0"))
	float CaptureBarrageShardSpeed = 2600.f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|CaptureBarrage", meta=(ClampMin="1"))
	int32 CaptureBarrageHitsToForceRelease = 3;

	UPROPERTY(EditDefaultsOnly, Category="Boss|CaptureBarrage")
	float CaptureBarrageSpawnForwardOffset = 0.f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|CaptureBarrage", meta=(ClampMin="0.0"))
	float CaptureBarrageKnockbackStrength = 320.f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|CaptureBarrage", meta=(ClampMin="0.0"))
	float CaptureBarrageKnockbackUp = 90.f;

	// =========================
	// Phase2 연출/패턴
	// =========================
	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase2|Head", meta=(ClampMin="0.0"))
	float Phase2HeadLaunchBackSpeed = 2300.f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase2|Head", meta=(ClampMin="0.0"))
	float Phase2HeadLaunchUpSpeed = 1850.f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase2|Head", meta=(ClampMin="0.0"))
	float Phase2HeadGravity = 4200.f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase2|Head", meta=(ClampMin="0.0"))
	float Phase2HeadLaunchMinSeconds = 0.28f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase2|Head", meta=(ClampMin="0.0"))
	float Phase2HeadFallDepthForReturn = 320.f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase2|Head", meta=(ClampMin="0.01"))
	float Phase2HeadReturnSeconds = 1.1f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase2|SpinDash", meta=(ClampMin="0.1"))
	float Phase2FistSpinSeconds = 3.0f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase2|SpinDash", meta=(ClampMin="0.0"))
	float Phase2FistSpinYawDegPerSec = 720.f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase2|SpinDash", meta=(ClampMin="0.01"))
	float Phase2FistDashSeconds = 0.70f;

	// Phase2 전방 발사 속도(직접 튜닝용).
	// 0보다 크면 이 값을 우선 사용하고,
	// 0 이하일 때만 Phase2FistDashSeconds 기반 보간으로 동작.
	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase2|SpinDash", meta=(ClampMin="0.0"))
	float Phase2FistDashSpeed = 9000.f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase2|Flow", meta=(ClampMin="0.01"))
	float Phase2ReturnToCenterSeconds = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase2|Flow", meta=(ClampMin="0.0"))
	float Phase2RestAfterCenterSeconds = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase2|Flow", meta=(ClampMin="0.1"))
	float Phase2AlternatingSlamInterval = 3.2f;

	// ===== Phase3 (Phase2 확장 패턴) =====
	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase3")
	bool bPromoteToPhase3OnHeadHitDuringPhase2 = true;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase3", meta=(ClampMin="1", UIMin="1"))
	int32 Phase3ClapCount = 3;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase3", meta=(ClampMin="0.05"))
	float Phase3ClapInterval = 0.50f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase3", meta=(ClampMin="0.05"))
	float Phase3AlternatingSlamInterval = 2.60f;

	// Phase2 -> Phase3 승급 시, 소켓 스냅처럼 보이지 않도록 비캡쳐 손 복귀 시간을 분리
	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase3", meta=(ClampMin="0.01"))
	float Phase3TransitionFistReturnSeconds = 0.90f;

	// Phase3 Spin 진입 직전 손을 앵커로 부드럽게 맞추는 시간
	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase3", meta=(ClampMin="0.01"))
	float Phase3SpinPrepReturnSeconds = 0.90f;

	// 박수 패턴 세부 튜닝
	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase3", meta=(ClampMin="0.0"))
	float Phase3ClapPreDelaySeconds = 1.50f;

	// Phase2->Phase3 전환 후 중앙 복귀 다음 휴식 시간
	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase3", meta=(ClampMin="0.0"))
	float Phase3RestAfterCenterSeconds = 3.0f;

	// 마리오 좌/우 트래킹 파라미터
	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase3", meta=(ClampMin="0.0"))
	float Phase3TrackSideOffset = 520.f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase3")
	float Phase3TrackHeightOffset = 0.f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase3", meta=(ClampMin="0.1"))
	float Phase3TrackMoveSpeed = 2600.f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase3", meta=(ClampMin="0.01"))
	float Phase3FirstTrackSeconds = 5.0f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase3", meta=(ClampMin="0.01"))
	float Phase3NextTrackSeconds = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase3", meta=(ClampMin="0.01"))
	float Phase3TrackHoldSeconds = 0.8f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase3", meta=(ClampMin="0.01"))
	float Phase3ClapCloseSeconds = 0.12f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase3", meta=(ClampMin="0.01"))
	float Phase3ClapOpenSeconds = 0.18f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Phase3", meta=(ClampMin="0.0"))
	float Phase3ClapContactHalfDistance = 35.f;


private:
	EAttrenashinPhase Phase = EAttrenashinPhase::Phase1;

	enum class EPhase2FlowState : uint8
	{
		None,
		HeadLaunch,
		HeadReturn,
		Spin,
		Dash,
		Clap,
		ReturnToCenter,
		Rest,
		ShardRain,
		AlternatingSlamLoop,
	};

	enum class EPhase3ClapSubState : uint8
	{
		Tracking,
		Hold,
		Close,
		Open,
	};

	enum class EPhase1CaptureResult : uint8
	{
		None,
		Success,
		Fail
	};

	TWeakObjectPtr<class AAttrenashinFist> LeftFist;
	TWeakObjectPtr<class AAttrenashinFist> RightFist;
	EPhase2FlowState Phase2State = EPhase2FlowState::None;
	bool bPhase2FlowActive = false;
	bool bPhase2ShardRainStarted = false;
	bool bPhase3FlowMode = false;
	float Phase2StateElapsed = 0.f;
	float Phase3ClapElapsed = 0.f;
	int32 Phase3ClapsDone = 0;
	int32 Phase3TrackIndex = 0;
	EPhase3ClapSubState Phase3ClapSubState = EPhase3ClapSubState::Tracking;
	float Phase2AlternatingSlamTimer = 0.f;

	FVector Phase3ClapOpenL = FVector::ZeroVector;
	FVector Phase3ClapOpenR = FVector::ZeroVector;
	FVector Phase3ClapCloseL = FVector::ZeroVector;
	FVector Phase3ClapCloseR = FVector::ZeroVector;
	float Phase3ClapStepElapsed = 0.f;
	bool bPhase3ClapDelayActive = false;
	bool bPhase3ClapClosing = true;

	FVector Phase2RetreatBossLocation = FVector::ZeroVector;
	FVector Phase2CenterBossLocation = FVector::ZeroVector;
	FVector Phase2ReturnBossStartLocation = FVector::ZeroVector;

	FVector Phase2HeadRetreatWorldLocation = FVector::ZeroVector;
	FVector Phase2HeadVelocity = FVector::ZeroVector;
	FVector Phase2HeadReturnStartWorldLocation = FVector::ZeroVector;

	FVector Phase2DashStartL = FVector::ZeroVector;
	FVector Phase2DashStartR = FVector::ZeroVector;
	FVector Phase2DashTargetL = FVector::ZeroVector;
	FVector Phase2DashTargetR = FVector::ZeroVector;

	float SlamAttemptTimer = 0.f;
	bool bNextLeft = true;
	int32 HeadHitCount = 0;
	float LastHeadContactDamageTime = -1000.f;

	// 공포 후퇴/시선 처리
	UPROPERTY(EditDefaultsOnly, Category="Boss|Retreat", meta=(ClampMin="0.0"))
	float RetreatDistance = 2200.f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Retreat", meta=(ClampMin="0.0"))
	float RetreatSpeed = 2600.f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|Retreat", meta=(ClampMin="0.0"))
	float RetreatWorldRadiusLimit = 7300.f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|LookAt", meta=(ClampMin="0.0"))
	float FacePlayerInterpSpeed = 6.0f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|LookAt", meta=(ClampMin="0.0"))
	float MarioReturnToOriginBoostXY = 2100.f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|LookAt", meta=(ClampMin="0.0"))
	float MarioReturnToOriginBoostZ = 380.f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|IceShard", meta=(ClampMin="0.0"))
	float IceRainWorldRadius = 4200.f;

	UPROPERTY(EditDefaultsOnly, Category="Boss|IceShard")
	float IceRainWorldCenterZ = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Boss|State", meta=(AllowPrivateAccess="true"))
	bool bIsInFear = false;

	FTimerHandle Phase1CaptureWindowTimerHandle;
	bool bPhase1CaptureWindowActive = false;
	EPhase1CaptureResult Phase1CaptureResult = EPhase1CaptureResult::None;
	float Phase1FailCooldownRemain = 0.f;

	// Phase1 캡쳐 실패(캡쳐 해제) 후 중앙 복귀 처리
	FTimerHandle Phase1CaptureFailReturnTimerHandle;
	bool bPhase1ReturnToCenterActive = false;
	float Phase1ReturnToCenterElapsed = 0.f;
	FVector Phase1ReturnToCenterStart = FVector::ZeroVector;
	FVector Phase1ReturnToCenterTarget = FVector::ZeroVector;

	bool bRetreating = false;
	FVector RetreatStart = FVector::ZeroVector;
	FVector RetreatTarget = FVector::ZeroVector;
	float RetreatT = 0.f;
	bool bWorldDynamicIgnoredForRetreat = false;

	TMap<TObjectPtr<UPrimitiveComponent>, TEnumAsByte<ECollisionResponse>> CachedWorldDynamicResponses;
	TMap<TObjectPtr<UPrimitiveComponent>, TEnumAsByte<ECollisionResponse>> CachedWorldStaticResponses;

	// 캡쳐 카운터 샤드 상태
	FTimerHandle CaptureBarrageTimerHandle;
	TWeakObjectPtr<class AAttrenashinFist> BarrageCapturedFist;
	TWeakObjectPtr<class AAttrenashinFist> BarrageThrowingFist;
	int32 BarrageHitCount = 0;
	bool bCaptureBarrageActive = false;

	UFUNCTION()
	void OnHeadBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	                        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	                        bool bFromSweep, const FHitResult& SweepResult);

	AActor* GetPlayerTarget() const;
	void TryStartPhase1Slam();
	void SpawnIceShardsAt(const FVector& Center);

	void EnterPhase(EAttrenashinPhase NewPhase);
	void BeginPhase1CaptureWindow(class AAttrenashinFist* CapturedFist);
	void EndPhase1CaptureWindow(bool bSuccess);
	void OnPhase1CaptureWindowTimeout();
	void CancelPhase1CaptureFailReturnToCenter();
	void BeginPhase1CaptureFailReturnToCenter();
	void UpdatePhase1CaptureFailReturnToCenter(float DeltaSeconds);
	void StartRetreatMotion();
	void UpdateRetreat(float DeltaSeconds);

	void StartCaptureBarrage(class AAttrenashinFist* CapturedFist);
	void StopCaptureBarrage();
	void HandleCaptureBarrageTick();

	void BeginPhase2FromHeadHit(class AAttrenashinFist* CapturedFist);
	void BeginPhase3FromHeadHit(class AAttrenashinFist* CapturedFist);
	void BeginAdvancedPhaseFromHeadHit(EAttrenashinPhase TargetPhase, class AAttrenashinFist* CapturedFist, bool bUsePhase3Rules);
	void EnterPhase2State(EPhase2FlowState NewState);
	void UpdatePhase2(float DeltaSeconds);
	bool AreBothFistsIdle() const;
	void ForceFistsReturnToAnchor(float ReturnSeconds);
	void StartPhase2Dash();
	FVector BuildPhase2OppositeBossLocation() const;
	void CachePhase3ClapTargets();
	void UpdatePhase3Clap(float DeltaSeconds);

	bool ShouldFacePlayerDuringPunch() const;
	void FacePlayerYawOnly(float DeltaSeconds);
	void MoveNonCapturedFistWithBoss(const FVector& DeltaMove);
	void BoostMarioTowardWorldOrigin();
	void SetWorldDynamicIgnoredForRetreat(bool bIgnore);

	FVector GetAnchorLocationBySide(EFistSide Side) const;
};
