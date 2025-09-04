#include "EnemyActor.h"
#include "Kismet/GameplayStatics.h"

#include "../GameMode/MainGameMode.h"

// Sets default values
AEnemyActor::AEnemyActor()
{
	PrimaryActorTick.bCanEverTick = true;

	Tags.Add(FName("Enemy"));

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(GetRootComponent());
	RootComponent = MeshComponent;

}

// Called when the game starts or when spawned
void AEnemyActor::BeginPlay()
{
	Super::BeginPlay();

	if (UStaticMeshComponent* Mesh = FindComponentByClass<UStaticMeshComponent>())
	{
		// Slot 0 is the first material on the mesh
		UMaterialInterface* BaseMat = Mesh->GetMaterial(0);
		DynMat = UMaterialInstanceDynamic::Create(BaseMat, this);

		// Assign the dynamic material back to the mesh
		Mesh->SetMaterial(0, DynMat);
	}
}

// Called every frame
void AEnemyActor::Tick(float DeltaTime)
{
	if (IsPendingKillPending())
		return;

	Super::Tick(DeltaTime);

	// Destroy self if gone behind player
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (PlayerPawn)
	{
		if (GetActorLocation().X < PlayerPawn->GetActorLocation().X)
		{
			AMainGameMode* GM = Cast<AMainGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
			if (GM)
				GM->OnActiveEnemyDestroyed(this); // still need to notify GM so it has proper alive enemy count

			PrimaryActorTick.bCanEverTick = false;
			Destroy();
		}
	}
}

void AEnemyActor::Freeze()
{
	if (ActorHasTag("Frozen"))
		return;

	AMainGameMode* GM = Cast<AMainGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
	if (GM)
		GM->OnActiveEnemyDestroyed(this);

	// Ensure adding the Frozen tag comes after OnActiveEnemyDestroyed() call.
	// OnActiveEnemyDestroyed() will not decrement active enemies if passed in actor has Frozen tag.
	Tags.Add(FName("Frozen"));

	// Set frozen material visuals
	if (DynMat)
	{
		DynMat->SetScalarParameterValue(FName("Dithering"), -0.1f);
		DynMat->SetVectorParameterValue(FName("BaseTint"), FrozenTintColor);
	}

	// Change collision preset
	if (UStaticMeshComponent* Mesh = FindComponentByClass<UStaticMeshComponent>())
	{
		Mesh->SetCollisionProfileName(FName("FrozenEnemy"));
	}
}

