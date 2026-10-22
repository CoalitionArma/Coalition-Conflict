//------------------------------------------------------------------------------------------------
//! The map's base icons pick their own colour: the owner only for HQs, Game Master, and bases inside the
//! player's radio coverage - every other base is neutral (yellow). Advance and Secure keeps the front
//! public, so objectives in the chain always show their owner, as Game Master sees them. HQs keep
//! vanilla's rules (enemy HQs stay hidden).
modded class SCR_CampaignMapUIBase
{
	//------------------------------------------------------------------------------------------------
	//! An objective in the Advance and Secure chain, shown to everyone
	protected bool COA_AAS_IsPublic()
	{
		COA_AAS_ConflictComponent aas = COA_AAS_ConflictComponent.GetInstance();
		return m_Base && aas && !m_Base.IsHQ() && aas.IsInChain(m_Base);
	}

	//------------------------------------------------------------------------------------------------
	override protected void InitBaseIcon()
	{
		super.InitBaseIcon();
		if (!COA_AAS_IsPublic())
			return;

		SetIconFaction(m_Base.GetCampaignFaction());
		SetBaseImage();
	}

	//------------------------------------------------------------------------------------------------
	//! The capture flash, and its return to the owner's colour, as for a base in radio range
	override void FlashBaseIcon(Faction faction = null, bool changeToDefault = false)
	{
		if (!COA_AAS_IsPublic())
		{
			super.FlashBaseIcon(faction, changeToDefault);
			return;
		}

		if (changeToDefault)
			SetBaseIconFactionColor(m_Base.GetCampaignFaction());
		else
			SetBaseIconFactionColor(faction);
	}
}
