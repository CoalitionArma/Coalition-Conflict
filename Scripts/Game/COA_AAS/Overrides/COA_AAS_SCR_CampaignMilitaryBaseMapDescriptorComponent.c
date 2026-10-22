//------------------------------------------------------------------------------------------------
//! Advance and Secure keeps the front line public: every base in the objective chain shows its real
//! owner's colour. Vanilla only shows an owner for HQs and bases inside your radio network, so at the
//! start every objective you haven't linked up to - the enemy's above all - showed as unknown (yellow).
//! HQs and bases outside the chain keep vanilla's rules (enemy HQs stay hidden).
modded class SCR_CampaignMilitaryBaseMapDescriptorComponent
{
	//------------------------------------------------------------------------------------------------
	override void HandleMapInfo(SCR_CampaignFaction playerFactionCampaign = null)
	{
		super.HandleMapInfo(playerFactionCampaign);

		COA_AAS_ConflictComponent aas = COA_AAS_ConflictComponent.GetInstance();
		if (!aas || !m_CampaignBase || m_CampaignBase.IsHQ() || !aas.IsInChain(m_CampaignBase))
			return;

		SCR_GameModeCampaign campaign = SCR_GameModeCampaign.GetInstance();
		Faction owner = m_CampaignBase.GetFaction();
		if (!campaign || !owner)
			return;

		EFactionMapID factionMapID = EFactionMapID.UNKNOWN;
		switch (owner.GetFactionKey())
		{
			case campaign.GetFactionKeyByEnum(SCR_ECampaignFaction.OPFOR): {factionMapID = EFactionMapID.EAST; break;};
			case campaign.GetFactionKeyByEnum(SCR_ECampaignFaction.BLUFOR): {factionMapID = EFactionMapID.WEST; break;};
			case campaign.GetFactionKeyByEnum(SCR_ECampaignFaction.INDFOR): {factionMapID = EFactionMapID.FIA; break;};
		}

		if (factionMapID == EFactionMapID.UNKNOWN)
			return;

		Item().SetFactionIndex(factionMapID);
		SCR_CampaignMapUIBase mapUI = m_CampaignBase.GetMapUI();
		if (mapUI)
			mapUI.UpdateBaseIcon(factionMapID);
	}
}
