#include "World/GLGameMode.h"

#include "Character/GLCharacter.h"

AGLGameMode::AGLGameMode()
{
	DefaultPawnClass = AGLCharacter::StaticClass();
}
