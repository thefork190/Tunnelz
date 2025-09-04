#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TunnellerActorComponent.generated.h"

class AMainPawn;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class TUNNELZ_API UTunnellerActorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTunnellerActorComponent();

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
	float ActiveSpeed = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behavior")
	float FrozenSpeed = 50.f;

private:
	AMainPawn* MainPawnRef = nullptr;
	FVector PlayerPosOnBeginPlay = FVector(0.f, 0.f, 0.f);
	FVector MoveDirOnBeginPlay;
};
