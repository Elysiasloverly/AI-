#include "Combat/Projectiles/RogueBossAttackActor.h"
#include "Combat/Visuals/RogueGroundEffectFade.h"

#include "Combat/Pooling/RogueCombatPoolSubsystem.h"
#include "Combat/Visuals/RogueCombatAccentEffect.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Enemies/RogueEnemy.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/RogueCharacter.h"

namespace
{
struct FBossFxFrame
{
    float Age;
    float Progress = 0.f;
    float Opacity = 1.f;
    float Flash = 0.f;
    float Phase = 0.f;
};

void Layer(UStaticMeshComponent* Mesh, const FVector& Offset, const FVector& Scale,
    const FRotator& Rotation, const FBossFxFrame& Frame)
{
    if (!Mesh->GetStaticMesh() || Frame.Opacity <= .002f)
    {
        Mesh->SetVisibility(false);
        return;
    }
    Mesh->SetVisibility(true);
    Mesh->SetRelativeTransform(FTransform(Rotation, Offset, Scale));
    if (auto* Material = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0)))
    {
        Material->SetScalarParameterValue(TEXT("EffectAge"), Frame.Age);
        Material->SetScalarParameterValue(TEXT("Progress"), Frame.Progress);
        Material->SetScalarParameterValue(TEXT("Opacity"), Frame.Opacity);
        Material->SetScalarParameterValue(TEXT("Flash"), Frame.Flash);
        Material->SetScalarParameterValue(TEXT("Phase"), Frame.Phase);
    }
}

FVector CircleScale(float Radius)
{
    return FVector(Radius / 50.f, Radius / 50.f, 1.f);
}
}

void ARogueBossAttackActor::UpdateVisuals(float Age)
{
    if (!bPresentationReady) return;
    const auto& Settings = Payload.Settings;
    const float Radius = FMath::Max(10.f, Settings.Radius);
    const float Warmup = FMath::Clamp(Age / FMath::Max(.08f, Payload.WarmupSeconds), 0.f, 1.f);
    const float Release = Payload.bReleased ? FMath::Exp(-Age * 9.f) : 0.f;
    const FVector Origin = FVector::ZeroVector;
    const FRotator Level = FRotator::ZeroRotator;

    switch (Payload.Shape)
    {
    case ERogueBossAttackShape::RollingBall:
    {
        const FVector Aim = Payload.Direction.GetSafeNormal2D();
        const FRotator Heading = Aim.Rotation();
        const FVector Axis = FVector::CrossProduct(FVector::UpVector, Aim).GetSafeNormal();
        const FRotator Roll = FQuat(Axis, Age * Settings.Speed / Radius).Rotator();
        const float Form = Payload.bReleased ? 1.f : FMath::Lerp(.32f, 1.f, FMath::SmoothStep(0.f, 1.f, Warmup));
        const FVector CoreScale(Radius / 50.f * Form);
        const FVector Lift(0, 0, Payload.bReleased ? 0.f : FMath::Sin(Age * 7.f) * 8.f);
        Layer(Body, Lift, BaseBodyScale * CoreScale, Payload.bReleased ? Roll : FRotator(0, Age * 75.f, 0),
            {Age, Warmup, 1.f, Release});
        const FVector FlameAxis = Payload.bReleased ? -Aim : FVector::UpVector;
        const FRotator FlameHeading = FQuat::FindBetweenNormals(FVector::UpVector, FlameAxis).Rotator();
        const float FlameLength = Radius * (Payload.bReleased ? 3.3f : 1.9f);
        Layer(Glow, Lift + FlameAxis * FlameLength * .47f,
            BaseGlowScale * FVector(Radius / 50.f * Form, Radius / 50.f * Form, FlameLength / 100.f * Form), FlameHeading,
            {Age, Warmup, .88f, Release});
        if (Payload.bReleased && Warning->IsVisible())
            ARogueGroundEffectFade::CaptureComponents(GetWorld(),{Warning.Get()},.4f);
        Layer(Warning, FVector(0, 0, 8.f - Radius), CircleScale(Radius * 1.18f), Heading,
            {Age, Warmup, Payload.bReleased ? 0.f : 1.f});
        Layer(Trail, -Aim * (Radius * 2.25f) + FVector(0, 0, 12.f - Radius),
            FVector(Radius / 50.f * 3.6f, Radius / 50.f * .86f, 1.f), Heading,
            {Age, Warmup, Payload.bReleased ? .94f : 0.f, Release});
        Layer(Accent, Lift, CoreScale * (1.19f + Release * .18f), FRotator(Age * 17.f, Age * -33.f, 0),
            {Age, Warmup, .39f, Release});
        break;
    }
    case ERogueBossAttackShape::Beam:
    {
        FVector Direction = Payload.Direction;
        if (!Payload.bReleased && IsValid(Payload.AimTarget))
        {
            const FVector Aim = (Payload.AimTarget->GetActorLocation() - Payload.Origin).GetSafeNormal2D();
            if (!Aim.IsNearlyZero()) Direction = Aim;
        }
        const float Length = Payload.bReleased ? Payload.ReleasedBeamLength : ResolveBeamLength(Direction);
        const float Width = Settings.BeamHalfWidth;
        const FRotator AlongBeam = FQuat::FindBetweenNormals(FVector::UpVector, Direction).Rotator();
        const FVector Mid = Direction * Length * .5f;
        const float EndFade = Payload.bReleased ? 1.f - .36f * FMath::SmoothStep(Payload.ActiveSeconds - .22f, Payload.ActiveSeconds, Age) : 1.f;
        Layer(Body, Mid, BaseBodyScale * FVector(Width / 50.f * .56f, Width / 50.f * .56f, Length / 100.f), AlongBeam,
            {Age, Warmup, Payload.bReleased ? EndFade : 0.f, Release});
        Layer(Glow, Mid, BaseGlowScale * FVector(Width / 50.f, Width / 50.f, Length / 100.f), AlongBeam,
            {Age, Warmup, Payload.bReleased ? EndFade : 0.f, Release});

        // 贴地画出完整危险宽度，和实际射线一起延伸到地图外轮廓。
        const FVector Axis = Direction.GetSafeNormal2D();
        const float FlatLength = Length * Direction.Size2D();
        const FVector GroundOffset(0, 0, VisualGroundZ - Payload.Origin.Z + 9.f);
        Layer(Warning, Axis * FlatLength * .5f + GroundOffset,
            FVector(FlatLength / 100.f, Width / 50.f, 1.f), Axis.Rotation(),
            {Age, Warmup, Payload.bReleased ? FMath::Lerp(1.f,.27f,FMath::SmoothStep(0.f,.25f,Age)) : 1.f, Release, Payload.bReleased ? 1.f : 0.f});
        const float Lens = Width / 50.f * (Payload.bReleased ? 1.58f + Release * .62f : .30f + Warmup * .95f);
        Layer(Accent, Direction * 35.f, FVector(Lens), AlongBeam, {Age, Warmup, EndFade, Release});
        Layer(Trail, Direction * FMath::Max(0.f, Length - Width * .25f), FVector(Width / 42.f), Direction.Rotation(),
            {Age, Warmup, Payload.bReleased ? .91f * EndFade : 0.f, Release});
        break;
    }
    case ERogueBossAttackShape::FlameWave:
    {
        const FVector Aim = Payload.Direction.GetSafeNormal2D();
        const FRotator Heading = Aim.Rotation();
        const float Growth=GetFlameWaveScale(GetActorLocation());
        const float Width=Settings.WaveHalfWidth / 50.f * Growth;
        Layer(Body, Origin, BaseBodyScale * FVector(1.75f*Growth, Width, Settings.WaveHalfHeight/50.f*Growth), Heading, {Age, 0.f, .86f, Release});
        Layer(Glow, FVector(0, 0, 10.f-GetMovingAttackHeight(GetActorLocation())), BaseGlowScale * FVector(1.9f*Growth, Width, 1), Heading,
            {Age, 0.f, .12f, Release});
        // 刀波保留单一弯月轮廓，移除地面交叉光网，避免连续释放时叠满画面。
        Trail->SetVisibility(false);
        Warning->SetVisibility(false);
        Accent->SetVisibility(false);
        break;
    }
    case ERogueBossAttackShape::Meteor:
    {
        const float Warn = FMath::Max(.4f, Settings.WarningSeconds);
        const float Impact = FMath::Max(0.f, Age - Warn);
        const bool bBefore = Age < Warn;
        const float Progress = FMath::Clamp(Age / Warn, 0.f, 1.f);
        const float Seed = FMath::Frac(FMath::Abs(float(Payload.Origin.X) * .0017f + float(Payload.Origin.Y) * .0011f));
        const float FallTime = FMath::Min(Warn, FMath::Lerp(.81f, 1.03f, Seed));
        const float Fall = FMath::Clamp((Age - (Warn - FallTime)) / FallTime, 0.f, 1.f);
        const float Height = FMath::Max(300.f, Settings.FallHeight) * (1.f - Fall * Fall);
        const float BurnFade = 1.f - FMath::SmoothStep(FMath::Max(0.f, Settings.FireSeconds - .65f), Settings.FireSeconds, Impact);
        const float Flash = bBefore ? 0.f : FMath::Exp(-Impact * 9.f);
        const FVector TailAxis = Payload.Direction.Z < -.2f ? -Payload.Direction : FVector::UpVector;
        const FVector BallOffset = TailAxis * (Height / FMath::Max(.2f, float(TailAxis.Z))) + FVector(0, 0, Radius * .43f);
        Layer(Body, BallOffset, BaseBodyScale * FVector(Radius / 61.f),
            FRotator(Age * 73.f, Age * 93.f, 0), {Age, Progress, bBefore ? 1.f : 0.f});
        const float TailLength = Radius * (3.1f + 4.9f * Fall);
        const FRotator TailRotation = FQuat::FindBetweenNormals(FVector::UpVector, TailAxis).Rotator();
        Layer(Glow, bBefore ? BallOffset + TailAxis * (TailLength * .44f) : FVector(0, 0, Radius * .35f),
            BaseGlowScale * FVector(Radius / 49.f, Radius / 49.f, (bBefore ? TailLength : Radius * (2.1f + Flash * 2.1f)) / 100.f),
            bBefore ? TailRotation : FRotator(0, Age * 36.f, 0),
            {Age, Progress, bBefore ? .96f : BurnFade * .67f, Flash, bBefore ? 0.f : 1.f});
        Layer(Warning, FVector(0, 0, 9.f), CircleScale(Radius), Level,
            {Age, bBefore ? Progress : FMath::Clamp(Impact / .38f, 0.f, 1.f), bBefore ? 1.f : 1.f-FMath::SmoothStep(0.f,.55f,Impact), Flash, bBefore ? 0.f : 1.f});
        if (bBefore)
        {
            FVector Across = FVector::CrossProduct(FVector::UpVector, TailAxis).GetSafeNormal();
            if (Across.IsNearlyZero()) Across = FVector::RightVector;
            const FRotator StreakRotation = FRotationMatrix::MakeFromXY(TailAxis, Across).Rotator();
            Layer(Trail, BallOffset + TailAxis * (TailLength * .53f), FVector(TailLength / 73.f, Radius / 25.f, 1.f),
                StreakRotation, {Age + Seed, Progress, FMath::SmoothStep(0.f, .19f, Age), 0.f, 2.f});
        }
        else
        {
            Layer(Trail, FVector(0, 0, 8.f), CircleScale(Radius), Level, {Impact, 0.f, BurnFade, Flash});
        }
        Layer(Accent, FVector(0, 0, 14.f), CircleScale(Radius * (1.f + .38f * FMath::Clamp(Impact * 3.f, 0.f, 1.f))), Level,
            {Impact, FMath::Clamp(Impact / .42f, 0.f, 1.f), bBefore ? 0.f : FMath::Exp(-Impact * 6.5f), Flash, 1.f});
        break;
    }
    case ERogueBossAttackShape::IceSpike:
    {
        const float Warn = FMath::Max(.4f, Settings.WarningSeconds);
        const float Impact = FMath::Max(0.f, Age - Warn);
        const bool bBefore = Age < Warn;
        const float Rise = FMath::SmoothStep(0.f, .075f, Impact)
            * (1.f + .12f * FMath::Exp(-Impact * 8.f) * FMath::Sin(Impact * 26.f));
        const float Fade = 1.f - FMath::SmoothStep(.37f, .79f, Impact);
        const float Progress = FMath::Clamp(Age / Warn, 0.f, 1.f);
        const float Flash = bBefore ? 0.f : FMath::Exp(-Impact * 12.f);
        const FBossFxFrame Crystal{Age, Progress, bBefore ? 0.f : Fade * Rise, Flash};
        const float Height = (230.f + Radius * 1.15f) * Rise;
        Layer(Body, FVector(0, 0, Height * .48f), BaseBodyScale * FVector(Radius / 83.f, Radius / 83.f, Height / 100.f),
            FRotator(9, 22, -8), Crystal);
        Layer(Glow, FVector(Radius * .4f, 0, Height * .29f), BaseGlowScale * FVector(Radius / 155.f, Radius / 155.f, Height / 175.f),
            FRotator(29, 18, 19), Crystal);
        Layer(Accent, FVector(-Radius * .43f, Radius * .12f, Height * .24f), FVector(Radius / 169.f, Radius / 169.f, Height / 198.f),
            FRotator(-29, 90, -15), Crystal);
        Layer(Warning, FVector(0, 0, 10.f), CircleScale(Radius), Level,
            {Age, bBefore ? Progress : FMath::Clamp(Impact / .32f, 0.f, 1.f), bBefore ? 1.f : 1.f-FMath::SmoothStep(0.f,.55f,Impact), Flash, bBefore ? 0.f : 1.f});
        Layer(Trail, FVector(0, 0, 7.f), CircleScale(Radius), FRotator(0, 23, 0), {Impact, 0.f, bBefore ? 0.f : Fade, Flash});
        break;
    }
    case ERogueBossAttackShape::HealAura:
    case ERogueBossAttackShape::GoldenWard:
    {
        if (const auto* Enemy = Cast<ARogueEnemy>(GetOwner())) SetActorLocation(Enemy->GetActorLocation());
        const bool bWard = Payload.Shape == ERogueBossAttackShape::GoldenWard;
        const float Intro = FMath::SmoothStep(0.f, .16f, Age);
        const float Fade = Intro * (1.f - .62f * FMath::SmoothStep(Payload.ActiveSeconds - .28f, Payload.ActiveSeconds, Age));
        const float Pulse = .5f + .5f * FMath::Sin(Age * (bWard ? 5.f : 8.f));
        const FVector FloorOffset(0, 0, VisualGroundZ - GetActorLocation().Z + 9.f);
        Layer(Warning, FloorOffset, CircleScale(Radius * 1.16f), FRotator(0, Age * (bWard ? -21.f : 33.f), 0),
            {Age, Pulse, Fade, 0.f});
        const FVector WardSize(Radius / 31.f, Radius / 31.f, Radius / 24.f);
        Layer(Body, FVector(0, 0, Radius * .48f), BaseBodyScale * WardSize * (1.f + .02f * Pulse),
            FRotator(0, Age * 29.f, 0), {Age, Pulse, Fade * (bWard ? .95f : .65f), 1.f - Intro});
        Layer(Glow, FVector(0, 0, Radius * .65f), BaseGlowScale * FVector(Radius / 48.f, Radius / 48.f, Radius / (bWard ? 65.f : 24.f)),
            FRotator(0, -Age * 26.f, 0), {Age, Pulse, Fade * (bWard ? .83f : .9f), 1.f - Intro});
        Layer(Trail, FloorOffset + FVector(0, 0, 2.f), CircleScale(Radius * 1.37f), FRotator(0, -Age * 19.f, 0),
            {Age, Pulse, Fade * .68f, 1.f - Intro});
        Layer(Accent, FVector(0, 0, Radius * 1.9f), CircleScale(Radius * 1.36f), FRotator(23, Age * 31.f, 0),
            {Age, Pulse, Fade * .85f, 1.f - Intro});
        break;
    }
    }
}

void ARogueBossAttackActor::Multicast_WallImpact_Implementation(FVector_NetQuantize Location, FVector_NetQuantizeNormal Direction)
{
    if (GetNetMode() == NM_DedicatedServer || !WallImpactClass) return;
    auto* Pool = GetWorld()->GetSubsystem<URogueCombatPoolSubsystem>();
    if (!Pool) return;
    FRogueCombatAccentSettings AccentSettings;
    AccentSettings.EffectClass = WallImpactClass;
    const bool bWave = Payload.Shape == ERogueBossAttackShape::FlameWave;
    AccentSettings.Tint = bWave ? FLinearColor(1.f, .22f, .07f) : FLinearColor(1.f, .45f, .10f);
    AccentSettings.Scale = bWave ? 2.7f : 3.4f;
    AccentSettings.Duration = .43f;
    AccentSettings.ShardCount = 6;
    Pool->PlayCombatAccent(AccentSettings, Location, Direction, true);
}
