#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/GameModeBase.h"
#include "MainGameMode.generated.h"

class AEnemyActor;
class UHighScoreSaveGame;

UENUM(BlueprintType)
enum class ERunPhase : uint8 { Menu, Playing, GameOver };

USTRUCT(BlueprintType)
struct FEnemyWeight 
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSubclassOf<AEnemyActor> Class;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0")) float Weight = 1.f;
};

USTRUCT(BlueprintType)
struct FLevelProgression
{
    GENERATED_BODY()

    // How long this level lasts
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level", meta = (ClampMin = "1.0"))
    float DurationSec = 5.f;

    // Enemy weights (enemies that can spawn in this level)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level")
    TArray<FEnemyWeight> EnemyWeights;

    // Seconds between spawns while under MaxNumActiveEnemies
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level", meta = (ClampMin = "0.05"))
    float SpawnRateSec = 1.0f;

    // Cap on concurrent enemies
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level", meta = (ClampMin = "0"))
    int32 MaxNumActiveEnemies = 5;
};

UCLASS()
class TUNNELZ_API AMainGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, Category = UI)
    TSubclassOf<UUserWidget> MenuWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category = UI)
    TSubclassOf<UUserWidget> HUDWidgetClass;

    UFUNCTION(BlueprintCallable) void StartRun();      // Start / Retry
    UFUNCTION(BlueprintCallable) void ShowMenu();      // Menu after death or at boot
    UFUNCTION(BlueprintCallable) void OnPlayerDied();  // Called by pawn
    UFUNCTION(BlueprintCallable) void CollectFrozenEnemies();

    virtual void Tick(float DeltaTime) override;

    UFUNCTION(BlueprintCallable) bool IsPlaying() const 
    { 
        return Phase == ERunPhase::Playing;
    }

    UFUNCTION(BlueprintCallable) int GetScore() const
    {
        return Score;
    }

    UFUNCTION(BlueprintCallable) int GetHighScore() const;

    UFUNCTION(BlueprintCallable) bool HasNewHighScore() const
    {
        return bHasNewHighScore;
    }

    void OnActiveEnemyDestroyed(AActor* const EnemyActor);

protected:
    virtual void BeginPlay() override;

private:
    void SoftResetWorld();
    void SetInputUI(bool bUI);
    TSubclassOf<AEnemyActor> PickEnemyFromWeights() const;

public:
    UPROPERTY() UUserWidget* MenuWidget = nullptr;
    UPROPERTY() UUserWidget* HUDWidget = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Behavior")
    ERunPhase Phase = ERunPhase::Menu;
    
    UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Arena")
    FVector ArenaSize = FVector(20.f, 3.f, 4.f);

    UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Arena")
    FVector SpawnOffsetFromArenaWall = FVector(1.f, 1.f, 1.f);

    UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Level Progression")
    TArray<FLevelProgression> Levels;

private:
    FBox EnemySpawnAABB;

    int CurLevel = 0;
    float NextLevelTimer = 0.f;
    float SpawnTimer = 0.f;
    int NumAliveEnemies = 0;
    unsigned int Score = 0;
    bool bHasNewHighScore = false;

    UPROPERTY(Transient)
    TObjectPtr<UHighScoreSaveGame> SaveHighScoreSG = nullptr;
};