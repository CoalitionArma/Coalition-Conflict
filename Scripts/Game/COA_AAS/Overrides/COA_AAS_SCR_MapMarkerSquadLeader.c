//------------------------------------------------------------------------------------------------
//! Squad leader markers
//!  - WCS_Core (via the WCS Stryker) skips OnUpdate while the marker has no widget, but vanilla re-checks
//!    who may see the marker at the top of OnUpdate - before there's a widget. A marker hidden once (anyone's
//!    while the local player wasn't in a group yet - every marker on a fresh join) then never came back.
//!    The check runs here first; it creates the widget when the marker should show, and WCS's guard and
//!    vanilla's update carry on from there.
//!  - Leaders see their own group's marker too (vanilla hides it from them), as in COALITION-Lobby
modded class SCR_MapMarkerSquadLeader
{
	//------------------------------------------------------------------------------------------------
	override void OnUpdate()
	{
		if (m_bDoLocalVisibilityUpdate)
			UpdateLocalVisibility();

		super.OnUpdate();
	}

	//------------------------------------------------------------------------------------------------
	//! Vanilla's rules, minus hiding the leader's own marker from them
	override void UpdateLocalVisibility()
	{
		m_bDoLocalVisibilityUpdate = false;

		PlayerController pController = GetGame().GetPlayerController();
		if (!pController)
			return;

		SCR_GroupsManagerComponent groupManager = SCR_GroupsManagerComponent.GetInstance();
		if (!groupManager)
			return;

		SCR_AIGroup localPlayerGroup = groupManager.GetPlayerGroup(pController.GetPlayerId());
		if (!localPlayerGroup)
		{
			SetLocalVisible(false);
			return;
		}

		bool isLocalPlayerLeader = localPlayerGroup.IsPlayerLeader(pController.GetPlayerId());

		if (isLocalPlayerLeader && (CanLeaderSeeOtherLeaders() || m_PlayerID == pController.GetPlayerId()))
		{
			SetLocalVisible(true);
			return;
		}

		if (!isLocalPlayerLeader && (CanMemberSeeOtherLeaders() || localPlayerGroup.IsPlayerInGroup(m_PlayerID)))
		{
			SetLocalVisible(true);
			return;
		}

		SetLocalVisible(false);
	}
}
