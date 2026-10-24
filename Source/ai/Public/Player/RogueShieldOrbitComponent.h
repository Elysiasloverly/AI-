#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RogueShieldOrbitComponent.generated.h"

class ARogueCharacter;
class UStaticMesh;
class UStaticMeshComponent;

/** 护盾存在且当前值大于零时显示三个环绕模型；只负责表现，不参与碰撞和伤害。 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent, DisplayName = "护盾环绕组件"))
class AI_API URogueShieldOrbitComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URogueShieldOrbitComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void Activate(bool bReset = false) override;
	virtual void Deactivate() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "护盾环绕", meta = (DisplayName = "启用护盾环绕"))
	bool bOrbitEnabled = true;

	/** 不指定默认模型；使用所选静态网格自带的材质。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "护盾环绕", meta = (DisplayName = "护盾静态网格体"))
	TObjectPtr<UStaticMesh> ShieldMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "护盾环绕", meta = (DisplayName = "环绕半径", ClampMin = "0", Units = "cm"))
	float OrbitRadius = 110.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "护盾环绕", meta = (DisplayName = "旋转速度（度/秒）", ToolTip = "正值与负值控制相反的环绕方向；0 表示停留在当前位置。"))
	float OrbitSpeed = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "护盾环绕", meta = (DisplayName = "环绕高度偏移", Units = "cm", ToolTip = "相对于玩家胶囊中心的高度。"))
	float HeightOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "护盾环绕", meta = (DisplayName = "模型缩放"))
	FVector MeshScale = FVector(0.25f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "护盾环绕", meta = (DisplayName = "模型朝向偏移"))
	FRotator MeshRotation = FRotator::ZeroRotator;

	/** 运行时修改配置后可主动刷新；正常运行每帧也会检查复制到本机的护盾数值。 */
	UFUNCTION(BlueprintCallable, Category = "护盾环绕")
	void RefreshShieldVisuals();

	UFUNCTION(BlueprintPure, Category = "护盾环绕")
	bool IsShieldVisible() const { return bShieldVisible; }

	const TArray<TObjectPtr<UStaticMeshComponent>>& GetOrbitMeshes() const { return OrbitMeshes; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

private:
	void SetShieldVisible(bool bVisible);
	void DestroyOrbitMeshes();

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> OrbitMeshes;

	TWeakObjectPtr<ARogueCharacter> Character;
	double OrbitAngleDegrees = 0.0;
	bool bShieldVisible = false;
};
