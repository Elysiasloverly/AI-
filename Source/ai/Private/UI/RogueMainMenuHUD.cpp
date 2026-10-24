#include "UI/RogueMainMenuHUD.h"
#include "Networking/RogueRoomSubsystem.h"

#include "Camera/CameraActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

ARogueMainMenuHUD::ARogueMainMenuHUD()
{
	SetMainMenuMode(true);
}

void ARogueMainMenuHUD::BeginPlay()
{
	Super::BeginPlay();
	if (URogueRoomSubsystem* Rooms=URogueRoomSubsystem::GetForPlayer(GetOwningPlayerController())) Rooms->MainMenuEntered();
	ApplyMenuCamera();
}

void ARogueMainMenuHUD::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 相机可能晚于 HUD 生成；找到之前每帧重试一次。
	if (!bMenuCameraApplied)
	{
		ApplyMenuCamera();
	}
}

void ARogueMainMenuHUD::ApplyMenuCamera()
{
	if (bMenuCameraApplied)
	{
		return;
	}

	APlayerController* PlayerController = GetOwningPlayerController();
	UWorld* World = GetWorld();
	if (PlayerController == nullptr || World == nullptr)
	{
		return;
	}

	for (TActorIterator<ACameraActor> It(World); It; ++It)
	{
		if (It->ActorHasTag(TEXT("MenuCamera")))
		{
			PlayerController->SetViewTarget(*It);
			bMenuCameraApplied = true;
			UE_LOG(LogTemp, Log, TEXT("[Rogue] 主菜单背景相机：%s"), *GetNameSafe(*It));
			return;
		}
	}
}
