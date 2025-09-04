#include "TunnellerActorComponent.h"
#include "Kismet/GameplayStatics.h"

#include "../Player/AMainPawn.h"


UTunnellerActorComponent::UTunnellerActorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}


void UTunnellerActorComponent::BeginPlay()
{
	Super::BeginPlay();

    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC) return;

    APawn* Pawn = PC->GetPawn();
    AActor* Owner = GetOwner();
    if (Pawn && Owner)
    {
        MainPawnRef = Cast<AMainPawn>(Pawn);
        PlayerPosOnBeginPlay = Pawn->GetActorLocation();

        MoveDirOnBeginPlay = (PlayerPosOnBeginPlay - Owner->GetActorLocation()).GetSafeNormal();
    }
}

void UTunnellerActorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    AActor* Owner = GetOwner();
    if (Owner)
    {
        float Speed = ActiveSpeed;
        if (Owner->ActorHasTag("Frozen"))
            Speed = FrozenSpeed;

        Owner->SetActorLocation(Owner->GetActorLocation() + (MoveDirOnBeginPlay * Speed * DeltaTime), true);
    }
}

