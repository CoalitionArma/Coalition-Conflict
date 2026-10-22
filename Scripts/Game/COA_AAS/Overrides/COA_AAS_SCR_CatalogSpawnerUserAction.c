//------------------------------------------------------------------------------------------------
//! Depot and helipad requests show what an asset costs in XP (COA_AAS_AssetXPCost), and why it's
//! blocked when the player hasn't got it
modded class SCR_CatalogSpawnerUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		if (!super.GetActionNameScript(outName))
			return false;

		int cost = COA_AAS_AssetXPCost.GetCostOf(m_EntityData);
		if (cost <= 0)
			return true;

		// The request text is localised with parameters - fill them in so the XP cost can follow it
		outName = WidgetManager.Translate(outName, ActionNameParams[0], ActionNameParams[1], ActionNameParams[2]) + string.Format(" | %1 XP", cost);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		bool canPerform = super.CanBePerformedScript(user);
		if (m_iRequestStatus == SCR_EEntityRequestStatus.COA_AAS_NOT_ENOUGH_XP)
		{
			int cost = COA_AAS_AssetXPCost.GetCostOf(m_EntityData);
			int xp = COA_AAS_AssetXPCost.GetPlayerXP(user);
			SetCannotPerformReason(string.Format("Needs %1 XP (you have %2)", cost, Math.Max(xp, 0)));
			return false;
		}

		return canPerform;
	}
}
