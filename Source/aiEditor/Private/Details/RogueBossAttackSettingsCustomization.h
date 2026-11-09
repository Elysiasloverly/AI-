#pragma once

#include "IPropertyTypeCustomization.h"

class FRogueBossAttackSettingsCustomization : public IPropertyTypeCustomization
{
public:
    static TSharedRef<IPropertyTypeCustomization> MakeInstance();
    virtual void CustomizeHeader(TSharedRef<IPropertyHandle> Struct, FDetailWidgetRow& Header,
        IPropertyTypeCustomizationUtils& Utils) override;
    virtual void CustomizeChildren(TSharedRef<IPropertyHandle> Struct, IDetailChildrenBuilder& Children,
        IPropertyTypeCustomizationUtils& Utils) override;
};
