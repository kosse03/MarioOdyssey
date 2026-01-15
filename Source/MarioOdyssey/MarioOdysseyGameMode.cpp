// Copyright Epic Games, Inc. All Rights Reserved.

#include "MarioOdysseyGameMode.h"
#include "MarioOdysseyCharacter.h"
#include "UObject/ConstructorHelpers.h"

AMarioOdysseyGameMode::AMarioOdysseyGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
