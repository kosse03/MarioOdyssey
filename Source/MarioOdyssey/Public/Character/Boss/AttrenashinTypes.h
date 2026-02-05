#pragma once

#include "CoreMinimal.h"
#include "AttrenashinTypes.generated.h"

UENUM(BlueprintType)
enum class EAttrenashinPhase : uint8
{
	Phase0 UMETA(DisplayName="Phase0"),
	Phase1 UMETA(DisplayName="Phase1"),
	Phase2 UMETA(DisplayName="Phase2"),
	Phase3 UMETA(DisplayName="Phase3"),
};

UENUM(BlueprintType)
enum class EFistSide : uint8
{
	Left  UMETA(DisplayName="Left"),
	Right UMETA(DisplayName="Right"),
};

UENUM(BlueprintType)
enum class EFistState : uint8
{
	Idle           UMETA(DisplayName="Idle"),
	SlamSequence   UMETA(DisplayName="SlamSequence"),
	IceRainSlam    UMETA(DisplayName="IceRainSlam"),
	Stunned        UMETA(DisplayName="Stunned"),
	CapturedDrive  UMETA(DisplayName="CapturedDrive"),
	ReturnToAnchor UMETA(DisplayName="ReturnToAnchor"),
};

UENUM(BlueprintType)
enum class EAttrenashinSlamSeq : uint8
{
	None           UMETA(DisplayName="None"),
	MoveAbove      UMETA(DisplayName="MoveAbove"),
	FollowAbove    UMETA(DisplayName="FollowAbove"),
	PreSlamPause   UMETA(DisplayName="PreSlamPause"),
	SlamDown       UMETA(DisplayName="SlamDown"),
	PostImpactWait UMETA(DisplayName="PostImpactWait")
};
