#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

/**
 * 弹体/表现 Actor 的碰撞消毒。
 *
 * 规则：只保留命中球（QueryOnly）参与查询；其它 Primitive（蓝图里加的网格、粒子碰撞等）
 * 一律关闭物理模拟、碰撞与重叠事件，避免它们阻挡角色、互相推挤或参与物理。
 */
namespace RogueCombatCollision
{
	inline void DisableNonHitCollision(AActor* Actor, const UPrimitiveComponent* HitCollision)
	{
		if (Actor == nullptr)
		{
			return;
		}

		TArray<UPrimitiveComponent*> Primitives;
		Actor->GetComponents<UPrimitiveComponent>(Primitives);
		for (UPrimitiveComponent* Primitive : Primitives)
		{
			if (Primitive == nullptr)
			{
				continue;
			}

			Primitive->SetSimulatePhysics(false);
			if (Primitive == HitCollision)
			{
				// 命中球只做查询；即使蓝图把它改成 QueryAndPhysics/PhysicsCollision，也拉回 QueryOnly。
				Primitive->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
				continue;
			}

			Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Primitive->SetGenerateOverlapEvents(false);
		}
	}
}
