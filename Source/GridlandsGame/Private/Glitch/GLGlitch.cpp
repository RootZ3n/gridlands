#include "Glitch/GLGlitch.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Glitch/GLGlitchComponent.h"
#include "UObject/ConstructorHelpers.h"

AGLGlitch::AGLGlitch()
{
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		Mesh->SetStaticMesh(Sphere.Object);
	}
	Mesh->SetRelativeScale3D(FVector(1.2f));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // never an interaction target
	Mesh->SetVisibility(false);
	Glitch = CreateDefaultSubobject<UGLGlitchComponent>(TEXT("Glitch"));
}

void AGLGlitch::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	Glitch->OnStateChanged.AddUObject(this, &AGLGlitch::UpdateVisual);
	UpdateVisual(Glitch->GetState(), Glitch->GetState());
}

void AGLGlitch::UpdateVisual(EGLGlitchState From, EGLGlitchState To)
{
	// Debug visualization only; the finished Grid shader is out of scope (VISUAL-DIRECTION).
	Mesh->SetVisibility(FGLGlitchLifecycle::IsVisibleToPlayer(To) && To != EGLGlitchState::Repaired);
}
