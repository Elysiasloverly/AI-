#include "World/RogueShopTerminal.h"

#include "Player/RogueCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"

ARogueShopTerminal::ARogueShopTerminal()
{
	PrimaryActorTick.bCanEverTick = false;

	// 终端由关卡摆放；复制支持服务端交互与客户端本地提示。
	// 实际购买/报价仍由服务端 PlayerState/GameMode 入口校验。
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
	BaseMesh->SetupAttachment(SceneRoot);
	BaseMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	BaseMesh->SetCastShadow(false);

	GlowMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GlowMesh"));
	GlowMesh->SetupAttachment(SceneRoot);
	GlowMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	GlowMesh->SetCastShadow(false);

	InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
	InteractionBox->SetupAttachment(SceneRoot);
	InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractionBox->SetGenerateOverlapEvents(true);
	InteractionBox->SetBoxExtent(FVector(150.0f, 150.0f, 150.0f));

    // 模型、材质、缩放由 BP_RogueShopTerminal 的组件默认值配置，运行时不覆写。
    PromptAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("PromptAnchor"));
    PromptAnchor->SetupAttachment(SceneRoot);
    PromptAnchor->SetRelativeLocation(FVector(0.0f, 0.0f, 220.0f));
}

void ARogueShopTerminal::BeginPlay()
{
	Super::BeginPlay();

	InteractionBox->OnComponentBeginOverlap.AddDynamic(this, &ARogueShopTerminal::HandleOverlapBegin);
	InteractionBox->OnComponentEndOverlap.AddDynamic(this, &ARogueShopTerminal::HandleOverlapEnd);
}

FVector ARogueShopTerminal::GetPromptWorldLocation() const
{
	return PromptAnchor->GetComponentLocation();
}

void ARogueShopTerminal::HandleOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (ARogueCharacter* PlayerCharacter = Cast<ARogueCharacter>(OtherActor))
	{
		PlayerCharacter->SetNearbyShopTerminal(this);
	}
}

void ARogueShopTerminal::HandleOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (ARogueCharacter* PlayerCharacter = Cast<ARogueCharacter>(OtherActor))
	{
		PlayerCharacter->ClearNearbyShopTerminal(this);
	}
}
