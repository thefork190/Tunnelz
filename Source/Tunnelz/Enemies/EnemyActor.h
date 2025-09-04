#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "EnemyActor.generated.h"

UCLASS()
class TUNNELZ_API AEnemyActor : public AActor
{
	GENERATED_BODY()
	
public:
	// Sets default values for this actor's properties
	AEnemyActor();

	UFUNCTION(BlueprintCallable) void Freeze();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComponent = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals")
	FVector FrozenTintColor = FVector(1.f, 0.f, 0.f);

private:
	UMaterialInstanceDynamic* DynMat = nullptr;
};
