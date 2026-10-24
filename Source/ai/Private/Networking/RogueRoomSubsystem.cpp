#include "Networking/RogueRoomSubsystem.h"
#include "Algo/AllOf.h"
#include "Networking/RogueLobby.h"
#include "UI/Widgets/RogueMultiplayerWidgetBase.h"
#include "UI/RogueHUD.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSessionSettings.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "Engine/PendingNetGame.h"
#include "Kismet/GameplayStatics.h"
#include "IPAddress.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "GameFramework/GameModeBase.h"

namespace { const FName RoomNameKey(TEXT("BAKA_ROOM_NAME")), GameKey(TEXT("BAKA_GAME")); }
void URogueRoomSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	PreLoginHandle=FGameModeEvents::OnGameModePreLoginEvent().AddUObject(this,&ThisClass::CheckIncomingPlayer);
	if (GEngine)
	{
		NetworkHandle=GEngine->OnNetworkFailure().AddUObject(this,&ThisClass::NetworkFailed);
		TravelHandle=GEngine->OnTravelFailure().AddUObject(this,&ThisClass::TravelFailed);
	}
}
void URogueRoomSubsystem::Deinitialize()
{
	FTSTicker::RemoveTicker(ConnectTimeout);
	FGameModeEvents::OnGameModePreLoginEvent().Remove(PreLoginHandle);
	if (GEngine) { GEngine->OnNetworkFailure().Remove(NetworkHandle); GEngine->OnTravelFailure().Remove(TravelHandle); }
	if (SessionInterface.IsValid())
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
	}
	// PIE tears down its online provider before these UObject members are destroyed by GC.
	// Release shared session ownership during subsystem shutdown, after removing callbacks.
	Search.Reset();
	SessionInterface.Reset();
	Super::Deinitialize();
}
URogueRoomSubsystem* URogueRoomSubsystem::GetForPlayer(APlayerController* Player)
{
	return Player && Player->GetGameInstance()?Player->GetGameInstance()->GetSubsystem<URogueRoomSubsystem>():nullptr;
}
IOnlineSessionPtr URogueRoomSubsystem::Sessions()
{
	if (!SessionInterface.IsValid()) SessionInterface=Online::GetSessionInterface(GetWorld());
	return SessionInterface;
}
void URogueRoomSubsystem::SetStatus(const FString& Message,bool bNewBusy)
{
	Status=FText::FromString(Message); bBusy=bNewBusy; OnRoomChanged.Broadcast();
}
FString URogueRoomSubsystem::CleanName(const FString& Input)
{
	FString Result=Input.TrimStartAndEnd().Left(24);
	for (const TCHAR* Bad : {TEXT("?"),TEXT("&"),TEXT("="),TEXT("#"),TEXT("/"),TEXT("\\"),TEXT("\r"),TEXT("\n"),TEXT("\t")}) Result.ReplaceInline(Bad,TEXT(""));
	Result.ReplaceInline(TEXT(" "),TEXT("_"));
	return Result.IsEmpty()?TEXT("玩家"):Result;
}
void URogueRoomSubsystem::HostRoom(const FString& Name,const FString& PlayerName,int32 Capacity)
{
	if (bBusy || bInRoom) return;
	if (!Sessions().IsValid()) { SetStatus(TEXT("局域网服务未加载，请检查 OnlineSubsystemNull 配置。")); return; }
	DisplayName=CleanName(PlayerName);
	HostedRoomName=Name.TrimStartAndEnd().Left(40);
	if (HostedRoomName.IsEmpty()) HostedRoomName=DisplayName+TEXT("的房间");
	HostedCapacity=FMath::Clamp(Capacity,2,8); bLeaving=false;
	SetStatus(TEXT("正在创建房间…"),true);
	if (Sessions()->GetNamedSession(NAME_GameSession)) DestroySession(true);
	else BeginCreate();
}
void URogueRoomSubsystem::BeginCreate()
{
	FOnlineSessionSettings Settings;
	Settings.bIsLANMatch=true; Settings.bShouldAdvertise=true;
	Settings.NumPublicConnections=HostedCapacity; Settings.NumPrivateConnections=0;
	Settings.bAllowJoinInProgress=false; Settings.bUsesPresence=false;
	Settings.Set(RoomNameKey,HostedRoomName,EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(GameKey,FString(TEXT("BakaRoomV1")),EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	CreateHandle=Sessions()->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(this,&ThisClass::OnCreated));
	if (!Sessions()->CreateSession(0,NAME_GameSession,Settings)) OnCreated(NAME_GameSession,false);
}
void URogueRoomSubsystem::OnCreated(FName Name,bool bSuccess)
{
	Sessions()->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle); CreateHandle.Reset();
	if (!bSuccess) { SetStatus(TEXT("创建房间失败，请重试或检查网络端口。")); return; }
	bWaitingForLobby=true;
	UGameplayStatics::OpenLevel(this,FName(*LobbyMap),true,TEXT("listen?game=/Script/ai.RogueLobbyGameMode"));
}
void URogueRoomSubsystem::FindRooms()
{
	if (bBusy || bInRoom) return;
	if (!Sessions().IsValid()) { SetStatus(TEXT("局域网服务未加载。")); return; }
	Rooms.Reset(); Search=MakeShared<FOnlineSessionSearch>(); Search->bIsLanQuery=true; Search->MaxSearchResults=100;
	SetStatus(TEXT("正在搜索同一局域网内的房间…"),true);
	FindHandle=Sessions()->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateUObject(this,&ThisClass::OnFound));
	if (!Sessions()->FindSessions(0,Search.ToSharedRef())) OnFound(false);
}
void URogueRoomSubsystem::OnFound(bool bSuccess)
{
	Sessions()->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle); FindHandle.Reset();
	Rooms.Reset();
	if (bSuccess && Search)
	{
		for (int32 Index=0;Index<Search->SearchResults.Num();++Index)
		{
			const auto& Result=Search->SearchResults[Index]; FString Game;
			if (!Result.IsValid() || !Result.Session.SessionSettings.Get(GameKey,Game) || Game!=TEXT("BakaRoomV1")) continue;
			FRogueRoomInfo& Row=Rooms.AddDefaulted_GetRef(); Row.Index=Index;
			Result.Session.SessionSettings.Get(RoomNameKey,Row.Name);
			Row.Capacity=Result.Session.SessionSettings.NumPublicConnections;
			Row.Players=Row.Capacity-Result.Session.NumOpenPublicConnections;
			Row.Ping=Result.PingInMs;
		}
	}
	SetStatus(!bSuccess?TEXT("搜索失败，请重试。"):(Rooms.IsEmpty()?TEXT("未发现房间。请确认同一局域网，或输入房主 IP。 "):TEXT("选择房间加入；也可以输入房主 IP。")));
}
void URogueRoomSubsystem::JoinRoom(int32 Index,const FString& PlayerName)
{
	if (bBusy || bInRoom) return;
	if (!Search || !Search->SearchResults.IsValidIndex(Index) || !Sessions().IsValid()) { SetStatus(TEXT("房间列表已失效，请重新搜索。")); return; }
	DisplayName=CleanName(PlayerName); bLeaving=false;
	SetStatus(TEXT("正在加入房间…"),true);
	JoinHandle=Sessions()->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(this,&ThisClass::OnJoined));
	if (!Sessions()->JoinSession(0,NAME_GameSession,Search->SearchResults[Index])) OnJoined(NAME_GameSession,EOnJoinSessionCompleteResult::UnknownError);
}
void URogueRoomSubsystem::OnJoined(FName Name,EOnJoinSessionCompleteResult::Type Result)
{
	Sessions()->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle); JoinHandle.Reset();
	FString URL;
	if (Result!=EOnJoinSessionCompleteResult::Success || !Sessions()->GetResolvedConnectString(Name,URL))
	{
		SetStatus(Result==EOnJoinSessionCompleteResult::SessionIsFull?TEXT("房间已满，请选择其他房间。"):TEXT("加入失败，房间可能已关闭或开始游戏。"));
		DestroySession(); return;
	}
	Connect(URL);
}
bool URogueRoomSubsystem::ValidateAddress(const FString& Input,FString& Output)
{
	Output=Input.TrimStartAndEnd();
	FString Host=Output,Port; int32 Split=INDEX_NONE;
	if (Output.FindChar(TEXT(':'),Split))
	{
		Host=Output.Left(Split); Port=Output.Mid(Split+1);
		if (Port.IsEmpty() || !Algo::AllOf(Port, [](TCHAR C) { return C >= '0' && C <= '9'; }) || Port.Len()>5 || FCString::Atoi(*Port)<1 || FCString::Atoi(*Port)>65535) return false;
	}
	FIPv4Address Parsed;
	if (Host.Equals(TEXT("localhost"),ESearchCase::IgnoreCase)) return true;
	for (TCHAR Char : Host) if ((Char<TEXT('0') || Char>TEXT('9')) && Char!=TEXT('.')) return false;
	TArray<FString> Parts; Host.ParseIntoArray(Parts,TEXT("."),false);
	if (Parts.Num()!=4) return false;
	for (const FString& Part : Parts) if (Part.IsEmpty() || Part.Len()>3 || FCString::Atoi(*Part)>255) return false;
	return FIPv4Address::Parse(Host,Parsed);
}
void URogueRoomSubsystem::JoinByAddress(const FString& Address,const FString& PlayerName)
{
	if (bBusy || bInRoom) return;
	FString URL;
	if (!ValidateAddress(Address,URL)) { SetStatus(TEXT("请输入有效 IPv4 地址，可附加端口，例如 192.168.1.10:7777。")); return; }
	DisplayName=CleanName(PlayerName); bLeaving=false;
	SetStatus(TEXT("正在连接房主…"),true); Connect(URL);
}
void URogueRoomSubsystem::Connect(const FString& URL)
{
	APlayerController* Player=GetGameInstance()->GetFirstLocalPlayerController();
	if (!Player) { ConnectionFailed(TEXT("本地玩家尚未就绪，请重试。")); return; }
	bWaitingForLobby=true;
	FTSTicker::RemoveTicker(ConnectTimeout);
	ConnectTimeout=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,[this](float)
	{
		if (bWaitingForLobby) ConnectionFailed(TEXT("连接超时，请检查 IP、房主是否已创建房间及防火墙。"));
		return false;
	}),15.0f);
	Player->ClientTravel(URL+TEXT("?Name=")+DisplayName,TRAVEL_Absolute);
}
void URogueRoomSubsystem::DestroySession(bool bCreateAfter)
{
	bCreateAfterDestroy=bCreateAfter;
	if (!Sessions().IsValid() || !Sessions()->GetNamedSession(NAME_GameSession))
	{
		if (bCreateAfter) BeginCreate();
		return;
	}
	if (DestroyHandle.IsValid()) return;
	DestroyHandle=Sessions()->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateUObject(this,&ThisClass::OnDestroyed));
	if (!Sessions()->DestroySession(NAME_GameSession)) OnDestroyed(NAME_GameSession,false);
}
void URogueRoomSubsystem::OnDestroyed(FName Name,bool bSuccess)
{
	Sessions()->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle); DestroyHandle.Reset();
	const bool bCreate=bCreateAfterDestroy; bCreateAfterDestroy=false;
	if (bCreate) { if (bSuccess) BeginCreate(); else SetStatus(TEXT("旧房间清理失败，请重试。")); }
}
void URogueRoomSubsystem::EnterLobby(APlayerController* Player)
{
	bWaitingForLobby=false; bInRoom=true; bLeaving=false; bMatchStarted=false;
	FTSTicker::RemoveTicker(ConnectTimeout);
	SetStatus(TEXT("点击准备；所有玩家准备后，由房主开始游戏。")); ShowMenu(Player,2);
}
void URogueRoomSubsystem::MarkMatchStarted()
{
	bMatchStarted=true;
	SetStatus(TEXT("正在进入战斗…"),true);
	if (Sessions().IsValid())
	{
		if (FNamedOnlineSession* Session=Sessions()->GetNamedSession(NAME_GameSession))
		{
			Session->SessionSettings.bShouldAdvertise=false;
			Sessions()->UpdateSession(NAME_GameSession,Session->SessionSettings,true);
			Sessions()->StartSession(NAME_GameSession);
		}
	}
}
void URogueRoomSubsystem::LeaveRoom()
{
	if (bLeaving) return;
	bLeaving=true; bInRoom=false; bWaitingForLobby=false; bBusy=false; bMatchStarted=false;
	FTSTicker::RemoveTicker(ConnectTimeout);
	if (Sessions().IsValid())
	{
		Sessions()->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle); CreateHandle.Reset();
		Sessions()->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle); FindHandle.Reset();
		Sessions()->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle); JoinHandle.Reset();
		if (Search) Sessions()->CancelFindSessions();
	}
	SetStatus(TEXT("已返回主菜单。"));
	DestroySession(); CloseMenu(); GetGameInstance()->ReturnToMainMenu();
}
void URogueRoomSubsystem::ConnectionFailed(const FString& Message)
{
	if (bLeaving) return;
	LeaveRoom(); SetStatus(Message);
}
void URogueRoomSubsystem::NetworkFailed(UWorld* World,UNetDriver* Driver,ENetworkFailure::Type Type,const FString& Error)
{
	bool bOurs=World && World->GetGameInstance()==GetGameInstance();
	// Pending connection errors have no World. Match the pending driver to this GameInstance,
	// so PIE clients do not consume one another's failures.
	if (!World && Driver && GEngine && bWaitingForLobby)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.OwningGameInstance==GetGameInstance() && Context.PendingNetGame && Context.PendingNetGame->NetDriver==Driver) { bOurs=true; break; }
		}
	}
	if (!bOurs || !(bInRoom || bWaitingForLobby)) return;
	if (Driver && Driver->GetNetMode()!=NM_Client && (Type==ENetworkFailure::ConnectionLost || Type==ENetworkFailure::ConnectionTimeout)) return;
	ConnectionFailed(TEXT("连接已断开：")+Error);
}
void URogueRoomSubsystem::TravelFailed(UWorld* World,ETravelFailure::Type Type,const FString& Error)
{
	if (World && World->GetGameInstance()==GetGameInstance() && (bInRoom || bWaitingForLobby)) ConnectionFailed(TEXT("进入地图失败：")+Error);
}
void URogueRoomSubsystem::MainMenuEntered()
{
	// A menu world can be loaded while ClientTravel is still connecting. Preserve the operation
	// until EnterLobby / NetworkFailed; a deliberate LeaveRoom sets bLeaving before returning.
	if (bWaitingForLobby && !bLeaving) return;
	bInRoom=false; bWaitingForLobby=false; bBusy=false; bLeaving=false; bMatchStarted=false;
	FTSTicker::RemoveTicker(ConnectTimeout); CloseMenu(); DestroySession();
	if (APlayerController* Player=GetGameInstance()->GetFirstLocalPlayerController())
		if (ARogueHUD* HUD=Cast<ARogueHUD>(Player->GetHUD())) HUD->ShowMainMenuHint(Status);
}
void URogueRoomSubsystem::ShowMenu(APlayerController* Player,int32 Page)
{
	if (!Player || !Player->IsLocalController()) return;
	CloseMenu();
	const auto& Path=Page==2?LobbyWidgetClass:(Page==1?BrowserWidgetClass:ChoiceWidgetClass);
	UClass* Class=Path.LoadSynchronous();
	if (!Class)
	{
		SetStatus(TEXT("多人界面未配置，请检查 WBP 路径。"));
		if (ARogueHUD* HUD=Cast<ARogueHUD>(Player->GetHUD())) { HUD->CloseMultiplayerMenu(); HUD->ShowMainMenuHint(Status); }
		else ConnectionFailed(Status.ToString());
		return;
	}
	ActiveWidget=CreateWidget<URogueMultiplayerWidgetBase>(Player,Class);
	if (!ActiveWidget) return;
	ActiveWidget->AddToViewport(200); ActiveWidget->SetIsFocusable(true); ActiveWidget->SetKeyboardFocus();
	FInputModeUIOnly Input; Input.SetWidgetToFocus(ActiveWidget->TakeWidget()); Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Player->SetInputMode(Input); Player->bShowMouseCursor=true;
}
void URogueRoomSubsystem::CloseMenu()
{
	if (ActiveWidget) { ActiveWidget->RemoveFromParent(); ActiveWidget=nullptr; }
}
void URogueRoomSubsystem::DismissForPlayer(APlayerController* Player)
{
	if (ActiveWidget && ActiveWidget->GetOwningPlayer()==Player) CloseMenu();
}
void URogueRoomSubsystem::CheckIncomingPlayer(AGameModeBase* Mode,const FUniqueNetIdRepl& ID,FString& Error)
{
	if (Mode && Mode->GetGameInstance()==GetGameInstance() && bInRoom && bMatchStarted)
		Error=TEXT("房间已开始游戏，请等待房主创建下一局。");
}
