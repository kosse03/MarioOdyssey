// Copyright Epic Games, Inc. All Rights Reserved.

#include "MarioOdysseyGameMode.h"
#include "MarioCharacter.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Engine.h"

AMarioOdysseyGameMode::AMarioOdysseyGameMode()
{
	// BP_Mario를 기본 Pawn으로 사용
	static ConstructorHelpers::FClassFinder<APawn> MarioPawnBPClass(
		TEXT("/Game/_BP/Character/Mario/BP_Mario")
	);

	if (MarioPawnBPClass.Succeeded() && MarioPawnBPClass.Class)
	{
		DefaultPawnClass = MarioPawnBPClass.Class;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("BP_Mario 로드 실패. AMarioCharacter로 폴백"));
		DefaultPawnClass = AMarioCharacter::StaticClass();
	}
}
