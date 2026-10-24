#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "RogueLobby.generated.h"

UCLASS()
class AI_API ARogueLobbyPlayerState : public APlayerState
{
	GENERATED_BODY()
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	UPROPERTY(Replicated, BlueprintReadOnly) bool bReady=false;
	UPROPERTY(Replicated, BlueprintReadOnly) bool bHost=false;
};

UCLASS()
class AI_API ARogueLobbyGameState : public AGameStateBase
{
	GENERATED_BODY()
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	UPROPERTY(Replicated, BlueprintReadOnly) FString RoomName;
	UPROPERTY(Replicated, BlueprintReadOnly) FString HostAddress;
	UPROPERTY(Replicated, BlueprintReadOnly) int32 Capacity=4;
	UPROPERTY(Replicated, BlueprintReadOnly) bool bStarting=false;
	UFUNCTION(BlueprintPure) bool AreAllPlayersReady() const;
};

UCLASS()
class AI_API ARogueLobbyPlayerController : public APlayerController
{
	GENERATED_BODY()
public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	UFUNCTION(Server,Reliable,BlueprintCallable) void ServerSetReady(bool bReady);
	UFUNCTION(Server,Reliable) void ServerSetDisplayName(const FString& Name);
	UFUNCTION(Server,Reliable,BlueprintCallable) void ServerStartGame();
};

/** Lobby authority. Clients may change their own readiness; only the local listen host may start. */
UCLASS()
class AI_API ARogueLobbyGameMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	ARogueLobbyGameMode();
	virtual void BeginPlay() override;
	virtual void InitGame(const FString& MapName, const FString& Options, FString& Error) override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& ID, FString& Error) override;
	virtual void PostLogin(APlayerController* Player) override;
	bool TryStartGame(APlayerController* Requester);
};
