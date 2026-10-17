//------------------------------------------------------------------------------------------------
//! Advance & Secure: at any objective in the line that isn't the live one, only its owners count -
//! attackers standing there can neither seize nor contest it (COA_AAS_ConflictComponent)
modded class SCR_CampaignSeizingComponent
{
	//------------------------------------------------------------------------------------------------
	override protected SCR_Faction EvaluateEntityFaction(IEntity ent)
	{
		SCR_Faction faction = super.EvaluateEntityFaction(ent);
		if (!faction || !m_Base)
			return faction;

		COA_AAS_ConflictComponent aas = COA_AAS_ConflictComponent.GetInstance();
		if (!aas || !aas.IsAASActive() || !aas.IsInChain(m_Base) || aas.IsLiveObjective(m_Base))
			return faction;

		if (faction != m_Base.GetFaction())
			return null;

		return faction;
	}
}
