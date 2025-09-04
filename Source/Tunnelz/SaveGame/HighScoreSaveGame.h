#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "HighScoreSaveGame.generated.h"

UCLASS()
class TUNNELZ_API UHighScoreSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame)
	unsigned int HighScore = 0;
};
