#include "Structure/GLStructurePart.h"

#include "Salvage/GLSalvageableComponent.h"

AGLStructurePart::AGLStructurePart()
{
	Salvageable = CreateDefaultSubobject<UGLSalvageableComponent>(TEXT("Salvageable"));
}
