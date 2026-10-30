#include "RogueWidgetBuilder.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "Blueprint/WidgetTree.h"
#include "AssetToolsModule.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Components/SizeBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Factories/FontFactory.h"
#include "Fonts/CompositeFont.h"
#include "IAssetTools.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UObject/ObjectRedirector.h"
#include "Engine/FontFace.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/CoreStyle.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "WidgetBlueprint.h"

namespace
{
	UCanvasPanel* ResetCanvas(UWidgetBlueprint* Blueprint)
	{
		UWidgetTree* Tree = Blueprint->WidgetTree;
		Tree->Modify();

		UCanvasPanel* Root = Cast<UCanvasPanel>(Tree->RootWidget);
		if (Root == nullptr)
		{
			Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
			Tree->RootWidget = Root;
		}

		Root->ClearChildren();
		return Root;
	}

	UCanvasPanelSlot* AddToCanvas(UCanvasPanel* Root, UWidget* Widget, const FAnchors& Anchors, const FVector2D& Alignment, const FVector2D& Position, const FVector2D& Size)
	{
		UCanvasPanelSlot* Slot = Root->AddChildToCanvas(Widget);
		if (Slot == nullptr)
		{
			return nullptr;
		}

		Slot->SetAnchors(Anchors);
		Slot->SetAlignment(Alignment);
		Slot->SetAutoSize(false);
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
		return Slot;
	}

	UProgressBar* MakeBar(UWidgetBlueprint* Blueprint, UCanvasPanel* Root, FName Name, const FLinearColor& Color, const FVector2D& Position)
	{
		UProgressBar* Bar = Blueprint->WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), Name);
		Bar->SetFillColorAndOpacity(Color);
		Bar->SetPercent(1.0f);

		// 锚定左下角：Position 是相对锚点的偏移，Y 为负表示向上。
		AddToCanvas(Root, Bar, FAnchors(0.0f, 1.0f, 0.0f, 1.0f), FVector2D(0.0f, 1.0f), Position, FVector2D(320.0f, 18.0f));
		return Bar;
	}

	UFontFace* GetChineseFontFace()
	{
		// Roboto 不含 CJK 字形，直接用引擎自带的 DroidSansFallback。
		static UFontFace* FontFace = LoadObject<UFontFace>(nullptr,
			TEXT("/Engine/EngineFonts/Faces/DroidSansFallback.DroidSansFallback"));
		return FontFace;
	}

	/** 统一字体对象：优先项目复合字体（Mono + 中文回退），创建失败再退回 CJK 字体。 */
	UObject* GetUIFontObject()
	{
		if (UFont* ProjectFont = URogueWidgetBuilder::GetOrCreateProjectFont())
		{
			return ProjectFont;
		}
		return GetChineseFontFace();
	}

	UTextBlock* MakeText(
		UWidgetBlueprint* Blueprint,
		UCanvasPanel* Root,
		FName Name,
		const FString& DefaultText,
		const FLinearColor& Color,
		int32 FontSize,
		const FAnchors& Anchors,
		const FVector2D& Alignment,
		const FVector2D& Position,
		const FVector2D& Size)
	{
		UTextBlock* Text = Blueprint->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Text->SetText(FText::FromString(DefaultText));
		Text->SetColorAndOpacity(FSlateColor(Color));
		if (UObject* FontObject = GetUIFontObject())
		{
			Text->SetFont(FSlateFontInfo(FontObject, static_cast<float>(FontSize)));
		}
		else
		{
			Text->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FontSize));
		}
		Text->SetJustification(ETextJustify::Left);
		Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
		Text->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.65f));
		AddToCanvas(Root, Text, Anchors, Alignment, Position, Size);
		return Text;
	}

	/** 只创建 TextBlock（不挂到 Canvas），供 VerticalBox / ScrollBox 排版使用。 */
	UTextBlock* MakeTextBlock(
		UWidgetBlueprint* Blueprint,
		FName Name,
		const FString& DefaultText,
		const FLinearColor& Color,
		int32 FontSize,
		ETextJustify::Type Justification,
		bool bAutoWrap)
	{
		UTextBlock* Text = Blueprint->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Text->SetText(FText::FromString(DefaultText));
		Text->SetColorAndOpacity(FSlateColor(Color));
		if (UObject* FontObject = GetUIFontObject())
		{
			Text->SetFont(FSlateFontInfo(FontObject, static_cast<float>(FontSize)));
		}
		else
		{
			Text->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FontSize));
		}
		Text->SetJustification(Justification);
		Text->SetAutoWrapText(bAutoWrap);
		Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
		Text->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.65f));
		return Text;
	}

	/** 按钮 + 居中文字；名字就是 C++ 的 BindWidgetOptional 名字。 */
	UButton* MakeMenuButton(
		UWidgetBlueprint* Blueprint,
		UVerticalBox* Box,
		FName Name,
		const FString& Label,
		int32 FontSize)
	{
		UButton* Button = Blueprint->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		// 占位配色：深蓝底 + 浅色字，保证默认样式下也看得清；换样式直接改 WBP。
		Button->SetBackgroundColor(FLinearColor(0.12f, 0.14f, 0.22f, 0.95f));

		UTextBlock* LabelText = MakeTextBlock(Blueprint, FName(*(Name.ToString() + TEXT("Label"))),
			Label, FLinearColor(0.95f, 0.95f, 1.0f), FontSize, ETextJustify::Center, false);
		Button->AddChild(LabelText);

		if (UVerticalBoxSlot* Slot = Box->AddChildToVerticalBox(Button))
		{
			Slot->SetPadding(FMargin(0.0f, 5.0f));
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
		return Button;
	}

	void Finish(UWidgetBlueprint* Blueprint)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		Blueprint->GetOutermost()->MarkPackageDirty();
	}
}

UFont* URogueWidgetBuilder::GetOrCreateProjectFont()
{
	static TWeakObjectPtr<UFont> CachedFont;
	if (CachedFont.IsValid())
	{
		return CachedFont.Get();
	}

	const TCHAR* FontAssetPath = TEXT("/Game/UMG/Fonts/F_DroidMono_CJK.F_DroidMono_CJK");
	UFont* Font = LoadObject<UFont>(nullptr, FontAssetPath);
	if (Font == nullptr)
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
		Font = Cast<UFont>(AssetTools.CreateAsset(
			TEXT("F_DroidMono_CJK"),
			TEXT("/Game/UMG/Fonts"),
			UFont::StaticClass(),
			NewObject<UFontFactory>()));
	}
	if (Font == nullptr)
	{
		return nullptr;
	}

	UFontFace* MonoFace = LoadObject<UFontFace>(nullptr,
		TEXT("/Engine/EngineFonts/Faces/DroidSansMono.DroidSansMono"));
	UFontFace* CJKFace = LoadObject<UFontFace>(nullptr,
		TEXT("/Engine/EngineFonts/Faces/DroidSansFallback.DroidSansFallback"));
	if (MonoFace == nullptr)
	{
		return nullptr;
	}

	// 运行时复合字体：拉丁/数字用 DroidSansMono，缺字（中文）回退 DroidSansFallback。
	Font->FontCacheType = EFontCacheType::Runtime;
	FCompositeFont& Composite = Font->CompositeFont;

	Composite.DefaultTypeface.Fonts.Reset();
	{
		FTypefaceEntry& Entry = Composite.DefaultTypeface.Fonts.AddDefaulted_GetRef();
		Entry.Name = TEXT("Regular");
		Entry.Font = FFontData(MonoFace);
	}

	Composite.FallbackTypeface.Typeface.Fonts.Reset();
	if (CJKFace != nullptr)
	{
		FTypefaceEntry& Entry = Composite.FallbackTypeface.Typeface.Fonts.AddDefaulted_GetRef();
		Entry.Name = TEXT("Regular");
		Entry.Font = FFontData(CJKFace);
	}
	Composite.FallbackTypeface.ScalingFactor = 1.0f;

	Font->MarkPackageDirty();
	CachedFont = Font;
	return Font;
}

bool URogueWidgetBuilder::ApplyProjectFont(UWidgetBlueprint* Blueprint)
{
	if (Blueprint == nullptr || Blueprint->WidgetTree == nullptr)
	{
		return false;
	}

	UFont* ProjectFont = GetOrCreateProjectFont();
	if (ProjectFont == nullptr)
	{
		return false;
	}

	TArray<UWidget*> Widgets;
	Blueprint->WidgetTree->GetAllWidgets(Widgets);

	int32 ChangedCount = 0;
	for (UWidget* Widget : Widgets)
	{
		if (UTextBlock* Text = Cast<UTextBlock>(Widget))
		{
			const FSlateFontInfo OldFont = Text->GetFont();
			Text->SetFont(FSlateFontInfo(ProjectFont, OldFont.Size, OldFont.TypefaceFontName));
			++ChangedCount;
		}
	}

	Finish(Blueprint);
	return ChangedCount > 0;
}

bool URogueWidgetBuilder::MoveAsset(const FString& OldAssetPath, const FString& NewAssetPath)
{
	UObject* Asset = LoadObject<UObject>(nullptr, *OldAssetPath);
	if (Asset == nullptr)
	{
		return false;
	}

	const FString NewPackagePath = FPackageName::GetLongPackagePath(NewAssetPath);
	const FString NewAssetName = FPackageName::GetShortName(NewAssetPath);
	if (NewPackagePath.IsEmpty() || NewAssetName.IsEmpty())
	{
		return false;
	}

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();

	TArray<FAssetRenameData> Renames;
	Renames.Emplace(Asset, NewPackagePath, NewAssetName);
	if (!AssetTools.RenameAssets(Renames))
	{
		return false;
	}

	// 重命名会在旧路径留 redirector；批量移动后由 ResavePackages -FixupRedirectors 统一修引用并删除。
	return true;
}

bool URogueWidgetBuilder::BuildPlayerVitals(UWidgetBlueprint* Blueprint)
{
	if (Blueprint == nullptr || Blueprint->WidgetTree == nullptr)
	{
		return false;
	}

	UCanvasPanel* Root = ResetCanvas(Blueprint);

	// 左上角：波数 + 金币。
	MakeText(Blueprint, Root, TEXT("WaveText"), TEXT("第 0 波"), FLinearColor::White, 26,
		FAnchors(0.0f, 0.0f), FVector2D(0.0f, 0.0f), FVector2D(36.0f, 24.0f), FVector2D(280.0f, 40.0f));
	MakeText(Blueprint, Root, TEXT("GoldText"), TEXT("金币 0"), FLinearColor(1.0f, 0.84f, 0.34f), 20,
		FAnchors(0.0f, 0.0f), FVector2D(0.0f, 0.0f), FVector2D(36.0f, 64.0f), FVector2D(280.0f, 32.0f));

	// 左下角：生命 / 护甲 / 经验条 + 数值。
	MakeBar(Blueprint, Root, TEXT("HealthBar"), FLinearColor(0.85f, 0.15f, 0.15f, 0.95f), FVector2D(40.0f, -92.0f));
	MakeText(Blueprint, Root, TEXT("HealthText"), TEXT("生命 0 / 0"), FLinearColor::White, 18,
		FAnchors(0.0f, 1.0f), FVector2D(0.0f, 1.0f), FVector2D(40.0f, -116.0f), FVector2D(320.0f, 24.0f));

	MakeBar(Blueprint, Root, TEXT("ArmorBar"), FLinearColor(0.58f, 0.60f, 0.64f, 0.96f), FVector2D(40.0f, -152.0f));
	MakeText(Blueprint, Root, TEXT("ArmorText"), TEXT("护甲 0 / 0"), FLinearColor(0.78f, 0.80f, 0.84f), 18,
		FAnchors(0.0f, 1.0f), FVector2D(0.0f, 1.0f), FVector2D(40.0f, -176.0f), FVector2D(320.0f, 24.0f));

	MakeBar(Blueprint, Root, TEXT("ExperienceBar"), FLinearColor(0.2f, 0.6f, 1.0f, 0.95f), FVector2D(40.0f, -212.0f));
	MakeText(Blueprint, Root, TEXT("ExperienceText"), TEXT("经验 0 / 0"), FLinearColor(0.4f, 0.8f, 1.0f), 18,
		FAnchors(0.0f, 1.0f), FVector2D(0.0f, 1.0f), FVector2D(40.0f, -236.0f), FVector2D(320.0f, 24.0f));

	Finish(Blueprint);
	return true;
}

bool URogueWidgetBuilder::BuildEnemyHealthBar(UWidgetBlueprint* Blueprint)
{
	if (Blueprint == nullptr || Blueprint->WidgetTree == nullptr)
	{
		return false;
	}

	UCanvasPanel* Root = ResetCanvas(Blueprint);

	// 背景铺满 WBP 的 viewport 尺寸（由 HUD 的 BarSize 决定）。
	UBorder* Background = Blueprint->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
	Background->SetBrushColor(FLinearColor(0.04f, 0.04f, 0.05f, 0.78f));
	if (UCanvasPanelSlot* BackgroundSlot = Root->AddChildToCanvas(Background))
	{
		BackgroundSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		BackgroundSlot->SetOffsets(FMargin(0.0f));
	}

	UProgressBar* Fill = Blueprint->WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("FillBar"));
	Fill->SetFillColorAndOpacity(FLinearColor(0.95f, 0.15f, 0.20f, 0.95f));
	Fill->SetPercent(1.0f);
	if (UCanvasPanelSlot* FillSlot = Root->AddChildToCanvas(Fill))
	{
		FillSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		// 留 2px 边，让背景露出一点描边。
		FillSlot->SetOffsets(FMargin(2.0f));
	}

	Finish(Blueprint);
	return true;
}

bool URogueWidgetBuilder::BuildDamageFlash(UWidgetBlueprint* Blueprint)
{
	if (Blueprint == nullptr || Blueprint->WidgetTree == nullptr)
	{
		return false;
	}

	UCanvasPanel* Root = ResetCanvas(Blueprint);

	// FlashRoot 是 C++ 唯一按名字找的控件（BindWidgetOptional）：它承载整个泛红层，
	// C++ 只改它的 RenderOpacity，里面的配色/边缘宽度/材质都归 WBP。
	UCanvasPanel* FlashRoot = Blueprint->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("FlashRoot"));
	// 默认完全透明：本控件的语义是"受伤才出现"。留 1 的话，只要创建即加视口，
	// 整屏泛红就会从进游戏起一直挂着（C++ 只在受伤后才会写这个值）。
	// 勾了 bBlueprintHandlesOpacity 由蓝图自己驱动透明度时，这个初值同样必须留 0。
	FlashRoot->SetRenderOpacity(0.0f);
	if (UCanvasPanelSlot* FlashSlot = Root->AddChildToCanvas(FlashRoot))
	{
		FlashSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		FlashSlot->SetOffsets(FMargin(0.0f));
	}

	// 中心底色：很淡的一层，让"掉血"在屏幕中间也有感知，但不会盖住画面。
	UBorder* CoreTint = Blueprint->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CoreTint"));
	CoreTint->SetBrushColor(FLinearColor(0.62f, 0.02f, 0.04f, 0.10f));
	if (UCanvasPanelSlot* CoreSlot = FlashRoot->AddChildToCanvas(CoreTint))
	{
		CoreSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		CoreSlot->SetOffsets(FMargin(0.0f));
	}

	// 四周边缘：占位用纯色 Border，后续换成径向渐变材质或加噪点都在这四条上改。
	const float EdgeThickness = 180.0f;
	const FLinearColor EdgeColor(0.72f, 0.01f, 0.03f, 0.55f);

	struct FEdgeSpec
	{
		const TCHAR* Name;
		FAnchors Anchors;
		FMargin Offsets;
	};

	const FEdgeSpec Edges[] =
	{
		{ TEXT("TopEdge"), FAnchors(0.0f, 0.0f, 1.0f, 0.0f), FMargin(0.0f, 0.0f, 0.0f, EdgeThickness) },
		{ TEXT("BottomEdge"), FAnchors(0.0f, 1.0f, 1.0f, 1.0f), FMargin(0.0f, -EdgeThickness, 0.0f, 0.0f) },
		{ TEXT("LeftEdge"), FAnchors(0.0f, 0.0f, 0.0f, 1.0f), FMargin(0.0f, 0.0f, EdgeThickness, 0.0f) },
		{ TEXT("RightEdge"), FAnchors(1.0f, 0.0f, 1.0f, 1.0f), FMargin(-EdgeThickness, 0.0f, 0.0f, 0.0f) },
	};

	for (const FEdgeSpec& Edge : Edges)
	{
		UBorder* Strip = Blueprint->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Edge.Name);
		Strip->SetBrushColor(EdgeColor);
		if (UCanvasPanelSlot* StripSlot = FlashRoot->AddChildToCanvas(Strip))
		{
			StripSlot->SetAnchors(Edge.Anchors);
			StripSlot->SetOffsets(Edge.Offsets);
		}
	}

	Finish(Blueprint);
	return true;
}

bool URogueWidgetBuilder::BuildMainMenu(UWidgetBlueprint* Blueprint)
{
	if (Blueprint == nullptr || Blueprint->WidgetTree == nullptr)
	{
		return false;
	}

	UCanvasPanel* Root = ResetCanvas(Blueprint);

	// 透明背景：露出关卡相机里的 3D 实时画面；想改回纯色/图片就在 WBP 里改这个 Border。
	UBorder* Background = Blueprint->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
	Background->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
	Background->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UCanvasPanelSlot* BackgroundSlot = Root->AddChildToCanvas(Background))
	{
		BackgroundSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		BackgroundSlot->SetOffsets(FMargin(0.0f));
	}

	// 居中面板：尺寸/边距都可以直接在 WBP 里拖。
	UBorder* Panel = Blueprint->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MenuPanel"));
	Panel->SetBrushColor(FLinearColor(0.05f, 0.06f, 0.10f, 0.94f));
	Panel->SetPadding(FMargin(44.0f, 30.0f, 44.0f, 30.0f));
	AddToCanvas(Root, Panel, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D(0.0f, 0.0f), FVector2D(620.0f, 660.0f));

	UVerticalBox* MenuBox = Blueprint->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MenuBox"));
	Panel->SetContent(MenuBox);

	const auto AddMenuText = [&](FName Name, const FString& Text, const FLinearColor& Color, int32 FontSize,
		ETextJustify::Type Justification, const FMargin& Padding)
	{
		UTextBlock* TextBlock = MakeTextBlock(Blueprint, Name, Text, Color, FontSize, Justification, true);
		if (UVerticalBoxSlot* Slot = MenuBox->AddChildToVerticalBox(TextBlock))
		{
			Slot->SetPadding(Padding);
			Slot->SetHorizontalAlignment(HAlign_Fill);
		}
	};

	AddMenuText(TEXT("TitleText"), TEXT("Baka！"), FLinearColor(0.96f, 0.94f, 1.0f), 64, ETextJustify::Center, FMargin(0.0f));
	AddMenuText(TEXT("SubtitleText"), TEXT("俯视角动作生存 · 联机合作"), FLinearColor(0.62f, 0.68f, 0.82f), 20,
		ETextJustify::Center, FMargin(0.0f, 0.0f, 0.0f, 22.0f));

	MakeMenuButton(Blueprint, MenuBox, TEXT("SinglePlayerButton"), TEXT("单人游戏"), 26);
	MakeMenuButton(Blueprint, MenuBox, TEXT("MultiplayerButton"), TEXT("多人游戏（暂未开放）"), 26);
	MakeMenuButton(Blueprint, MenuBox, TEXT("SkillTreeButton"), TEXT("技能树（暂未开放）"), 26);
	MakeMenuButton(Blueprint, MenuBox, TEXT("TutorialButton"), TEXT("教程说明"), 26);
	MakeMenuButton(Blueprint, MenuBox, TEXT("SettingsButton"), TEXT("设置"), 26);
	MakeMenuButton(Blueprint, MenuBox, TEXT("QuitButton"), TEXT("退出游戏"), 26);

	AddMenuText(TEXT("HintText"), TEXT(""), FLinearColor(1.0f, 0.82f, 0.35f), 18,
		ETextJustify::Center, FMargin(0.0f, 18.0f, 0.0f, 0.0f));

	Finish(Blueprint);
	return true;
}

bool URogueWidgetBuilder::UpdateTutorialText(UWidgetBlueprint* Blueprint, const FString& Title, const FString& Body)
{
	if (!Blueprint || !Blueprint->WidgetTree) { return false; }
	UTextBlock* TitleWidget = Cast<UTextBlock>(Blueprint->WidgetTree->FindWidget(TEXT("TitleText")));
	UTextBlock* BodyWidget = Cast<UTextBlock>(Blueprint->WidgetTree->FindWidget(TEXT("TutorialText")));
	if (!TitleWidget || !BodyWidget) { return false; }
	TitleWidget->SetText(FText::FromString(Title));
	BodyWidget->SetText(FText::FromString(Body));
	Finish(Blueprint);
	return true;
}

bool URogueWidgetBuilder::UpdateTextBlock(UWidgetBlueprint* Blueprint, FName WidgetName, const FString& Text)
{
	if (!Blueprint || !Blueprint->WidgetTree) { return false; }
	UTextBlock* TextWidget = Cast<UTextBlock>(Blueprint->WidgetTree->FindWidget(WidgetName));
	if (!TextWidget) { return false; }
	TextWidget->SetText(FText::FromString(Text));
	Finish(Blueprint);
	return true;
}

FString URogueWidgetBuilder::GetTextBlockText(UWidgetBlueprint* Blueprint, FName WidgetName)
{
	if (!Blueprint || !Blueprint->WidgetTree) { return FString(); }
	const UTextBlock* TextWidget = Cast<UTextBlock>(Blueprint->WidgetTree->FindWidget(WidgetName));
	return TextWidget != nullptr ? TextWidget->GetText().ToString() : FString();
}

bool URogueWidgetBuilder::BuildTutorial(UWidgetBlueprint* Blueprint)
{
    FString Json;
    if (!FFileHelper::LoadFileToString(Json,*(FPaths::ProjectDir()/TEXT("SourceArt/UI/tutorial_chapters.json")))) return false;
    return BuildTutorialFromJson(Blueprint,Json);
}

bool URogueWidgetBuilder::AddDeathScreenMainMenuButton(UWidgetBlueprint* Blueprint)
{
	if (Blueprint == nullptr || Blueprint->WidgetTree == nullptr) return false;
	UWidgetTree* Tree = Blueprint->WidgetTree;
	if (Tree->FindWidget(TEXT("MainMenuButton")) != nullptr) return true;
	UButton* Reference = Cast<UButton>(Tree->FindWidget(TEXT("Button_Quit")));
	UVerticalBox* Box = Reference != nullptr ? Cast<UVerticalBox>(Reference->GetParent()) : nullptr;
	if (Box == nullptr) return false;
	Blueprint->Modify(); Tree->Modify(); Box->Modify();
	UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("MainMenuButton"));
	Button->bIsVariable = true;
	Button->SetStyle(Reference->GetStyle());
	UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MainMenuButtonLabel"));
	Label->SetText(FText::FromString(TEXT("返回主菜单")));
	Label->SetJustification(ETextJustify::Center);
	if (UTextBlock* ReferenceLabel = Cast<UTextBlock>(Reference->GetContent()))
	{
		Label->SetFont(ReferenceLabel->GetFont());
		Label->SetColorAndOpacity(ReferenceLabel->GetColorAndOpacity());
	}
	Button->SetContent(Label);
	UVerticalBoxSlot* NewSlot = Cast<UVerticalBoxSlot>(Box->InsertChildAt(Box->GetChildIndex(Reference), Button));
	if (NewSlot == nullptr) return false;
	if (const UVerticalBoxSlot* ReferenceSlot = Cast<UVerticalBoxSlot>(Reference->Slot))
	{
		NewSlot->SetPadding(ReferenceSlot->GetPadding());
		NewSlot->SetSize(ReferenceSlot->GetSize());
		NewSlot->SetHorizontalAlignment(ReferenceSlot->GetHorizontalAlignment());
		NewSlot->SetVerticalAlignment(ReferenceSlot->GetVerticalAlignment());
	}
	Finish(Blueprint);
	return Blueprint->Status != BS_Error;
}

bool URogueWidgetBuilder::BuildMultiplayerPage(UWidgetBlueprint* Blueprint,int32 Page)
{
	if (!Blueprint || !Blueprint->WidgetTree) return false;
	Blueprint->Modify();
	UCanvasPanel* Root=ResetCanvas(Blueprint);
	UBorder* Background=Blueprint->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("Background"));
	Background->SetBrushColor(FLinearColor(.025f,.04f,.075f,.97f));
	AddToCanvas(Root,Background,FAnchors(0,0,1,1),FVector2D::ZeroVector,FVector2D::ZeroVector,FVector2D::ZeroVector);
	UVerticalBox* Box=Blueprint->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("Content"));
	AddToCanvas(Root,Box,FAnchors(.5f,.5f),FVector2D(.5f,.5f),FVector2D::ZeroVector,FVector2D(1000,800));
	auto Text=[&](const TCHAR* Name,const TCHAR* Value,int32 Size=22)
	{
		UTextBlock* Widget=MakeTextBlock(Blueprint,Name,Value,FLinearColor(.85f,.94f,1),Size,ETextJustify::Left,true);
		Box->AddChildToVerticalBox(Widget)->SetPadding(FMargin(4,8)); return Widget;
	};
	auto Input=[&](const TCHAR* Name,const TCHAR* Hint)
	{
		UEditableTextBox* Widget=Blueprint->WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(),Name);
		Widget->SetHintText(FText::FromString(Hint));
		Widget->WidgetStyle.SetFont(FSlateFontInfo(GetUIFontObject(),24));
		Widget->WidgetStyle.SetForegroundColor(FSlateColor(FLinearColor(.04f,.07f,.12f)));
		Widget->WidgetStyle.SetFocusedForegroundColor(FSlateColor(FLinearColor(.04f,.07f,.12f)));
		Box->AddChildToVerticalBox(Widget)->SetPadding(FMargin(4,8));
	};
	Text(TEXT("RoomTitle"),Page==2?TEXT("房间大厅"):(Page==1?TEXT("加入房间"):TEXT("多人游戏")),36);
	Text(TEXT("Description"),Page==2?TEXT("每位玩家点击准备，全部准备后由房主开始。"):(Page==1?TEXT("搜索同一局域网的房间，或输入房主 IP。 "):TEXT("在同一局域网下创建房间，与朋友一起开始冒险。")));
	if (Page==2) Text(TEXT("AddressText"),TEXT("房主地址："),20);
	if (Page!=2) Input(TEXT("PlayerNameInput"),TEXT("玩家昵称"));
	if (Page==0)
	{
		Input(TEXT("RoomNameInput"),TEXT("房间名称（可留空，默认 4 人房间）"));
		MakeMenuButton(Blueprint,Box,TEXT("CreateRoomButton"),TEXT("创建房间"),28);
		MakeMenuButton(Blueprint,Box,TEXT("JoinRoomButton"),TEXT("加入房间"),28);
	}
	else
	{
		if (Page==1) MakeMenuButton(Blueprint,Box,TEXT("RefreshButton"),TEXT("刷新局域网房间"),24);
		USizeBox* Size=Blueprint->WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),TEXT("ListSize"));
		Size->SetHeightOverride(Page==2?400:300);
		UScrollBox* List=Blueprint->WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(),TEXT("Entries"));
		Size->AddChild(List); Box->AddChildToVerticalBox(Size)->SetPadding(FMargin(4,12));
		if (Page==1)
		{
			Input(TEXT("IPInput"),TEXT("房主 IPv4，例如 192.168.1.10:7777"));
			MakeMenuButton(Blueprint,Box,TEXT("JoinIPButton"),TEXT("通过 IP 加入"),24);
		}
		else
		{
			MakeMenuButton(Blueprint,Box,TEXT("ReadyButton"),TEXT("准备"),26);
			MakeMenuButton(Blueprint,Box,TEXT("StartButton"),TEXT("开始游戏（房主）"),26);
		}
	}
	MakeMenuButton(Blueprint,Box,TEXT("BackButton"),Page==2?TEXT("离开房间"):TEXT("返回"),24);
	Text(TEXT("StatusText"),TEXT(""),21);
	Finish(Blueprint); return true;
}
bool URogueWidgetBuilder::BuildRoomEntry(UWidgetBlueprint* Blueprint)
{
	if (!Blueprint || !Blueprint->WidgetTree) return false;
	Blueprint->Modify(); Blueprint->WidgetTree->Modify();
	USizeBox* Root=Blueprint->WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),TEXT("RowSize")); Root->SetHeightOverride(80);
	Blueprint->WidgetTree->RootWidget=Root;
	UBorder* Border=Blueprint->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("RowBackground"));
	Border->SetBrushColor(FLinearColor(.08f,.13f,.2f,.95f)); Border->SetPadding(FMargin(14,8)); Root->AddChild(Border);
	UHorizontalBox* Box=Blueprint->WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),TEXT("RowContent")); Border->AddChild(Box);
	for (const TCHAR* Name : {TEXT("TitleText"),TEXT("DetailText")})
	{
		UTextBlock* Text=MakeTextBlock(Blueprint,Name,TEXT(""),FLinearColor(.85f,.95f,1),22,ETextJustify::Left,false);
		UHorizontalBoxSlot* Slot=Box->AddChildToHorizontalBox(Text); Slot->SetVerticalAlignment(VAlign_Center); Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	UButton* Button=Blueprint->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("ActionButton"));
	Button->AddChild(MakeTextBlock(Blueprint,TEXT("ActionLabel"),TEXT("加入"),FLinearColor::White,22,ETextJustify::Center,false));
	Box->AddChildToHorizontalBox(Button)->SetPadding(FMargin(12,4));
	Finish(Blueprint); return true;
}


bool URogueWidgetBuilder::BuildNightTransition(UWidgetBlueprint* Blueprint)
{
    if (!Blueprint || !Blueprint->WidgetTree) { return false; }
    Blueprint->Modify();
    UCanvasPanel* Root = ResetCanvas(Blueprint);
    UBorder* Background = Blueprint->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("NoticeBackground"));
    Background->SetBrushColor(FLinearColor(.025f, .035f, .10f, .88f));
    AddToCanvas(Root, Background, FAnchors(.5f, .12f), FVector2D(.5f, 0), FVector2D::ZeroVector, FVector2D(860, 206));
    const FLinearColor White(.93f, .96f, 1.f);
    const FLinearColor Ice(.55f, .91f, 1.f);
    MakeText(Blueprint, Root, TEXT("TitleText"), TEXT("暮色将至"), White, 30,
        FAnchors(.5f, .12f), FVector2D(.5f, 0), FVector2D(0, 20), FVector2D(800, 42))->SetJustification(ETextJustify::Center);
    MakeText(Blueprint, Root, TEXT("CountdownText"), TEXT("距离黑夜降临：30 秒"), Ice, 38,
        FAnchors(.5f, .12f), FVector2D(.5f, 0), FVector2D(0, 70), FVector2D(800, 54))->SetJustification(ETextJustify::Center);
    MakeText(Blueprint, Root, TEXT("DescriptionText"), TEXT("白天的战斗已结束。稍作休整，黑夜中敌群更加密集，增援来得更快。"), White, 19,
        FAnchors(.5f, .12f), FVector2D(.5f, 0), FVector2D(0, 142), FVector2D(820, 36))->SetJustification(ETextJustify::Center);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    return true;
}

FString URogueWidgetBuilder::DescribeWidgetTree(UWidgetBlueprint* Blueprint)
{
	if (Blueprint == nullptr || Blueprint->WidgetTree == nullptr)
	{
		return TEXT("<no widget tree>");
	}

	TArray<UWidget*> Widgets;
	Blueprint->WidgetTree->GetAllWidgets(Widgets);

	FString Result;
	for (const UWidget* Widget : Widgets)
	{
		Result += FString::Printf(TEXT("%s:%s\n"), *GetNameSafe(Widget->GetClass()), *GetNameSafe(Widget));
	}
	return Result;
}

FString URogueWidgetBuilder::DescribeTextFonts(UWidgetBlueprint* Blueprint)
{
	if (Blueprint == nullptr || Blueprint->WidgetTree == nullptr)
	{
		return TEXT("<no widget tree>");
	}

	TArray<UWidget*> Widgets;
	Blueprint->WidgetTree->GetAllWidgets(Widgets);

	FString Result;
	for (const UWidget* Widget : Widgets)
	{
		const UTextBlock* Text = Cast<UTextBlock>(Widget);
		if (Text == nullptr)
		{
			continue;
		}

		const FSlateFontInfo Font = Text->GetFont();
		Result += FString::Printf(TEXT("%s -> %s (%.0f)\n"),
			*GetNameSafe(Text),
			Font.FontObject != nullptr ? *Font.FontObject->GetPathName() : TEXT("<none>"),
			Font.Size);
	}
	return Result;
}
