#pragma once
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Texture2D.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"

/** 编辑器生成器的共同操作。调用者负责备份和保存；这里不重建现有界面。 */
namespace RogueWidgetAuthoring
{
inline bool Finish(UWidgetBlueprint *Blueprint, bool bStructural = true)
{
	if (!Blueprint)
	{
		return false;
	}
	if (bStructural)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	}
	else
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	}
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	Blueprint->MarkPackageDirty();
	return Blueprint->Status != BS_Error;
}

template <typename T>
T *FindOrCreate(UWidgetBlueprint *Blueprint, const TCHAR *Name, bool bModifyExisting = false)
{
	if (auto *Widget = FindObject<T>(Blueprint->WidgetTree, Name))
	{
		if (bModifyExisting)
		{
			Widget->Modify();
		}
		return Widget;
	}
	return Blueprint->WidgetTree->ConstructWidget<T>(T::StaticClass(), Name);
}

inline FSlateBrush ImageBrush(UTexture2D *Texture, FVector2D Size)
{
	FSlateBrush Brush;
	Brush.SetResourceObject(Texture);
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.SetImageSize(Size);
	Brush.Margin = FMargin(0);
	return Brush;
}
} // namespace RogueWidgetAuthoring
