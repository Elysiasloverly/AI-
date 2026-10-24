#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Networking/RogueRoomSubsystem.h"
#include "RogueMultiplayerWidgetBase.generated.h"
class UButton;
class UTextBlock;
class UEditableTextBox;
class UScrollBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRogueJoinRoomPressed,int32,Index);
UCLASS(Abstract,Blueprintable)
class AI_API URogueRoomEntryWidgetBase : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetRoom(const FRogueRoomInfo& Room);
	void SetPlayer(const FString& Name,bool bReady,bool bHost);
	UPROPERTY(BlueprintAssignable) FRogueJoinRoomPressed OnJoinPressed;
protected:
	virtual void NativeConstruct() override;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> DetailText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> ActionButton;
private:
	UFUNCTION() void Join();
	int32 RoomIndex=INDEX_NONE;
};

/** Shared interaction for three WBP pages; all layout, fonts and row styles remain in WBP. */
UCLASS(Abstract,Blueprintable)
class AI_API URogueMultiplayerWidgetBase : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="多人界面",meta=(ClampMin="0",ClampMax="2",ToolTip="0 创建/加入选择，1 搜房与 IP，2 房间大厅")) int32 Page=0;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="多人界面") TSubclassOf<URogueRoomEntryWidgetBase> EntryWidgetClass;
	UFUNCTION(BlueprintCallable) void Refresh();
	/** The same actions used by the default buttons, available to custom WBP layouts. */
	UFUNCTION(BlueprintCallable) void ToggleReady();
	UFUNCTION(BlueprintCallable) void StartGame();
protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry,float Delta) override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry,const FKeyEvent& Event) override;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> CreateRoomButton;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> JoinRoomButton;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> RefreshButton;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> JoinIPButton;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> ReadyButton;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> StartButton;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> BackButton;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UEditableTextBox> PlayerNameInput;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UEditableTextBox> RoomNameInput;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UEditableTextBox> IPInput;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> RoomTitle;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> AddressText;
	UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UScrollBox> Entries;
private:
	URogueRoomSubsystem* Rooms() const;
	FString PlayerName() const;
	UFUNCTION() void CreateRoom();
	UFUNCTION() void BrowseRooms();
	UFUNCTION() void SearchRooms();
	UFUNCTION() void JoinIP();
	UFUNCTION() void JoinSelected(int32 Index);
	UFUNCTION() void Back();
	FString Signature;
	float RefreshTime=0;
};
