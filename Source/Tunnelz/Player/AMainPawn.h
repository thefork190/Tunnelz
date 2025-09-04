#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "AMainPawn.generated.h"

UCLASS()
class TUNNELZ_API AMainPawn : public APawn
{
    GENERATED_BODY()

public:
    AMainPawn();
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    void BeginSession();  // called by game manager

    // Enhanced Input
    UPROPERTY(EditDefaultsOnly, Category = "Input|Enhanced")
    TObjectPtr<UInputMappingContext> IMC_Default;

    UPROPERTY(EditDefaultsOnly, Category = "Input|Enhanced")
    TObjectPtr<UInputAction> IA_Look;

    UPROPERTY(EditDefaultsOnly, Category = "Behavior")
    float InvincibleTime = 1.f;

    UPROPERTY(EditDefaultsOnly, Category = "Behavior")
    float SwapLaneDestrEnemiesRadius = 100.f; // 1m

protected:
    virtual void BeginPlay() override;
    UFUNCTION() void OnLook(const FInputActionValue& Value);
    UFUNCTION(BlueprintPure, Category = "Behavior") bool IsInvincible() const { return InvincibleTimer > 0.f; }

    UFUNCTION(BlueprintPure, Category = "Input")
    bool IsChangeLaneFlickReady() const { return HUDCooldownUpT <= 0.f; }

    UFUNCTION(BlueprintPure, Category = "Input")
    float GetChangeLaneFlickCooldownRemaining() const { return HUDCooldownUpT; }

    // Used for power/loading bar ui widgets
    UFUNCTION(BlueprintPure, Category = "Input")
    float GetChangeLaneFlickCooldownNorm() const
    {
        const float maxCd = FMath::Max(UpChan.Detector.Cooldown, KINDA_SMALL_NUMBER);
        return 1.f - FMath::Clamp(HUDCooldownUpT / maxCd, 0.f, 1.f);
    }

    UFUNCTION(BlueprintPure, Category = "Input")
    bool IsCollectFlickReady() const { return HUDCooldownRightT <= 0.f; }

    void LaneSwapAndDestroyEnemies(FVector const Pos);

private:

    FVector ArenaSize = FVector(20.f, 3.f, 4.f);
    UCameraComponent* Camera = nullptr;
    FVector NeutralPosition = FVector(0.f, 0.f, 0.f);
    FVector StartPos = FVector(0.f, 0.f, 0.f);

    // I-frames
    float InvincibleTimer = 0.f;

    // -------- Small helpers shared by filters --------
    static float SoftDZ(float x, float dz)
    {
        const float ax = FMath::Abs(x);
        return (ax <= dz) ? 0.f : FMath::Sign(x) * (ax - dz);
    }
    static float Median3(float a, float b, float c)
    {
        return a + b + c - FMath::Min3(a, b, c) - FMath::Max3(a, b, c);
    }

    // -------- Signal filter: median3 + attack/decay EMA + soft deadzone --------
    struct FAxisFilter
    {
        // Tunables
        float TauAttack = 0.020f; // sec
        float TauRelease = 0.080f; // sec
        float Deadzone = 0.05f;  // rad/s (~3°/s)

        // State
        float LPF = 0.f;
        float Hist[3] = { 0.f,0.f,0.f };
        int   HistIdx = 0;
        bool  bFilled = false;

        float Step(float raw, float dt)
        {
            // median(3)
            Hist[HistIdx] = raw;
            HistIdx = (HistIdx + 1) % 3;
            if (HistIdx == 0) bFilled = true;
            const float in = bFilled ? AMainPawn::Median3(Hist[0], Hist[1], Hist[2]) : raw;

            // attack / release EMA
            const float aA = 1.f - FMath::Exp(-dt / FMath::Max(1e-6f, TauAttack));
            const float aR = 1.f - FMath::Exp(-dt / FMath::Max(1e-6f, TauRelease));
            const float alpha = (FMath::Abs(in) > FMath::Abs(LPF)) ? aA : aR;
            LPF = FMath::Lerp(LPF, in, alpha);

            // soft deadzone
            return AMainPawn::SoftDZ(LPF, Deadzone);
        }
    };

    // -------- Peak-based, debounced, sign-aware flick detector --------
    struct FFlickDetector
    {
        // Tunables
        float StartRateRad = 1.1f;
        float EndRateRad = 0.8f;
        float MaxDuration = 0.35f; // sec
        float Cooldown = 0.45f; // external knob; success enforces >= 0.08s

        // State
        enum class EState : uint8 { Idle, Armed } State = EState::Idle;
        float  Timer = 0.f;
        float  PeakAbsRate = 0.f;
        float  PeakSign = +1.f;
        float  CooldownT = 0.f;

        // Debounce per sign (require N consecutive frames above Start)
        static constexpr int DebounceN = 2;
        int  PosCount = 0, NegCount = 0;
        bool DebAbovePos = false, DebAboveNeg = false;
        bool PrevDebAbovePos = false, PrevDebAboveNeg = false;

        int Update(float rate, float dt)
        {
            const float dti = (dt > (1.0f / 45.0f)) ? (1.0f / 45.0f) : dt;
            if (CooldownT > 0.f) { CooldownT = FMath::Max(0.f, CooldownT - dti); return 0; }

            const float ar = FMath::Abs(rate);
            const float sgn = (rate >= 0.f) ? +1.f : -1.f;

            const bool aboveStartPos = (rate > +StartRateRad);
            const bool aboveStartNeg = (rate < -StartRateRad);

            PosCount = aboveStartPos ? FMath::Min(PosCount + 1, 1000) : 0;
            NegCount = aboveStartNeg ? FMath::Min(NegCount + 1, 1000) : 0;

            DebAbovePos = (PosCount >= DebounceN);
            DebAboveNeg = (NegCount >= DebounceN);

            const bool risingPos = (DebAbovePos && !PrevDebAbovePos);
            const bool risingNeg = (DebAboveNeg && !PrevDebAboveNeg);

            switch (State)
            {
            case EState::Idle:
            {
                if (risingPos || risingNeg)
                {
                    State = EState::Armed;
                    Timer = 0.f;
                    PeakAbsRate = ar;
                    PeakSign = sgn;
                }
                break;
            }
            case EState::Armed:
            {
                Timer += dti;
                if (ar > PeakAbsRate) PeakAbsRate = ar;

                const bool aboveEnd = (ar > EndRateRad);
                const bool sameSign = ((sgn >= 0.f) == (PeakSign >= 0.f));

                const bool timeOut = (Timer > MaxDuration);
                const bool dropBelowEnd = (!aboveEnd && Timer >= 0.02f);
                const bool signReversalBeyondEnd = (!sameSign && aboveEnd);

                if (timeOut || dropBelowEnd || signReversalBeyondEnd)
                {
                    int dir = 0;
                    if (PeakAbsRate >= StartRateRad && !timeOut)
                    {
                        dir = (PeakSign >= 0.f) ? +1 : -1;
                        CooldownT = FMath::Max(Cooldown, 0.08f);
                    }
                    else
                    {
                        CooldownT = 0.08f;
                    }

                    State = EState::Idle;
                    Timer = 0.f;
                    PeakAbsRate = 0.f;
                    PosCount = NegCount = 0;

                    PrevDebAbovePos = DebAbovePos;
                    PrevDebAboveNeg = DebAboveNeg;
                    return dir;
                }
                break;
            }
            }

            PrevDebAbovePos = DebAbovePos;
            PrevDebAboveNeg = DebAboveNeg;
            return 0;
        }
    };

    // -------- One per-axis channel: filter + detector + small “rest/cooldown” gate --------
    struct FAxisChannel
    {
        FAxisFilter    Filter;
        FFlickDetector Detector;

        // Pawn-level rest + manual cooldown (prevents rebound doubles)
        bool  bNeedsRest = false;
        float ManualCDT = 0.f;
        float RestRate = 0.25f; // must dip below this once after a hit

        void Decay(float smoothedRate, float dt)
        {
            if (ManualCDT > 0.f) ManualCDT = FMath::Max(0.f, ManualCDT - dt);
            if (bNeedsRest && FMath::Abs(smoothedRate) < RestRate)
                bNeedsRest = false;
        }
        int Submit(float feedRate, float smoothedForRest, float dt)
        {
            const int dir = Detector.Update(feedRate, dt);
            if (dir == 0) return 0;

            if (ManualCDT <= 0.f && !bNeedsRest)
            {
                ManualCDT = 0.18f;
                bNeedsRest = true;
                return dir;
            }
            return 0;
        }
    };

    // Channels
    FAxisChannel UpChan;
    FAxisChannel RightChan;

    // Cooldowns for UI
    float HUDCooldownUpT = 0.f;
    float HUDCooldownRightT = 0.f;

protected:
    // ---- Cross-talk gating ----
    UPROPERTY(EditAnywhere, Category = "Flick|Tuning", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float PurityMin = 0.75f;

    UPROPERTY(EditAnywhere, Category = "Flick|Tuning", meta = (ClampMin = "1.0"))
    float DominanceRatio = 1.40f;

    UPROPERTY(EditAnywhere, Category = "Flick|Tuning", meta = (ClampMin = "0.0"))
    float MinArmMagScale = 0.85f;

    // ---- Cone gating (degrees) ----
    // Narrow to ARM, wider to KEEP once armed (hysteresis)
    UPROPERTY(EditAnywhere, Category = "Flick|Tuning", meta = (ClampMin = "0.0", ClampMax = "90.0"))
    float ArmConeDeg = 16.f;

    UPROPERTY(EditAnywhere, Category = "Flick|Tuning", meta = (ClampMin = "0.0", ClampMax = "90.0"))
    float KeepConeDeg = 28.f;

private:
    // Lane switching
    void StartLaneChange(const FVector& TargetPos, float Duration);

    FAlphaBlend LaneBlend;
    FVector LaneStart, LaneTarget;
};
