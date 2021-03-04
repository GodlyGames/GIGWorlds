// Copyright Epic Games, Inc. All Rights Reserved.

#include "GIGWorldsGameMode.h"
#include "GIGWorldsCharacter.h"
#include "UObject/ConstructorHelpers.h"

AGIGWorldsGameMode::AGIGWorldsGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPersonCPP/Blueprints/ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
