#pragma once

#include "CoreMinimal.h"
#include "MarioActionTypes.h"

//  USTRUCT로 바꿀 수 있음.
//  C++에서만 관리하는 컨테이너.
struct FMarioState
{
	EMarioAirAction     AirAction = EMarioAirAction::None;
	EMarioGroundAction  GroundAction = EMarioGroundAction::None;
	EGroundPoundPhase   GroundPound = EGroundPoundPhase::None;

	bool bRunHeld = false;
	bool bCrouchHeld = false;

	// 반동 점프 창
	bool bPoundJumpWindowOpen = false;
	bool bPoundJumpConsumed = false;

	// 공중 1회 제한
	bool bGroundPoundUsed = false;

	// 게이트 (지금 코드의 Move 차단 조건을 여기로 모으는 용도)
	bool CanMove() const
	{
		return !(GroundPound == EGroundPoundPhase::Preparing
			  || GroundPound == EGroundPoundPhase::Pounding
			  || GroundPound == EGroundPoundPhase::Stunned);
	}
};
