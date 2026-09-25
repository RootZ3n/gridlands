#include "Pehlichi/GLPehlichi.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Pehlichi/GLCapabilityComponent.h"
#include "Pehlichi/GLCompanionPositioningComponent.h"
#include "Pehlichi/GLPehlichiCommandComponent.h"
#include "Pehlichi/GLRepairComponent.h"
#include "Pehlichi/GLScanComponent.h"
#include "UObject/ConstructorHelpers.h"

AGLPehlichi::AGLPehlichi()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	SetRootComponent(Body);
	Body->SetStaticMesh(Sphere.Object);
	Body->SetRelativeScale3D(FVector(0.4f, 0.3f, 0.35f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Tail = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Tail"));
	Tail->SetupAttachment(Body);
	Tail->SetStaticMesh(Cone.Object);
	Tail->SetRelativeLocation(FVector(-60.f, 0.f, 40.f));
	Tail->SetRelativeRotation(FRotator(-30.f, 0.f, 0.f));
	Tail->SetRelativeScale3D(FVector(0.8f, 0.8f, 1.6f));
	Tail->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Commands = CreateDefaultSubobject<UGLPehlichiCommandComponent>(TEXT("Commands"));
	Scan = CreateDefaultSubobject<UGLScanComponent>(TEXT("Scan"));
	Repair = CreateDefaultSubobject<UGLRepairComponent>(TEXT("Repair"));
	Capabilities = CreateDefaultSubobject<UGLCapabilityComponent>(TEXT("Capabilities"));
	Positioning = CreateDefaultSubobject<UGLCompanionPositioningComponent>(TEXT("Positioning"));
	// Starting levels (a new world); later loaded from the world save (M9).
	Capabilities->Grant(TEXT("capability.pehlichi.scan"), 1);
	Capabilities->Grant(TEXT("capability.pehlichi.distract"), 1);
	Capabilities->Grant(TEXT("capability.pehlichi.analysis"), 1);
}
