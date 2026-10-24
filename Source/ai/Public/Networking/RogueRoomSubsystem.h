#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/EngineBaseTypes.h"
#include "Containers/Ticker.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "RogueRoomSubsystem.generated.h"

class URogueMultiplayerWidgetBase;
class APlayerController;
class UNetDriver;
class AGameModeBase;
struct FUniqueNetIdRepl;

USTRUCT(BlueprintType)
struct AI_API FRogueRoomInfo
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) int32 Index = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly) FString Name;
	UPROPERTY(BlueprintReadOnly) int32 Players = 0;
	UPROPERTY(BlueprintReadOnly) int32 Capacity = 4;
	UPROPERTY(BlueprintReadOnly) int32 Ping = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRogueRoomChanged);

/** LAN discovery and direct IP connections. Owns async requests across map travel, not gameplay state. */
UCLASS(Config=Game)
class AI_API URogueRoomSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	UFUNCTION(BlueprintPure) static URogueRoomSubsystem* GetForPlayer(APlayerController* Player);
	UPROPERTY(BlueprintAssignable) FRogueRoomChanged OnRoomChanged;
	UFUNCTION(BlueprintCallable) void HostRoom(const FString& RoomName, const FString& PlayerName, int32 Capacity=4);
	UFUNCTION(BlueprintCallable) void FindRooms();
	UFUNCTION(BlueprintCallable) void JoinRoom(int32 Index, const FString& PlayerName);
	UFUNCTION(BlueprintCallable) void JoinByAddress(const FString& Address, const FString& PlayerName);
	UFUNCTION(BlueprintCallable) void LeaveRoom();
	UFUNCTION(BlueprintPure) TArray<FRogueRoomInfo> GetRooms() const { return Rooms; }
	UFUNCTION(BlueprintPure) FText GetStatus() const { return Status; }
	UFUNCTION(BlueprintPure) bool IsBusy() const { return bBusy; }
	UFUNCTION(BlueprintPure) FString GetPlayerDisplayName() const { return DisplayName; }
	UFUNCTION(BlueprintCallable) void SetPlayerDisplayName(const FString& Name) { DisplayName=CleanName(Name); }
	UFUNCTION(BlueprintPure) FString GetHostedRoomName() const { return HostedRoomName; }
	UFUNCTION(BlueprintPure) int32 GetHostedCapacity() const { return HostedCapacity; }
	UFUNCTION(BlueprintCallable) void ShowMenu(APlayerController* Player, int32 Page=0);
	void CloseMenu();
	void EnterLobby(APlayerController* Player);
	void DismissForPlayer(APlayerController* Player);
	void MainMenuEntered();
	void MarkMatchStarted();
	static bool ValidateAddress(const FString& Input, FString& Output);
	static FString CleanName(const FString& Input);
	UPROPERTY(Config, EditDefaultsOnly, Category="房间") FString LobbyMap = TEXT("/Game/Map/MainMenu");
	UPROPERTY(Config, EditDefaultsOnly, Category="房间") FString BattleMap = TEXT("/Game/Map/Battle_Day");
	UPROPERTY(Config, EditDefaultsOnly, Category="房间") TSoftClassPtr<URogueMultiplayerWidgetBase> ChoiceWidgetClass;
	UPROPERTY(Config, EditDefaultsOnly, Category="房间") TSoftClassPtr<URogueMultiplayerWidgetBase> BrowserWidgetClass;
	UPROPERTY(Config, EditDefaultsOnly, Category="房间") TSoftClassPtr<URogueMultiplayerWidgetBase> LobbyWidgetClass;
private:
	IOnlineSessionPtr Sessions();
	void SetStatus(const FString& Message, bool bNewBusy=false);
	void BeginCreate();
	void OnCreated(FName Name, bool bSuccess);
	void OnFound(bool bSuccess);
	void OnJoined(FName Name, EOnJoinSessionCompleteResult::Type Result);
	void OnDestroyed(FName Name, bool bSuccess);
	void DestroySession(bool bCreateAfter=false);
	void Connect(const FString& URL);
	void ConnectionFailed(const FString& Message);
	void NetworkFailed(UWorld* World, UNetDriver* Driver, ENetworkFailure::Type Type, const FString& Error);
	void TravelFailed(UWorld* World, ETravelFailure::Type Type, const FString& Error);
	void CheckIncomingPlayer(AGameModeBase* Mode, const FUniqueNetIdRepl& ID, FString& Error);
	IOnlineSessionPtr SessionInterface;
	TSharedPtr<FOnlineSessionSearch> Search;
	FDelegateHandle CreateHandle, FindHandle, JoinHandle, DestroyHandle, NetworkHandle, TravelHandle;
	FDelegateHandle PreLoginHandle;
	FTSTicker::FDelegateHandle ConnectTimeout;
	UPROPERTY(Transient) TArray<FRogueRoomInfo> Rooms;
	UPROPERTY(Transient) TObjectPtr<URogueMultiplayerWidgetBase> ActiveWidget;
	FText Status;
	FString DisplayName = TEXT("玩家");
	FString HostedRoomName;
	int32 HostedCapacity = 4;
	bool bBusy=false, bInRoom=false, bWaitingForLobby=false, bCreateAfterDestroy=false, bLeaving=false;
	bool bMatchStarted=false;
};
