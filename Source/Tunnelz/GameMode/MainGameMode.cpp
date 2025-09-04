#include "MainGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerController.h"

#include "../Player/AMainPawn.h"
#include "../Enemies/EnemyActor.h"
#include "../SaveGame/HighScoreSaveGame.h"

#define HIGH_SCORE_SAVE_SLOT_NAME TEXT("HighScore")

void AMainGameMode::BeginPlay()
{
    Super::BeginPlay();
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    // High score saving
    if (UGameplayStatics::DoesSaveGameExist(HIGH_SCORE_SAVE_SLOT_NAME, 0))
    {
        USaveGame* Loaded = UGameplayStatics::LoadGameFromSlot(HIGH_SCORE_SAVE_SLOT_NAME, 0);
        SaveHighScoreSG = Cast<UHighScoreSaveGame>(Loaded);
    }

    if (!SaveHighScoreSG)
    {
        SaveHighScoreSG = Cast<UHighScoreSaveGame>(UGameplayStatics::CreateSaveGameObject(UHighScoreSaveGame::StaticClass()));
    }

    check(SaveHighScoreSG);
    
    // Calculate spawn enemy aabb
    EnemySpawnAABB.Max.X = ArenaSize.X - SpawnOffsetFromArenaWall.X;
    EnemySpawnAABB.Max.Y = ArenaSize.Y / 2.f - SpawnOffsetFromArenaWall.Y;
    EnemySpawnAABB.Max.Z = ArenaSize.Z / 2.f - SpawnOffsetFromArenaWall.Z;

    EnemySpawnAABB.Min.X = EnemySpawnAABB.Max.X - 350.f; // 3.5 meter from max X
    EnemySpawnAABB.Min.Y = -ArenaSize.Y / 2.f + SpawnOffsetFromArenaWall.Y;
    EnemySpawnAABB.Min.Z = -ArenaSize.Z / 2.f + SpawnOffsetFromArenaWall.Z;

    if (!MenuWidget && MenuWidgetClass)
    {
        MenuWidget = CreateWidget<UUserWidget>(GetWorld(), MenuWidgetClass);
        if (MenuWidget)
        {
            MenuWidget->AddToViewport(100);
            MenuWidget->SetVisibility(ESlateVisibility::Hidden);
        }
    }

    if (!HUDWidget && HUDWidgetClass)
    {
        HUDWidget = CreateWidget<UUserWidget>(GetWorld(), HUDWidgetClass);
        if (HUDWidget)
        {
            HUDWidget->AddToViewport(101);
            HUDWidget->SetVisibility(ESlateVisibility::Hidden);
        }
    }

    ShowMenu(); // boot into menu
}

void AMainGameMode::ShowMenu()
{
    Phase = ERunPhase::Menu;

    if (MenuWidget)
    {
        MenuWidget->SetVisibility(ESlateVisibility::Visible);
    }

    if (HUDWidget)
    {
        HUDWidget->SetVisibility(ESlateVisibility::Hidden);
    }

    // freeze input to game world, allow UI
    SetInputUI(true);
    UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 0.f); // optional pause
}

void AMainGameMode::StartRun()
{
    if (MenuWidget)
    {
        MenuWidget->SetVisibility(ESlateVisibility::Hidden);
    }

    if (HUDWidget)
    {
        HUDWidget->SetVisibility(ESlateVisibility::Visible);
    }

    // unpause, game input on
    UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 1.f);
    SetInputUI(false);

    // reset world & (re)spawn player
    SoftResetWorld();

    Phase = ERunPhase::Playing;
}

void AMainGameMode::OnPlayerDied()
{
    Phase = ERunPhase::GameOver;

    ShowMenu();

    // Save high score if needed
    if (SaveHighScoreSG && Score > SaveHighScoreSG->HighScore)
    {
        SaveHighScoreSG->HighScore = Score;
        UGameplayStatics::SaveGameToSlot(SaveHighScoreSG, HIGH_SCORE_SAVE_SLOT_NAME, 0);
    }
}

void AMainGameMode::SoftResetWorld()
{
    // 1) Clear enemies / pickups (use tags or an interface in your project)
    TArray<AActor*> ToKill;
    UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Enemy"), ToKill);
    for (AActor* A : ToKill) { if (IsValid(A)) A->Destroy(); }

    // 2) Reset GM states
    CurLevel = 0;
    NumAliveEnemies = 0;
    Score = 0;
    if (Levels.Num() > 0)
    {
        NextLevelTimer = Levels[0].DurationSec;
        SpawnTimer = Levels[0].SpawnRateSec;
    }

    // 3) Respawn or reset the player
    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC) return;

    APawn* Pawn = PC->GetPawn();
    UE_LOG(LogTemp, Warning, TEXT("PC pawn: %s  class: %s"),
        *GetNameSafe(Pawn),
        Pawn ? *Pawn->GetClass()->GetName() : TEXT("<null>"));

    // If pawn exists, reset it; otherwise RestartPlayer will spawn at PlayerStart.
    if (AMainPawn* P = Cast<AMainPawn>(PC->GetPawn()))
    {
        P->BeginSession();
        P->ForceNetUpdate(); // local singleplayer but harmless
    }
    else
    {
        RestartPlayer(PC); // spawn at PlayerStart
    }
}

void AMainGameMode::SetInputUI(bool bUI)
{
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        FInputModeGameAndUI Mode;
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        Mode.SetHideCursorDuringCapture(false);
        // Optional: Mode.SetWidgetToFocus(nullptr); // don't force focus to a widget

        PC->SetInputMode(Mode);
        PC->bShowMouseCursor = true;

        // Keep/clear these depending on your camera needs:
        PC->SetIgnoreLookInput(bUI); // ignore look only when UI-focused
        // If you were using GameOnly, that often captures the mouse and kills further clicks.
    }
}

TSubclassOf<AEnemyActor> AMainGameMode::PickEnemyFromWeights() const
{
    check(Levels.Num() > 0);

    double total = 0.0;
    for (const auto& KV : Levels[CurLevel].EnemyWeights)
    {
        if (KV.Weight > 0.f)
            total += KV.Weight;
    }
    if (total <= 0.0) 
        return nullptr;

    // Weighted random pick
    const double r = FMath::FRand() * total;
    double acc = 0.0;
    for (const auto& KV : Levels[CurLevel].EnemyWeights)
    {
        const float w = KV.Weight > 0.f ? KV.Weight : 0.f;
        acc += w;
        if (r <= acc)
            return KV.Class;
    }
    return nullptr;
}

void AMainGameMode::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (ERunPhase::Playing != Phase)
        return;

    if (Levels.Num() == 0)
        return;

    // Check if we're going to the next level
    if (NextLevelTimer <= 0.f && CurLevel < Levels.Num() - 1)
    {
        CurLevel += 1;
        NextLevelTimer = Levels[CurLevel].DurationSec;
        SpawnTimer = Levels[CurLevel].SpawnRateSec;
    }

    GEngine->AddOnScreenDebugMessage(uint64(uintptr_t(this)), 9999.0f, FColor::Yellow, 
        FString::Printf(TEXT("Level: %d | Spawn T.: %.1f | Next Level T.: %.1f"), CurLevel, SpawnTimer, NextLevelTimer));
    
    NextLevelTimer -= DeltaTime;
    SpawnTimer -= DeltaTime;

    if (SpawnTimer <= 0.f)
    {
        SpawnTimer = Levels[CurLevel].SpawnRateSec;

        if (NumAliveEnemies < Levels[CurLevel].MaxNumActiveEnemies)
        {
            TSubclassOf<AEnemyActor> enemyClass = PickEnemyFromWeights();
            if (enemyClass)
            {
                int const maxTries = 16;
                for (int i = 0; i < maxTries; i++)
                {
                    FVector RandomPoint = UKismetMathLibrary::RandomPointInBoundingBox_Box(EnemySpawnAABB);
                    if (GetWorld()->SpawnActor<AEnemyActor>(enemyClass, RandomPoint, FRotator::ZeroRotator))
                    {
                        NumAliveEnemies++;
                        break;
                    }
                }
            }
        }
    }
}

void AMainGameMode::OnActiveEnemyDestroyed(AActor* const EnemyActor)
{
    if (EnemyActor->ActorHasTag("Frozen"))
        return;

    NumAliveEnemies -= 1;
    check(NumAliveEnemies >= 0);
}

void AMainGameMode::CollectFrozenEnemies()
{
    TArray<AActor*> FrozenEnemyActors;
    UGameplayStatics::GetAllActorsWithTag(GetWorld(), FName("Frozen"), FrozenEnemyActors);

    for (AActor* Actor : FrozenEnemyActors)
    {
        Actor->Destroy();
    }

    Score += FrozenEnemyActors.Num();
}