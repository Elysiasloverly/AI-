#include "Combat/Projectiles/RogueIceFlameField.h"
#include "Combat/Visuals/RogueGroundEffectFade.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Enemies/RogueEnemy.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ARogueIceFlameField::ARogueIceFlameField()
{
    bReplicates=true;NetUpdateFrequency=5;MinNetUpdateFrequency=2;
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.bStartWithTickEnabled=false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
    auto MakePlane=[&](const TCHAR* Name){
        auto* C=CreateDefaultSubobject<UStaticMeshComponent>(Name);C->SetupAttachment(RootComponent);
        C->SetStaticMesh(Plane.Object);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        C->SetGenerateOverlapEvents(false);C->SetCanEverAffectNavigation(false);C->SetCastShadow(false);C->bReceivesDecals=false;return C;
    };
    Ground=MakePlane(TEXT("GroundSeal"));
    for(int32 I=0;I<8;++I)Flames.Add(MakePlane(*FString::Printf(TEXT("BlueFlame%d"),I)));
    SetActorEnableCollision(false);SetActorHiddenInGame(true);
}

float ARogueIceFlameField::Clock() const
{
    const auto* GS=GetWorld()?GetWorld()->GetGameState():nullptr;
    return GS?GS->GetServerWorldTimeSeconds():GetWorld()->GetTimeSeconds();
}

void ARogueIceFlameField::Activate(AActor* Source,const FVector& Location,const FVector& Normal,float Radius,float DamagePerSecond,const FRogueIceFlameCombatSettings& Settings)
{
    if(GetNetMode()==NM_Client || !HasAuthority())return;
    const bool bFresh=!View.bActive;
    if(bFresh){ReleaseSlows();LastDamageTime=Clock();View.StartTime=Clock();}
    SetOwner(Source);Combat=Settings;
    DPS=bFresh?FMath::Max(0.f,DamagePerSecond):FMath::Max(DPS,DamagePerSecond);
    View.Radius=bFresh?Radius:FMath::Max(View.Radius,Radius);
    View.Location=Location;View.Normal=Normal.GetSafeNormal();View.EndTime=Clock()+FMath::Max(.1f,Settings.Lifetime);
    View.bActive=true;++View.Sequence;OnRep_View();ForceNetUpdate();
}

void ARogueIceFlameField::SetActorHiddenInGame(bool bNewHidden)
{
    if (bNewHidden && !IsHidden()) ARogueGroundEffectFade::CaptureActor(this);
    Super::SetActorHiddenInGame(bNewHidden);
}
void ARogueIceFlameField::OnRep_View()
{
    SetActorHiddenInGame(!View.bActive);SetActorTickEnabled(View.bActive);
    SetActorTickInterval(GetNetMode()==NM_DedicatedServer?.1f:.05f);
    if(!View.bActive)return;
    SetActorLocationAndRotation(View.Location,FRotationMatrix::MakeFromZ(View.Normal).Rotator());
    if(GetNetMode()==NM_DedicatedServer)return;
    if(!GroundMID&&GroundMaterial){GroundMID=UMaterialInstanceDynamic::Create(GroundMaterial,this);Ground->SetMaterial(0,GroundMID);}
    if(!FlameMID&&FlameMaterial){FlameMID=UMaterialInstanceDynamic::Create(FlameMaterial,this);for(UStaticMeshComponent* C:Flames)C->SetMaterial(0,FlameMID);}
    Ground->SetRelativeLocation(FVector(0,0,5));Ground->SetRelativeScale3D(FVector(View.Radius*2/100,View.Radius*2/100,1));
    const int32 Count=FMath::Clamp(FlameCount,1,8);
    for(int32 I=0;I<Flames.Num();++I)
    {
        UStaticMeshComponent* C=Flames[I];C->SetVisibility(I<Count);
        const float Angle=I*2.399963f;
        const float Reach=I==0?0:View.Radius*(.25f+.46f*FMath::Sqrt(float(I)/Count));
        C->SetRelativeLocation(FVector(FMath::Cos(Angle)*Reach,FMath::Sin(Angle)*Reach,FlameHeight*.5f+7));
        C->SetRelativeRotation(FRotator(0,FMath::RadiansToDegrees(Angle),90));
        C->SetRelativeScale3D(FVector(View.Radius*.70f/100,FlameHeight/100,1));
    }
    UpdateVisuals();
}

void ARogueIceFlameField::UpdateVisuals()
{
    const float Age=FMath::Max(0.f,Clock()-View.StartTime),Remaining=View.EndTime-Clock();
    const float Fade=FMath::SmoothStep(0.f,.16f,Age)*FMath::SmoothStep(0.f,.6f,Remaining);
    if(GroundMID)GroundMID->SetScalarParameterValue(TEXT("Opacity"),Fade);
    if(FlameMID)FlameMID->SetScalarParameterValue(TEXT("Opacity"),Fade);
}

void ARogueIceFlameField::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);if(!View.bActive)return;
    const float Now=Clock();
    if(GetNetMode()!=NM_DedicatedServer)UpdateVisuals();
    if(GetNetMode()==NM_Client)return; // The replicated field never applies client damage or movement status.
    if(!IsValid(GetOwner())){Deactivate();return;}
    const float Until=FMath::Min(Now,View.EndTime);
    const bool bDamageDue=Until-LastDamageTime>=FMath::Max(.1f,Combat.DamageInterval) || Now>=View.EndTime;
    const float Amount=bDamageDue?DPS*FMath::Max(0.f,Until-LastDamageTime):0;
    if(bDamageDue)LastDamageTime=Until;
    TMap<TWeakObjectPtr<ARogueEnemy>,uint32> Inside;
    if(auto* Tracker=GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>())
    {
        TArray<ARogueEnemy*> Enemies;Tracker->CollectEnemiesInRange(View.Location,View.Radius,Enemies,0,false);
        for(auto* Enemy:Enemies)
        {
            if(!IsValid(Enemy)||Enemy->IsDead())continue;
            const FVector Feet=Enemy->GetActorLocation()-FVector::UpVector*Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
            if(FMath::Abs(Feet.Z-View.Location.Z)>Combat.HeightTolerance || FVector::DistSquared2D(Feet,View.Location)>FMath::Square(View.Radius))continue;
            Inside.Add(Enemy,Enemy->GetPresentationGeneration());
            Enemy->RefreshMovementSlow(this,Combat.SlowFraction,.3f);
            if(Amount>0)RogueAbilitySystem::ApplyDamageToTarget(GetOwner(),Enemy,Amount,RogueGameplayTags::Damage_Source_Area);
        }
    }
    for(const auto& Pair:SlowedEnemies)
        if(auto* E=Pair.Key.Get();E&&!Inside.Contains(E)&&E->GetPresentationGeneration()==Pair.Value)E->ClearMovementSlow(this);
    SlowedEnemies=MoveTemp(Inside);
    if(Now>=View.EndTime)Deactivate();
}

void ARogueIceFlameField::ReleaseSlows()
{
    for(const auto& Pair:SlowedEnemies)
        if(auto* E=Pair.Key.Get();E&&E->GetPresentationGeneration()==Pair.Value)E->ClearMovementSlow(this);
    SlowedEnemies.Reset();
}
void ARogueIceFlameField::Deactivate()
{
    if(GetNetMode()==NM_Client)return;
    ReleaseSlows();View.bActive=false;++View.Sequence;OnRep_View();SetOwner(nullptr);ForceNetUpdate();
}
void ARogueIceFlameField::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Reason==EEndPlayReason::Destroyed) ARogueGroundEffectFade::CaptureActor(this);
    ReleaseSlows();Super::EndPlay(Reason);
}
void ARogueIceFlameField::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(ARogueIceFlameField,View);}
