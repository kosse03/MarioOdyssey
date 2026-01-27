#pragma once

#include "CoreMinimal.h"
#include "CaptureTypes.generated.h"

UENUM(BlueprintType)
enum class ECaptureReleaseReason : uint8
{
	Manual      UMETA(DisplayName="Manual"),
	GameOver    UMETA(DisplayName="GameOver"),
	InvalidPawn UMETA(DisplayName="InvalidPawn")
};

USTRUCT(BlueprintType)
struct FCaptureContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category="Capture")
	TObjectPtr<AActor> SourceActor = nullptr; // 모자

	UPROPERTY(BlueprintReadWrite, Category="Capture")
	TObjectPtr<AController> InstigatorController = nullptr;

	UPROPERTY(BlueprintReadWrite, Category="Capture")
	FVector HitLocation = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct FCaptureReleaseContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category="Capture")
	ECaptureReleaseReason Reason = ECaptureReleaseReason::Manual;

	UPROPERTY(BlueprintReadWrite, Category="Capture")
	TObjectPtr<AActor> ReleasedBy = nullptr; // 마리오(컴포넌트 소유자)

	UPROPERTY(BlueprintReadWrite, Category="Capture")
	FVector ExitLocation = FVector::ZeroVector;
};
