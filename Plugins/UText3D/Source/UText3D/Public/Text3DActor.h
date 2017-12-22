#pragma once

#include "Text3DComponent.h"
#include "Text3DActor.generated.h"

UCLASS(hideCategories = (Collision, Attachment, Actor))
class UTEXT3D_API AText3DActor : public AActor
{
	GENERATED_BODY()

public:
	AText3DActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	UText3DComponent* TextComponent;
};