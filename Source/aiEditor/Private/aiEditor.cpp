#include "Modules/ModuleManager.h"

#include "Details/RogueBossAttackSettingsCustomization.h"
#include "Enemies/Data/RogueBossAttackSettings.h"
#include "PropertyEditorModule.h"

class FRogueEditorModule : public IModuleInterface
{
public:
    void StartupModule() override
    {
        if (IsRunningCommandlet()) return;
        auto& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
        PropertyEditor.RegisterCustomPropertyTypeLayout(FRogueBossAttackSettings::StaticStruct()->GetFName(),
            FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FRogueBossAttackSettingsCustomization::MakeInstance));
    }

    void ShutdownModule() override
    {
        if (IsRunningCommandlet()) return;
        if (auto* PropertyEditor = FModuleManager::GetModulePtr<FPropertyEditorModule>(TEXT("PropertyEditor")))
            PropertyEditor->UnregisterCustomPropertyTypeLayout(FRogueBossAttackSettings::StaticStruct()->GetFName());
    }
};

IMPLEMENT_MODULE(FRogueEditorModule, aiEditor);
