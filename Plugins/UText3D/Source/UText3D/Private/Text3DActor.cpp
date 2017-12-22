#include "Text3DActor.h"

AText3DActor::AText3DActor()
{
	RootComponent = TextComponent = CreateDefaultSubobject<UText3DComponent>(FName("Text"));
}
