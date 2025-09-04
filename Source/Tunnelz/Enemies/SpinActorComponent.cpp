#include "SpinActorComponent.h"

// Sets default values for this component's properties
USpinActorComponent::USpinActorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	bAutoActivate = true;
}


// Called when the game starts
void USpinActorComponent::BeginPlay()
{
	Super::BeginPlay();
}


// Called every frame
void USpinActorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (AActor* Owner = GetOwner())
	{
		const FRotator DeltaRot = DegreesPerSecond * DeltaTime;
		Owner->AddActorLocalRotation(DeltaRot);
	}
}

