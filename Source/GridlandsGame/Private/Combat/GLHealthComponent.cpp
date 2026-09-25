#include "Combat/GLHealthComponent.h"

double UGLHealthComponent::ApplyDamage(double Amount, AActor* Instigator)
{
	if (Health.IsDead())
	{
		return 0.0;
	}
	const double Taken = Health.Damage(Amount);
	if (Taken > 0.0)
	{
		OnDamaged.Broadcast(Taken, Instigator);
	}
	if (Health.IsDead())
	{
		OnDied.Broadcast(Instigator);
	}
	return Taken;
}
