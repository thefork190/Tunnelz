#include "AMainPawn.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"

#include "../GameMode/MainGameMode.h"
#include "../Enemies/EnemyActor.h"


namespace
{
    // Simple exponential smoothing coefficient for time constant Tau
    inline float Alpha(float dt, float Tau) { return 1.f - FMath::Exp(-dt / FMath::Max(Tau, 1e-5f)); }

    // Return angle of (u,r) relative to +Up axis in radians: 0 = up, +right = +90°
    inline float AngleVsUp(float u, float r) { return FMath::Atan2(r, u); }

    // Gate vector into cone centered on Up:
    // - If Idle, require |ang| <= ArmCone; if Armed, allow wider |ang| <= KeepCone.
    inline bool InConeUp(float ang, bool bArmed, float ArmCone, float KeepCone)
    {
        const float limit = bArmed ? KeepCone : ArmCone;
        return FMath::Abs(ang) <= limit;
    }

    // Convert degrees once if gyro is in deg/s
    inline FVector MaybeDegToRad(const FVector& GyroDeg, bool bDeg)
    {
        return bDeg ? (GyroDeg * PI / 180.f) : GyroDeg;
    }
}

// Sets default values
AMainPawn::AMainPawn()
{
    PrimaryActorTick.bCanEverTick = true;
    AutoPossessPlayer = EAutoReceiveInput::Player0;

    // Simple root + camera
    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(Root);
    Camera->bUsePawnControlRotation = false; // we drive rotation manually
    Camera->bConstrainAspectRatio = false;
    Camera->SetAspectRatioAxisConstraint(EAspectRatioAxisConstraint::AspectRatio_MaintainYFOV);

    AMainGameMode* GM = Cast<AMainGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
    if (GM)
        ArenaSize = GM->ArenaSize;
}

void AMainPawn::BeginSession()
{
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        if (ULocalPlayer* LP = PC->GetLocalPlayer())
            if (auto* Sub = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
            {
                Sub->ClearAllMappings();
                if (IMC_Default) Sub->AddMappingContext(IMC_Default, 0);
                else UE_LOG(LogTemp, Warning, TEXT("IMC_Default is null - assign it in the PC details or load it in code."));
            }


        // Move camera based on viewport size
        int32 SX = 0, SY = 0;
        PC->GetViewportSize(SX, SY);

        const float realAR = (SY > 0) ? float(SX) / float(SY) : 0.5625f; // width/height
        const float designAR = 9.f / 16.f;                                 // 0.5625
        const float effAR = FMath::Min(realAR, designAR); // pretend narrow when wider

        const float halfY = FMath::DegreesToRadians(Camera->FieldOfView) * 0.5f;
        const float halfX = FMath::Atan(FMath::Tan(halfY) * effAR);

        // Your arena rectangle in camera plane:
        const float targetWidth = ArenaSize.Y;
        const float targetHeight = ArenaSize.Z;

        // Distance to fit both dimensions of the 9:16 “safe frame”
        const float dV = (0.5f * targetHeight) / FMath::Tan(halfY);
        const float dH = (0.5f * targetWidth) / FMath::Tan(halfX);
        const float d = FMath::Max(dV, dH);

        NeutralPosition = (-FVector::ForwardVector * d);

        StartPos = NeutralPosition;
        StartPos.Y -= ArenaSize.Y / 4.f;

        SetActorLocation(StartPos);

        LaneBlend.Reset();
        LaneStart = GetActorLocation();
        LaneTarget = StartPos;
    }
}

// Called when the game starts or when spawned
void AMainPawn::BeginPlay()
{
    Super::BeginPlay();

}

void AMainPawn::StartLaneChange(const FVector& TargetPos, float Duration)
{
    LaneStart = GetActorLocation();
    LaneTarget = TargetPos;

    if (LaneTarget != LaneStart)
    {
        LaneBlend.SetBlendTime(Duration);
        LaneBlend.SetBlendOption(EAlphaBlendOption::ExpOut); // exponential ease-out
        LaneBlend.Reset();
    }
}

void AMainPawn::LaneSwapAndDestroyEnemies(FVector const Pos)
{
    InvincibleTimer = InvincibleTime;

    UWorld* World = GetWorld();
    if (!World) return;

    StartLaneChange(Pos, 0.11f);

    // Overlap setup
    TArray<FOverlapResult> Overlaps;
    FCollisionShape Sphere = FCollisionShape::MakeSphere(SwapLaneDestrEnemiesRadius);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(DestroyEnemiesInRadius), false);

    bool bHit = World->OverlapMultiByObjectType(
        Overlaps,
        Pos,
        FQuat::Identity,
        FCollisionObjectQueryParams(ECollisionChannel::ECC_GameTraceChannel1),
        Sphere,
        Params);

    if (bHit)
    {
        for (const FOverlapResult& Result : Overlaps)
        {
            AActor* Actor = Result.GetActor();
            if (Actor && Actor->IsA(AEnemyActor::StaticClass()))
            {
                Actor->SetActorTickEnabled(false);
                Actor->Destroy();

                AMainGameMode* GM = Cast<AMainGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
                if (GM)
                    GM->OnActiveEnemyDestroyed(Actor);
            }
        }
    }

#if WITH_EDITOR
    // Optional debug visualization
    DrawDebugSphere(World, Pos, SwapLaneDestrEnemiesRadius, 16, FColor::Red, false, 0.5f);
#endif
}

// Called every frame
void AMainPawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!LaneBlend.IsComplete())
    {
        LaneBlend.Update(DeltaTime);
        float eased = LaneBlend.GetAlpha();

        FVector pos = FMath::Lerp(LaneStart, LaneTarget, eased);
        SetActorLocation(pos, true);
    }

    InvincibleTimer = FMath::Max(0.f, InvincibleTimer - DeltaTime);
    HUDCooldownUpT = FMath::Max(0.f, HUDCooldownUpT - DeltaTime);
    HUDCooldownRightT = FMath::Max(0.f, HUDCooldownRightT - DeltaTime);

    AMainGameMode* GM = Cast<AMainGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
    if (GM && GM->Phase != ERunPhase::Playing)
        return;

#if !WITH_EDITOR
    // -------- Controller & IMU --------
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC) return;

    FVector Tilt, RotationRate, Gravity, Accel;
    PC->GetInputMotionState(Tilt, RotationRate, Gravity, Accel);
    
    float u_raw = RotationRate.Z; // Up (toward screen top)
    float r_raw = RotationRate.Y; // Right

    static float maxu_raw = 0.f;
    static float maxr_raw = 0.f;
    maxu_raw = fmax(maxu_raw, u_raw);
    maxr_raw = fmax(maxr_raw, r_raw);
    
    // -------- Project gyro onto axes & filter each channel --------
    const float smUp = UpChan.Filter.Step(u_raw, DeltaTime);
    const float smRight = RightChan.Filter.Step(r_raw, DeltaTime);

    // No roll compensation needed in device space
    const float upAdj = smUp;
    const float rightAdj = smRight;

    // Decay/gate per-channel
    UpChan.Decay(upAdj, DeltaTime);
    RightChan.Decay(rightAdj, DeltaTime);

    // -------- Cross-talk & cone gating (UP channel) --------
    const float u = upAdj;
    const float r = rightAdj;
    const float uAbs = FMath::Abs(u);
    const float rAbs = FMath::Abs(r);
    const float vMag = FMath::Sqrt(u * u + r * r) + 1e-6f;

    const float minArmMag = UpChan.Detector.StartRateRad * MinArmMagScale;
    const bool  rightDom = (rAbs > uAbs * DominanceRatio) && (rAbs > minArmMag);

    const float angUp = FMath::Atan2(r, u); // 0=Up, +90°=Right
    const float armConeRad = FMath::DegreesToRadians(ArmConeDeg);
    const float keepConeRad = FMath::DegreesToRadians(KeepConeDeg);

    const bool bUpArmed = (UpChan.Detector.State == FFlickDetector::EState::Armed);
    const bool inCone = (FMath::Abs(angUp) <= (bUpArmed ? keepConeRad : armConeRad));

    const float purityUp = uAbs / vMag;
    const bool  purityOk = (purityUp >= PurityMin) || (vMag < minArmMag);

    const float feedUp = (inCone && purityOk && !rightDom) ? u : 0.f;

    if (rightDom && UpChan.Detector.State == FFlickDetector::EState::Idle)
    {
        UpChan.Detector.PosCount = 0;
        UpChan.Detector.NegCount = 0;
    }

    // -------- Mirror for RIGHT channel (center cone at ±90°) --------
    const float angRight = angUp - PI * 0.5f;
    const bool  bRightArmed = (RightChan.Detector.State == FFlickDetector::EState::Armed);
    const bool  inRightCone = (FMath::Abs(angRight) <= (bRightArmed ? keepConeRad : armConeRad));

    const float purityRight = rAbs / vMag;
    const bool  purityRightOk = (purityRight >= PurityMin) || (vMag < minArmMag);
    const bool  upDom = (uAbs > rAbs * DominanceRatio) && (uAbs > minArmMag);

    const float feedRight = (inRightCone && purityRightOk && !upDom) ? r : 0.f;

    if (upDom && RightChan.Detector.State == FFlickDetector::EState::Idle)
    {
        RightChan.Detector.PosCount = 0;
        RightChan.Detector.NegCount = 0;
    }

    // -------- Submit detectors --------
    const int upFlick = UpChan.Submit(feedUp, u, DeltaTime);
    const int rightFlick = RightChan.Submit(feedRight, r, DeltaTime);

    if (upFlick != 0 && IsChangeLaneFlickReady())
    {
        FVector L = LaneTarget;
        const float YOffset = ArenaSize.Y / 4.f;
        L.Y = (L.Y < 0.f) ? YOffset : -YOffset;
        LaneSwapAndDestroyEnemies(L);

        HUDCooldownUpT = UpChan.Detector.Cooldown; // reset cooldown
    }

    // Example: do something on right flick (optional)
    if (rightFlick != 0 && IsCollectFlickReady())
    {
        GEngine->AddOnScreenDebugMessage((uint64)uintptr_t(this) + 2, 0.5f, FColor::Red, TEXT("Collect Flick"));
        GM->CollectFrozenEnemies();

        HUDCooldownRightT = RightChan.Detector.Cooldown; // reset cooldown
    }
#endif
}

// Input bindings
void AMainPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (IA_Look)
        {
            EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AMainPawn::OnLook);
            EIC->BindAction(IA_Look, ETriggerEvent::Ongoing, this, &AMainPawn::OnLook);
            EIC->BindAction(IA_Look, ETriggerEvent::Started, this, &AMainPawn::OnLook);
            EIC->BindAction(IA_Look, ETriggerEvent::Completed, this, &AMainPawn::OnLook);
        }
    }
}

void AMainPawn::OnLook(const FInputActionValue& Value)
{
    AMainGameMode* GM = Cast<AMainGameMode>(UGameplayStatics::GetGameMode(GetWorld()));
    if (!LaneBlend.IsComplete() || (GM && GM->Phase != ERunPhase::Playing))
        return;

    const FVector2D Delta = Value.Get<FVector2D>();
    GEngine->AddOnScreenDebugMessage(uint64(uintptr_t(this)), 5.f, FColor::Yellow,
        FString::Printf(TEXT("Delta: %.1f, %.1f"), Delta.X, Delta.Y));

    FVector L = GetActorLocation();
    FVector NewLocation = L;
    float const YOffset = ArenaSize.Y / 4.f;
    float const mag = 5.f;
    if (Delta.X < -mag)
    {
        NewLocation.Y = -YOffset;
    }
    else if (Delta.X > mag)
    {
        NewLocation.Y = YOffset;
    }

    if (NewLocation != L)
    {        
        LaneSwapAndDestroyEnemies(NewLocation);
    }
}
