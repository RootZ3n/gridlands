#include "Salvage/GLSalvageNode.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Salvage/GLSalvageableComponent.h"
#include "UObject/ConstructorHelpers.h"

AGLSalvageNode::AGLSalvageNode()
{
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Mesh->SetStaticMesh(Cube.Object);
	}
	Mesh->SetRelativeScale3D(FVector(0.8f, 0.8f, 0.8f));
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	Salvageable = CreateDefaultSubobject<UGLSalvageableComponent>(TEXT("Salvageable"));
}
