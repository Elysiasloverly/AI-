#include "RogueEditorAssetUtils.h"

#include "EditorAssetLibrary.h"
#include "UObject/Package.h"

bool URogueEditorAssetUtils::SaveAsset(UObject* Asset)
{
	if (Asset == nullptr)
	{
		return false;
	}

	Asset->MarkPackageDirty();
	if (UPackage* Package = Asset->GetOutermost())
	{
		Package->SetDirtyFlag(true);
	}

	return UEditorAssetLibrary::SaveAsset(Asset->GetPathName(), false);
}
