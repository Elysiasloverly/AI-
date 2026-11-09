#include "RogueVitalsFeedbackPreviewLibrary.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Engine/Font.h"
#include "Enemies/Spawning/RogueSpawnSubsystem.h"
#include "GameFramework/HUD.h"
#include "GameFramework/PlayerController.h"
#include "RogueWidgetBuilder.h"
#include "UI/Widgets/RogueEnemyHealthBarWidgetBase.h"
#include "UI/Widgets/RogueVitalsWidgetBase.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"

namespace
{
	TStrongObjectPtr<URogueVitalsPreviewSession> Session;
	FDelegateHandle CleanupHandle;

	bool Matches(APlayerController* Owner)
	{
		return Owner && Session.IsValid() && Session->Owner == Owner && Session->World == Owner->GetWorld();
	}

	void ReleaseSession(bool bRestore)
	{
		if (Session.IsValid())
		{
			if (Session->Player) { Session->Player->RemoveFromParent(); }
			for (URogueEnemyHealthBarWidgetBase* Enemy : Session->Enemies)
			{
				if (Enemy) { Enemy->RemoveFromParent(); }
			}
			if (Session->Labels) { Session->Labels->RemoveFromParent(); }
			if (bRestore && Session->World.IsValid())
			{
				if (auto* Spawn = Session->World->GetSubsystem<URogueSpawnSubsystem>()) { Spawn->SetSpawningSuspended(Session->bOriginalSpawningSuspended); }
				if (Session->HUD.IsValid()) { Session->HUD->SetActorTickEnabled(Session->bOriginalHUDTick); }
				for (const TWeakObjectPtr<URogueVitalsWidgetBase>& Original : Session->OriginalVitals)
				{
					if (Original.IsValid() && !Original->IsInViewport()) { Original->AddToViewport(4); }
				}
			}
			Session.Reset();
		}
		if (CleanupHandle.IsValid())
		{
			FWorldDelegates::OnWorldCleanup.Remove(CleanupHandle);
			CleanupHandle.Reset();
		}
	}

	UTextBlock* Label(UWidgetTree* Tree, UCanvasPanel* Root, FName Name, const TCHAR* Value,
		FVector2D Position, FVector2D Size, FAnchors Anchors, FVector2D Alignment, int32 FontSize)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Text->SetText(FText::FromString(Value));
		Text->SetFont(FSlateFontInfo(URogueWidgetBuilder::GetOrCreateProjectFont(), FontSize));
		Text->SetColorAndOpacity(FLinearColor(0.94f, 0.93f, 0.88f, 1.0f));
		Text->SetShadowOffset(FVector2D(0.0f, 1.0f));
		Text->SetShadowColorAndOpacity(FLinearColor(0.005f, 0.01f, 0.018f, 0.85f));
		Text->SetJustification(ETextJustify::Center);
		UCanvasPanelSlot* Slot = Root->AddChildToCanvas(Text);
		Slot->SetAnchors(Anchors); Slot->SetAlignment(Alignment); Slot->SetPosition(Position); Slot->SetSize(Size);
		return Text;
	}

	float Percent(UUserWidget* Widget, const TCHAR* Name)
	{
		UProgressBar* Bar = Widget ? Cast<UProgressBar>(Widget->GetWidgetFromName(FName(Name))) : nullptr;
		return Bar ? Bar->GetPercent() : -1.0f;
	}
}

void URogueVitalsPreviewLabels::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree) { WidgetTree = NewObject<UWidgetTree>(this); }
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PreviewLabels"));
	WidgetTree->RootWidget = Root;
	PhaseCaption = Label(WidgetTree, Root, TEXT("PhaseCaption"), TEXT("状态反馈预览"),
		FVector2D(0.0f, 48.0f), FVector2D(1120.0f, 42.0f), FAnchors(0.5f, 0.0f), FVector2D(0.5f, 0.0f), 24);
	const TCHAR* Names[] = { TEXT("普通敌人 · 受击残影"), TEXT("普通敌人 · 池化复用"), TEXT("Boss · 头顶局部血条") };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Label(WidgetTree, Root, FName(*FString::Printf(TEXT("EnemyCaption%d"), Index)), Names[Index],
			FVector2D(0.0f, -38.0f), FVector2D(290.0f, 30.0f), FAnchors(0.45f + Index * 0.17f, 0.58f), FVector2D(0.5f, 0.0f), 18);
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void URogueVitalsPreviewLabels::SetPhaseCaption(const FText& Caption)
{
	if (PhaseCaption) { PhaseCaption->SetText(Caption); }
}

UUserWidget* URogueVitalsFeedbackPreviewLibrary::CreatePreview(APlayerController* Owner)
{
	if (!Owner || !Owner->GetWorld()) { return nullptr; }
	ReleaseSession(true);
	UClass* PlayerClass = LoadClass<URogueVitalsWidgetBase>(nullptr,
		TEXT("/Game/UMG/HUD/WBP_PlayerVitals.WBP_PlayerVitals_C"));
	UClass* EnemyClass = LoadClass<URogueEnemyHealthBarWidgetBase>(nullptr,
		TEXT("/Game/UMG/HUD/WBP_EnemyHealthBar.WBP_EnemyHealthBar_C"));
	if (!PlayerClass || !EnemyClass) { return nullptr; }
	Session.Reset(NewObject<URogueVitalsPreviewSession>(GetTransientPackage()));
	Session->World = Owner->GetWorld(); Session->Owner = Owner; Session->HUD = Owner->GetHUD();
	if (auto* Spawn = Owner->GetWorld()->GetSubsystem<URogueSpawnSubsystem>())
	{
		Session->bOriginalSpawningSuspended = Spawn->IsSpawningSuspended();
		Spawn->SetSpawningSuspended(true);
	}
	if (Session->HUD.IsValid())
	{
		Session->bOriginalHUDTick = Session->HUD->IsActorTickEnabled();
		Session->HUD->SetActorTickEnabled(false);
	}
	// 原状态条从视口暂时移除，避免两份数据同屏；引用保持弱关系。
	for (TObjectIterator<URogueVitalsWidgetBase> It; It; ++It)
	{
		if (It->GetWorld() == Owner->GetWorld() && It->GetOwningPlayer() == Owner && It->IsInViewport())
		{
			Session->OriginalVitals.Add(*It);
			It->RemoveFromParent();
		}
	}
	Session->Player = CreateWidget<URogueVitalsWidgetBase>(Owner, PlayerClass);
	if (!Session->Player) { ReleaseSession(true); return nullptr; }
	Session->Player->AddToViewport(200);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		URogueEnemyHealthBarWidgetBase* Enemy = CreateWidget<URogueEnemyHealthBarWidgetBase>(Owner, EnemyClass);
		if (!Enemy) { ReleaseSession(true); return nullptr; }
		Session->Enemies.Add(Enemy); Session->EnemyIds[Index] = FGuid::NewGuid();
		Enemy->AddToViewport(201); Enemy->SetAlignmentInViewport(FVector2D(0.5f, 0.0f));
	}
	Session->Labels = CreateWidget<URogueVitalsPreviewLabels>(Owner);
	Session->Labels->AddToViewport(199);
	CleanupHandle = FWorldDelegates::OnWorldCleanup.AddLambda([](UWorld* World, bool, bool)
	{
		if (Session.IsValid() && Session->World == World) { ReleaseSession(false); }
	});
	SetPreviewPhase(Owner, 0);
	return Session->Player;
}

bool URogueVitalsFeedbackPreviewLibrary::SetPreviewPhase(APlayerController* Owner, int32 Phase)
{
	if (!Matches(Owner) || Phase < 0 || Phase > 8) { return false; }
	const int32 PreviousPhase = Session->Phase;
	Session->Phase = Phase;
	FRoguePlayerVitalsView View;
	View.bVisible = true; View.SourceActor = Owner->GetPawn(); View.bHasArmor = true;
	View.CurrentHealth = 100.0f; View.MaxHealth = 100.0f; View.CurrentArmor = 50.0f; View.MaxArmor = 50.0f;
	View.Experience = 4; View.ExperienceToNextLevel = 20; View.Level = 5; View.CurrentWave = 12; View.Money = 348;
	if (Phase >= 1) { View.CurrentArmor = 30.0f; }
	if (Phase >= 2) { View.CurrentHealth = 62.0f; View.CurrentArmor = 0.0f; }
	if (Phase >= 3) { View.CurrentHealth = 82.0f; View.CurrentArmor = 20.0f; }
	if (Phase >= 4) { View.Experience = 16; }
	if (Phase >= 5) { View.Level = 6; View.Experience = 3; View.ExperienceToNextLevel = 24; }
	if (Phase >= 6) { View.bHasArmor = false; View.CurrentArmor = View.MaxArmor = 0.0f; }
	if (Phase == 7) { View.CurrentHealth = 0.0f; View.bDowned = true; }
	if (Phase == 8) { View.CurrentHealth = 100.0f; }
	View.HealthPercent = View.CurrentHealth / View.MaxHealth;
	View.ArmorPercent = View.MaxArmor > 0.0f ? View.CurrentArmor / View.MaxArmor : 0.0f;
	View.ExperiencePercent = float(View.Experience) / View.ExperienceToNextLevel;
	Session->Player->UpdateVitals(View);
	const TCHAR* Captions[] = {
		TEXT("初始状态 · 本地样例数据"), TEXT("护甲受击 · 短暂变化数值与暖色残影"),
		TEXT("生命受伤 / 护甲破裂 · 行强调与轻微动态"), TEXT("生命 / 护甲恢复 · 回升与短暂提示"),
		TEXT("经验增长 · 平滑追赶真实值"), TEXT("跨级 · 先满条，再过渡到新等级经验"),
		TEXT("无护甲时收拢 · 敌人血条隐藏与复用无旧残影"), TEXT("濒死 · 立即清除旧反馈"), TEXT("复活 · 从真实值重新开始") };
	Session->Labels->SetPhaseCaption(FText::FromString(Captions[Phase]));
	int32 Width = 1920, Height = 1080; Owner->GetViewportSize(Width, Height);
	const float Damaged[] = { 0.48f, 0.25f, 0.64f };
	for (int32 Index = 0; Index < Session->Enemies.Num(); ++Index)
	{
		FRogueEnemyHealthBarView EnemyView;
		EnemyView.bVisible = !(Phase >= 6 && Index == 0);
		EnemyView.bBoss = Index == 2;
		if (Phase == 6 && PreviousPhase != 6 && Index == 1)
		{
			Session->EnemyIds[Index] = FGuid::NewGuid(); ++Session->EnemyGenerations[Index];
		}
		EnemyView.SpawnId = Session->EnemyIds[Index]; EnemyView.SpawnGeneration = Session->EnemyGenerations[Index];
		EnemyView.HealthPercent = Phase == 0 ? 1.0f : Phase >= 6 && Index == 1 ? 1.0f : Damaged[Index];
		EnemyView.ScreenPosition = FVector2D(Width * (0.45f + Index * 0.17f), Height * 0.58f);
		Session->Enemies[Index]->UpdateBar(EnemyView);
	}
	return true;
}

FString URogueVitalsFeedbackPreviewLibrary::DescribePreview(APlayerController* Owner)
{
	if (!Matches(Owner)) { return TEXT("{}"); }
	return FString::Printf(TEXT("{\"phase\":%d,\"health_fill\":%.4f,\"health_trail\":%.4f,\"armor_fill\":%.4f,\"armor_trail\":%.4f,\"experience_fill\":%.4f,\"enemy0_fill\":%.4f,\"enemy0_trail\":%.4f,\"enemy1_fill\":%.4f,\"enemy1_trail\":%.4f,\"enemy0_visible\":%s}"),
		Session->Phase, Percent(Session->Player, TEXT("HealthBar")), Percent(Session->Player, TEXT("HealthDamageBar")),
		Percent(Session->Player, TEXT("ArmorBar")), Percent(Session->Player, TEXT("ArmorDamageBar")), Percent(Session->Player, TEXT("ExperienceBar")),
		Percent(Session->Enemies[0], TEXT("FillBar")), Percent(Session->Enemies[0], TEXT("DamageBar")),
		Percent(Session->Enemies[1], TEXT("FillBar")), Percent(Session->Enemies[1], TEXT("DamageBar")),
		Session->Enemies[0]->IsVisible() ? TEXT("true") : TEXT("false"));
}

void URogueVitalsFeedbackPreviewLibrary::ClearPreview(APlayerController* Owner)
{
	if (Matches(Owner)) { ReleaseSession(true); }
}
