#include "Networking/RogueLobby.h"
#include "Networking/RogueRoomSubsystem.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/HUD.h"
#include "Net/UnrealNetwork.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Camera/CameraActor.h"
#include "EngineUtils.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"

void ARogueLobbyPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARogueLobbyPlayerState,bReady);
	DOREPLIFETIME(ARogueLobbyPlayerState,bHost);
}
void ARogueLobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARogueLobbyGameState,RoomName);
	DOREPLIFETIME(ARogueLobbyGameState,HostAddress);
	DOREPLIFETIME(ARogueLobbyGameState,Capacity);
	DOREPLIFETIME(ARogueLobbyGameState,bStarting);
}
bool ARogueLobbyGameState::AreAllPlayersReady() const
{
	int32 Participants=0;
	for (APlayerState* Entry : PlayerArray)
	{
		const ARogueLobbyPlayerState* State=Cast<ARogueLobbyPlayerState>(Entry);
		if (!State || State->IsInactive() || State->IsOnlyASpectator()) continue;
		++Participants;
		if (!State->bReady) return false;
	}
	return Participants>0;
}
void ARogueLobbyPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController()) return;
	for (TActorIterator<ACameraActor> It(GetWorld());It;++It)
	{
		if (It->ActorHasTag(TEXT("MenuCamera"))) { SetViewTarget(*It); break; }
	}
	if (URogueRoomSubsystem* Rooms=URogueRoomSubsystem::GetForPlayer(this))
	{
		Rooms->EnterLobby(this); ServerSetDisplayName(Rooms->GetPlayerDisplayName());
	}
}
void ARogueLobbyPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
	if (URogueRoomSubsystem* Rooms=URogueRoomSubsystem::GetForPlayer(this)) Rooms->DismissForPlayer(this);
	Super::EndPlay(Reason);
}
void ARogueLobbyPlayerController::ServerSetReady_Implementation(bool bNewReady)
{
	if (!HasAuthority()) return;
	const ARogueLobbyGameState* Lobby=GetWorld()->GetGameState<ARogueLobbyGameState>();
	if (Lobby && !Lobby->bStarting)
	{
		if (ARogueLobbyPlayerState* State=GetPlayerState<ARogueLobbyPlayerState>())
		{
			State->bReady=bNewReady; State->ForceNetUpdate();
		}
	}
}
void ARogueLobbyPlayerController::ServerSetDisplayName_Implementation(const FString& Name)
{
	if (!HasAuthority()) return;
	if (ARogueLobbyPlayerState* State=GetPlayerState<ARogueLobbyPlayerState>())
	{
		State->SetPlayerName(URogueRoomSubsystem::CleanName(Name)); State->ForceNetUpdate();
	}
}
void ARogueLobbyPlayerController::ServerStartGame_Implementation()
{
	if (ARogueLobbyGameMode* Mode=GetWorld()->GetAuthGameMode<ARogueLobbyGameMode>()) Mode->TryStartGame(this);
}
ARogueLobbyGameMode::ARogueLobbyGameMode()
{
	GameStateClass=ARogueLobbyGameState::StaticClass();
	PlayerStateClass=ARogueLobbyPlayerState::StaticClass();
	PlayerControllerClass=ARogueLobbyPlayerController::StaticClass();
	DefaultPawnClass=nullptr;
	HUDClass=AHUD::StaticClass();
	bUseSeamlessTravel=true;
}
void ARogueLobbyGameMode::InitGame(const FString& MapName,const FString& Options,FString& Error)
{
	Super::InitGame(MapName,Options,Error);
	if (GameSession && GetGameInstance())
	{
		GameSession->MaxPlayers=GetGameInstance()->GetSubsystem<URogueRoomSubsystem>()->GetHostedCapacity();
	}
}
void ARogueLobbyGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (ARogueLobbyGameState* Lobby=GetGameState<ARogueLobbyGameState>())
	{
		const URogueRoomSubsystem* Rooms=GetGameInstance()->GetSubsystem<URogueRoomSubsystem>();
		Lobby->RoomName=Rooms->GetHostedRoomName(); Lobby->Capacity=Rooms->GetHostedCapacity();
		if (ISocketSubsystem* Sockets=ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
		{
			bool bCanBindAll=false;
			TSharedRef<FInternetAddr> Address=Sockets->GetLocalHostAddr(*GLog,bCanBindAll);
			Lobby->HostAddress=Address->ToString(false)+FString::Printf(TEXT(":%d"),GetWorld()->URL.Port);
		}
	}
}
void ARogueLobbyGameMode::PreLogin(const FString& Options,const FString& Address,const FUniqueNetIdRepl& ID,FString& Error)
{
	Super::PreLogin(Options,Address,ID,Error);
	if (const ARogueLobbyGameState* Lobby=GetGameState<ARogueLobbyGameState>())
	{
		if (Lobby->bStarting) Error=TEXT("房主已开始游戏，请等待下一局。");
		else if (GetNumPlayers()>=Lobby->Capacity) Error=TEXT("房间已满。");
	}
}
void ARogueLobbyGameMode::PostLogin(APlayerController* Player)
{
	Super::PostLogin(Player);
	if (ARogueLobbyPlayerState* State=Player->GetPlayerState<ARogueLobbyPlayerState>())
	{
		State->bHost=Player->IsLocalController();
		State->bReady=false;
		if (State->bHost) State->SetPlayerName(GetGameInstance()->GetSubsystem<URogueRoomSubsystem>()->GetPlayerDisplayName());
		State->ForceNetUpdate();
	}
}
bool ARogueLobbyGameMode::TryStartGame(APlayerController* Requester)
{
	ARogueLobbyGameState* Lobby=GetGameState<ARogueLobbyGameState>();
	const ARogueLobbyPlayerState* Player=Requester?Requester->GetPlayerState<ARogueLobbyPlayerState>():nullptr;
	if (!Requester || !Requester->IsLocalController() || !Player || !Player->bHost || !Lobby || Lobby->bStarting || !Lobby->AreAllPlayersReady()) return false;
	URogueRoomSubsystem* Rooms=GetGameInstance()->GetSubsystem<URogueRoomSubsystem>();
	Lobby->bStarting=true; Lobby->ForceNetUpdate();
	// Clear the lobby's ?game override, but keep this player hosting the battle.
	// PIE must also enable net.AllowPIESeamlessTravel (DefaultEngine.ini) to retain clients.
	FURL BattleURL(nullptr,*Rooms->BattleMap,TRAVEL_Absolute);
	BattleURL.AddOption(TEXT("listen"));
	UE_LOG(LogTemp,Log,TEXT("[RogueLobby] Starting battle with %d players: %s (WorldType=%d)"),GetNumPlayers(),*BattleURL.ToString(),static_cast<int32>(GetWorld()->WorldType));
	if (!GetWorld()->ServerTravel(BattleURL.ToString(),true)) { Lobby->bStarting=false; return false; }
	Rooms->MarkMatchStarted();
	return true;
}
