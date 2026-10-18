//------------------------------------------------------------------------------------------------
//! Rank-unlocked loadouts: vanilla only enforces a loadout's catalog required rank when the spawn
//! request reaches the server (SCR_GameModeCampaign.CanPlayerSpawn_S -> NOT_ALLOWED_RANK_TOO_LOW).
//! Grey out loadouts above the local player's rank in the deploy menu so they read as locked unlocks
//! instead of failing on deploy.
modded class SCR_LoadoutGallery
{
	//------------------------------------------------------------------------------------------------
	override int AddItem(SCR_BasePlayerLoadout loadout, bool enabled = true)
	{
		if (enabled && loadout && COA_AAS_ConflictComponent.GetInstance() && !COA_AAS_HasRequiredRank(loadout))
			enabled = false;

		return super.AddItem(loadout, enabled);
	}

	//------------------------------------------------------------------------------------------------
	protected bool COA_AAS_HasRequiredRank(notnull SCR_BasePlayerLoadout loadout)
	{
		PlayerController pc = GetGame().GetPlayerController();
		if (!pc)
			return true;

		SCR_PlayerXPHandlerComponent xpHandler = SCR_PlayerXPHandlerComponent.Cast(pc.FindComponent(SCR_PlayerXPHandlerComponent));
		if (!xpHandler)
			return true;

		return xpHandler.GetPlayerRankByXP() >= SCR_BasePlayerLoadout.GetLoadoutRequiredRank(loadout, pc.GetPlayerId());
	}
}
