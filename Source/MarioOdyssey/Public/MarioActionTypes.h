#pragma once

#include "CoreMinimal.h"
#include "MarioActionTypes.generated.h"

// 공중 액션
UENUM(BlueprintType)
enum class EMarioAirAction : uint8
{
	None       UMETA(DisplayName="None"),
	LongJump   UMETA(DisplayName="LongJump"),
	Backflip   UMETA(DisplayName="Backflip"),
	PoundJump  UMETA(DisplayName="PoundJump"),
	Dive       UMETA(DisplayName="Dive"),    // 나중에 구현해도 enum은 먼저 둬도 됨
};

// 지상 액션
UENUM(BlueprintType)
enum class EMarioGroundAction : uint8
{
	None  UMETA(DisplayName="None"),
	Roll  UMETA(DisplayName="Roll"),         // 나중에 구현
};
//구르기
UENUM(BlueprintType)
enum class ERollPhase : uint8
{
	None  UMETA(DisplayName="None"),
	Start UMETA(DisplayName="Start"),
	Loop  UMETA(DisplayName="Loop"),
	End   UMETA(DisplayName="End"),
};
// 그라운드 파운드 진행 단계
UENUM(BlueprintType)
enum class EGroundPoundPhase : uint8
{
	None       UMETA(DisplayName="None"),
	Preparing  UMETA(DisplayName="Preparing"),
	Pounding   UMETA(DisplayName="Pounding"),
	Stunned    UMETA(DisplayName="Stunned"),
};

//wall action Enum
UENUM(BlueprintType)
enum class EWallActionState : uint8
{
	None       UMETA(DisplayName="None"),
	SlideStart UMETA(DisplayName="SlideStart"),
	SlideLoop  UMETA(DisplayName="SlideLoop"),
	WallKick   UMETA(DisplayName="WallKick"),
};

//ledge action Enum
UENUM(BlueprintType)
enum class ELedgeState : uint8
{
	None      UMETA(DisplayName="None"),
	HangStart UMETA(DisplayName="HangStart"),
	HangLoop  UMETA(DisplayName="HangLoop"),
	MoveLeft  UMETA(DisplayName="MoveLeft"),
	MoveRight UMETA(DisplayName="MoveRight"),
	ClimbEnd  UMETA(DisplayName="ClimbEnd"),
};