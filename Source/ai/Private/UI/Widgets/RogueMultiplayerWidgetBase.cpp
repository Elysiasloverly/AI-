#include "UI/Widgets/RogueMultiplayerWidgetBase.h"
#include "Networking/RogueLobby.h"
#include "UI/RogueHUD.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"

namespace
{
	void ButtonLabel(UButton* Button,const FString& Label)
	{
		if (Button) if (UTextBlock* Text=Cast<UTextBlock>(Button->GetChildAt(0))) Text->SetText(FText::FromString(Label));
	}
}
void URogueRoomEntryWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();
	if (ActionButton) ActionButton->OnClicked.AddUniqueDynamic(this,&ThisClass::Join);
}
void URogueRoomEntryWidgetBase::Join() { if (RoomIndex!=INDEX_NONE) OnJoinPressed.Broadcast(RoomIndex); }
void URogueRoomEntryWidgetBase::SetRoom(const FRogueRoomInfo& Room)
{
	RoomIndex=Room.Index;
	if (TitleText) TitleText->SetText(FText::FromString(Room.Name));
	if (DetailText) DetailText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d 人  ·  %d ms"),Room.Players,Room.Capacity,Room.Ping)));
	if (ActionButton) ActionButton->SetIsEnabled(Room.Players<Room.Capacity);
}
void URogueRoomEntryWidgetBase::SetPlayer(const FString& Name,bool bReady,bool bHost)
{
	RoomIndex=INDEX_NONE;
	if (TitleText) TitleText->SetText(FText::FromString(Name+(bHost?TEXT("  [房主]"):TEXT(""))));
	if (DetailText)
	{
		DetailText->SetText(FText::FromString(bReady?TEXT("已准备"):TEXT("未准备")));
		DetailText->SetColorAndOpacity(FSlateColor(bReady?FLinearColor(.025f,.22f,.085f):FLinearColor(.12f,.12f,.15f)));
	}
	if (ActionButton) ActionButton->SetVisibility(ESlateVisibility::Collapsed);
}
URogueRoomSubsystem* URogueMultiplayerWidgetBase::Rooms() const { return URogueRoomSubsystem::GetForPlayer(GetOwningPlayer()); }
FString URogueMultiplayerWidgetBase::PlayerName() const
{
	return PlayerNameInput?PlayerNameInput->GetText().ToString():(Rooms()?Rooms()->GetPlayerDisplayName():TEXT("玩家"));
}
void URogueMultiplayerWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();
	if (CreateRoomButton) CreateRoomButton->OnClicked.AddUniqueDynamic(this,&ThisClass::CreateRoom);
	if (JoinRoomButton) JoinRoomButton->OnClicked.AddUniqueDynamic(this,&ThisClass::BrowseRooms);
	if (RefreshButton) RefreshButton->OnClicked.AddUniqueDynamic(this,&ThisClass::SearchRooms);
	if (JoinIPButton) JoinIPButton->OnClicked.AddUniqueDynamic(this,&ThisClass::JoinIP);
	if (ReadyButton) ReadyButton->OnClicked.AddUniqueDynamic(this,&ThisClass::ToggleReady);
	if (StartButton) StartButton->OnClicked.AddUniqueDynamic(this,&ThisClass::StartGame);
	if (BackButton) BackButton->OnClicked.AddUniqueDynamic(this,&ThisClass::Back);
	if (URogueRoomSubsystem* System=Rooms())
	{
		System->OnRoomChanged.AddUniqueDynamic(this,&ThisClass::Refresh);
		if (PlayerNameInput) PlayerNameInput->SetText(FText::FromString(System->GetPlayerDisplayName()));
	}
	Refresh();
}
void URogueMultiplayerWidgetBase::NativeDestruct()
{
	if (Rooms()) Rooms()->OnRoomChanged.RemoveDynamic(this,&ThisClass::Refresh);
	Super::NativeDestruct();
}
void URogueMultiplayerWidgetBase::NativeTick(const FGeometry& Geometry,float Delta)
{
	Super::NativeTick(Geometry,Delta);
	if (Page==2 && (RefreshTime+=Delta)>=.2f) { RefreshTime=0; Refresh(); }
}
FReply URogueMultiplayerWidgetBase::NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event)
{
	if (Event.GetKey()==EKeys::Escape) { Back(); return FReply::Handled(); }
	return Super::NativeOnKeyDown(Geometry,Event);
}
void URogueMultiplayerWidgetBase::Refresh()
{
	URogueRoomSubsystem* System=Rooms(); if (!System) return;
	if (StatusText) StatusText->SetText(System->GetStatus());
	for (UButton* Button : {CreateRoomButton.Get(),JoinRoomButton.Get(),RefreshButton.Get(),JoinIPButton.Get()}) if (Button) Button->SetIsEnabled(!System->IsBusy());
	if (Page==1 && Entries)
	{
		Entries->ClearChildren();
		if (EntryWidgetClass) for (const FRogueRoomInfo& Room : System->GetRooms())
		{
			URogueRoomEntryWidgetBase* Row=CreateWidget<URogueRoomEntryWidgetBase>(GetOwningPlayer(),EntryWidgetClass);
			Entries->AddChild(Row); Row->SetRoom(Room); Row->SetIsEnabled(!System->IsBusy());
			Row->OnJoinPressed.AddUniqueDynamic(this,&ThisClass::JoinSelected);
		}
	}
	if (Page!=2) return;
	const ARogueLobbyGameState* Lobby=GetWorld()->GetGameState<ARogueLobbyGameState>();
	const ARogueLobbyPlayerState* Local=GetOwningPlayer()->GetPlayerState<ARogueLobbyPlayerState>();
	if (!Lobby || !Local) return;
	if (RoomTitle) RoomTitle->SetText(FText::FromString(FString::Printf(TEXT("%s  ·  %d / %d 人"),*Lobby->RoomName,Lobby->PlayerArray.Num(),Lobby->Capacity)));
	if (AddressText) AddressText->SetText(FText::FromString(TEXT("房主地址：")+Lobby->HostAddress));
	if (ReadyButton) { ReadyButton->SetIsEnabled(!Lobby->bStarting); ButtonLabel(ReadyButton,Local->bReady?TEXT("取消准备"):TEXT("准备")); }
	if (StartButton)
	{
		StartButton->SetVisibility(Local->bHost?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
		StartButton->SetIsEnabled(Local->bHost && !Lobby->bStarting && Lobby->AreAllPlayersReady());
	}
	if (Lobby->bStarting && StatusText) StatusText->SetText(FText::FromString(TEXT("正在进入战斗，请稍候…")));
	FString NewSignature;
	for (APlayerState* Player : Lobby->PlayerArray)
	{
		if (const ARogueLobbyPlayerState* State=Cast<ARogueLobbyPlayerState>(Player)) NewSignature+=FString::Printf(TEXT("%d:%s:%d:%d;"),State->GetPlayerId(),*State->GetPlayerName(),State->bReady,State->bHost);
	}
	if (NewSignature==Signature || !Entries || !EntryWidgetClass) return;
	Signature=NewSignature; Entries->ClearChildren();
	for (APlayerState* Player : Lobby->PlayerArray)
	{
		if (const ARogueLobbyPlayerState* State=Cast<ARogueLobbyPlayerState>(Player))
		{
			URogueRoomEntryWidgetBase* Row=CreateWidget<URogueRoomEntryWidgetBase>(GetOwningPlayer(),EntryWidgetClass);
			Entries->AddChild(Row); Row->SetPlayer(State->GetPlayerName(),State->bReady,State->bHost);
		}
	}
}
void URogueMultiplayerWidgetBase::CreateRoom() { if (Rooms()) Rooms()->HostRoom(RoomNameInput?RoomNameInput->GetText().ToString():TEXT(""),PlayerName()); }
void URogueMultiplayerWidgetBase::BrowseRooms()
{
	if (Rooms()) { Rooms()->SetPlayerDisplayName(PlayerName()); Rooms()->ShowMenu(GetOwningPlayer(),1); Rooms()->FindRooms(); }
}
void URogueMultiplayerWidgetBase::SearchRooms() { if (Rooms()) Rooms()->FindRooms(); }
void URogueMultiplayerWidgetBase::JoinIP() { if (Rooms() && IPInput) Rooms()->JoinByAddress(IPInput->GetText().ToString(),PlayerName()); }
void URogueMultiplayerWidgetBase::JoinSelected(int32 Index) { if (Rooms()) Rooms()->JoinRoom(Index,PlayerName()); }
void URogueMultiplayerWidgetBase::ToggleReady()
{
	if (auto* Player=Cast<ARogueLobbyPlayerController>(GetOwningPlayer()))
		if (const auto* State=Player->GetPlayerState<ARogueLobbyPlayerState>()) Player->ServerSetReady(!State->bReady);
}
void URogueMultiplayerWidgetBase::StartGame() { if (auto* Player=Cast<ARogueLobbyPlayerController>(GetOwningPlayer())) Player->ServerStartGame(); }
void URogueMultiplayerWidgetBase::Back()
{
	if (!Rooms()) return;
	if (Page==2 || Rooms()->IsBusy()) { Rooms()->LeaveRoom(); return; }
	if (Page==1) Rooms()->ShowMenu(GetOwningPlayer(),0);
	else if (ARogueHUD* HUD=Cast<ARogueHUD>(GetOwningPlayer()->GetHUD())) HUD->CloseMultiplayerMenu();
}
