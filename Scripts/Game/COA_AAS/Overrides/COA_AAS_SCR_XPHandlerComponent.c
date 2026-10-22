//------------------------------------------------------------------------------------------------
//! Slower ranks: Conflict's XP rewards (kills, captures, tasks, deliveries...) are scaled by the AAS game
//! mode's XP gain multiplier on top of the server's own. Penalties scale with it too - Conflict applies one
//! multiplier to every reward. Custom XP (AAS rewards, asset XP costs) doesn't pass through here.
modded class SCR_XPHandlerComponent
{
	//------------------------------------------------------------------------------------------------
	override float GetXPMultiplier()
	{
		float multiplier = super.GetXPMultiplier();
		COA_AAS_ConflictComponent aas = COA_AAS_ConflictComponent.GetInstance();
		if (aas)
			multiplier *= aas.GetXPGainMultiplier();

		return multiplier;
	}
}
