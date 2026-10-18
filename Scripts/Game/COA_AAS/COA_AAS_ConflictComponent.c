//------------------------------------------------------------------------------------------------
//! Advance & Secure for Conflict.
//! The bases listed in m_aObjectiveChain (ordered from the BLUFOR HQ side to the OPFOR HQ side, HQs
//! not included) form a line where only one base - the live objective - can be seized at a time:
//!  - At the start the live objective is the base neither side owns (the CHDKz-held centre), open to both.
//!  - Whoever captures the live objective keeps attacking: the next base toward the enemy becomes live.
//!  - Tug of war: if the defenders hold the live objective for m_fDefenseHoldTime (the clock pauses
//!    while it is being seized), the roles swap and they counterattack the attackers' front base.
//!  - Capturing the last objective before the enemy HQ wins the match.
//! Seizing and seize tasks are gated in COA_AAS_SCR_CampaignSeizingComponent and
//! COA_AAS_SCR_CampaignMilitaryBaseComponent.
[ComponentEditorProps(category: "COALITION/Conflict", description: "Linear advance and secure for Conflict - one capturable base at a time")]
class COA_AAS_ConflictComponentClass : SCR_BaseGameModeComponentClass
{
}

class COA_AAS_ConflictComponent : SCR_BaseGameModeComponent
{
	[Attribute(desc: "Base entity names in order, from the BLUFOR HQ side to the OPFOR HQ side (HQs not included)", category: "Advance and Secure")]
	protected ref array<string> m_aObjectiveChain;

	[Attribute("900", desc: "How long the defenders must hold the live objective before they counterattack [s]. 0 = never swap.", params: "0 inf 1", category: "Advance and Secure")]
	protected float m_fDefenseHoldTime;

	[Attribute("300", desc: "Tell everyone this long before a defence hold completes [s]. 0 = no warning.", params: "0 inf 1", category: "Advance and Secure")]
	protected float m_fHoldWarningTime;

	//! Index into m_aObjectiveChain of the only base that can be seized, -1 before setup / after the match
	[RplProp()]
	protected int m_iLiveIndex = -1;

	//! SCR_ECampaignFaction of the attacking side, -1 while the live objective is open to both sides
	[RplProp()]
	protected int m_iAttacker = -1;

	//! Seconds left on the defence clock, -1 while it doesn't run (objective open to both / held by CHDKz)
	[RplProp()]
	protected int m_iHoldRemaining = -1;

	//! The defence clock is paused because the live objective is being seized
	[RplProp()]
	protected bool m_bHoldPaused;

	protected ref array<SCR_CampaignMilitaryBaseComponent> m_aChain = {};
	protected SCR_GameModeCampaign m_Campaign;
	protected bool m_bActive;
	protected float m_fHeldFor;		// how long the defenders have held the live objective [s]
	protected bool m_bHoldWarned;

	protected static COA_AAS_ConflictComponent s_Instance;
	protected static const int TICK_MS = 1000;
	protected static const int SETUP_DELAY_MS = 2000;
	protected static const int MAP_REFRESH_DELAY_MS = 1500;
	protected static const float NOTIFICATION_DURATION = 8;

	//------------------------------------------------------------------------------------------------
	static COA_AAS_ConflictComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! False until the chain is set up (and on clients) - the gates then leave vanilla Conflict alone
	bool IsAASActive()
	{
		return m_bActive;
	}

	//------------------------------------------------------------------------------------------------
	//! The only base that can currently be seized
	bool IsLiveObjective(notnull SCR_CampaignMilitaryBaseComponent base)
	{
		SCR_CampaignMilitaryBaseComponent live = GetLiveObjective();
		return live && live == base;
	}

	//------------------------------------------------------------------------------------------------
	//! Works on clients too: the chain is resolved by entity name on first use
	SCR_CampaignMilitaryBaseComponent GetLiveObjective()
	{
		if (m_aChain.IsEmpty())
			ResolveChain();

		if (!m_aChain.IsIndexValid(m_iLiveIndex))
			return null;

		return m_aChain[m_iLiveIndex];
	}

	//------------------------------------------------------------------------------------------------
	// Read-only state for UI (COA_AAS_WelcomeScreenView) - all of it is replicated or resolved locally
	int GetChainCount()
	{
		if (m_aChain.IsEmpty())
			ResolveChain();

		return m_aChain.Count();
	}

	//------------------------------------------------------------------------------------------------
	SCR_CampaignMilitaryBaseComponent GetChainBase(int index)
	{
		if (m_aChain.IsEmpty())
			ResolveChain();

		if (!m_aChain.IsIndexValid(index))
			return null;

		return m_aChain[index];
	}

	//------------------------------------------------------------------------------------------------
	int GetLiveIndex()
	{
		return m_iLiveIndex;
	}

	//------------------------------------------------------------------------------------------------
	//! SCR_ECampaignFaction of the attacking side, -1 while the live objective is open to both sides
	int GetAttacker()
	{
		return m_iAttacker;
	}

	//------------------------------------------------------------------------------------------------
	//! Seconds left before the defenders counterattack, -1 while the clock doesn't run
	int GetHoldRemaining()
	{
		return m_iHoldRemaining;
	}

	//------------------------------------------------------------------------------------------------
	bool IsHoldPaused()
	{
		return m_bHoldPaused;
	}

	//------------------------------------------------------------------------------------------------
	float GetDefenseHoldTime()
	{
		return m_fDefenseHoldTime;
	}

	//------------------------------------------------------------------------------------------------
	//! True for a base in the chain - HQs and anything else on the map stay out of the line
	bool IsInChain(notnull SCR_CampaignMilitaryBaseComponent base)
	{
		if (m_aChain.IsEmpty())
			ResolveChain();

		return m_aChain.Contains(base);
	}

	//------------------------------------------------------------------------------------------------
	override void OnGameModeStart()
	{
		super.OnGameModeStart();

		m_Campaign = SCR_GameModeCampaign.Cast(GetGameMode());
		if (!m_Campaign)
		{
			Print("[COA_AAS] Needs SCR_GameModeCampaign - advance and secure is off", LogLevel.ERROR);
			return;
		}

		if (RplSession.Mode() != RplMode.Dedicated)
			m_Campaign.GetOnFactionAssignedLocalPlayer().Insert(OnLocalPlayerFactionAssigned);

		if (!Replication.IsServer())
			return;

		SCR_CampaignMilitaryBaseManager baseManager = m_Campaign.GetBaseManager();
		if (!baseManager)
			return;

		// Base ownership is only final once Conflict has set up HQs and default owners
		if (baseManager.IsBasesInitDone())
			OnBasesInitialized();
		else
			baseManager.GetOnAllBasesInitialized().Insert(OnBasesInitialized);
	}

	//------------------------------------------------------------------------------------------------
	//! Leave Conflict a moment to finish its own start-up (HQs, coverage) before taking over
	protected void OnBasesInitialized()
	{
		GetGame().GetCallqueue().CallLater(Setup, SETUP_DELAY_MS);
	}

	//------------------------------------------------------------------------------------------------
	//! Owners preset in the world (each base's SCR_FactionAffiliationComponent) are already set when the
	//! base loads, so Conflict never sees an ownership change for them and skips everything it does in
	//! SCR_CampaignMilitaryBaseComponent.OnFactionChanged: the radio is never tuned to the owner (no
	//! radio coverage - the map shows the bases as unknown) and the spawn point is never opened to the
	//! owner. Hand every owned base over again so Conflict sets it up, then rebuild the coverage.
	protected void ApplyPresetOwners()
	{
		SCR_CampaignMilitaryBaseManager baseManager = m_Campaign.GetBaseManager();
		array<SCR_CampaignMilitaryBaseComponent> bases = {};
		baseManager.GetBases(bases);

		SCR_CampaignFaction noOwner;
		foreach (SCR_CampaignMilitaryBaseComponent base : bases)
		{
			SCR_CampaignFaction owner = base.GetCampaignFaction();
			if (!owner || !base.IsInitialized())
				continue;

			base.SetFaction(noOwner);
			base.SetFaction(owner);
		}

		SCR_CampaignFaction blufor = m_Campaign.GetFactionByEnum(SCR_ECampaignFaction.BLUFOR);
		if (blufor)
			baseManager.RecalculateRadioCoverageForced(blufor);

		SCR_CampaignFaction opfor = m_Campaign.GetFactionByEnum(SCR_ECampaignFaction.OPFOR);
		if (opfor)
			baseManager.RecalculateRadioCoverageForced(opfor);

		LogBases("server, after hand-over");
	}

	//------------------------------------------------------------------------------------------------
	//! The map's base icons are only redrawn on ownership/coverage changes and when the local player
	//! gets a faction - redraw them all once more after the faction is in, so they match the start state
	protected void OnLocalPlayerFactionAssigned(Faction faction)
	{
		GetGame().GetCallqueue().CallLater(RefreshMapIcons, MAP_REFRESH_DELAY_MS);
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshMapIcons()
	{
		SCR_CampaignFaction playerFaction = SCR_CampaignFaction.Cast(SCR_FactionManager.SGetLocalPlayerFaction());
		if (!playerFaction || !m_Campaign || !m_Campaign.GetBaseManager())
			return;

		array<SCR_CampaignMilitaryBaseComponent> bases = {};
		m_Campaign.GetBaseManager().GetBases(bases);
		foreach (SCR_CampaignMilitaryBaseComponent base : bases)
		{
			SCR_CampaignMilitaryBaseMapDescriptorComponent descriptor = base.GetMapDescriptor();
			if (descriptor)
				descriptor.HandleMapInfo(playerFaction);
		}

		LogBases("client, player faction " + playerFaction.GetFactionKey());
	}

	//------------------------------------------------------------------------------------------------
	//! Diagnostics for the map/radio start state: owner, HQ flag, radio and each side's coverage per base
	protected void LogBases(string when)
	{
		SCR_CampaignFaction blufor = m_Campaign.GetFactionByEnum(SCR_ECampaignFaction.BLUFOR);
		SCR_CampaignFaction opfor = m_Campaign.GetFactionByEnum(SCR_ECampaignFaction.OPFOR);
		string bluforHQ = "none";
		if (blufor && blufor.GetMainBase())
			bluforHQ = blufor.GetMainBase().GetBaseName();

		string opforHQ = "none";
		if (opfor && opfor.GetMainBase())
			opforHQ = opfor.GetMainBase().GetBaseName();

		Print(string.Format("[COA_AAS] Bases (%1) - BLUFOR main base: %2, OPFOR main base: %3", when, bluforHQ, opforHQ), LogLevel.NORMAL);

		array<SCR_CampaignMilitaryBaseComponent> bases = {};
		m_Campaign.GetBaseManager().GetBases(bases);
		foreach (SCR_CampaignMilitaryBaseComponent base : bases)
		{
			string owner = "none";
			if (base.GetCampaignFaction())
				owner = base.GetCampaignFaction().GetFactionKey();

			string radioState = "no radio";
			SCR_CoverageRadioComponent radio = SCR_CoverageRadioComponent.Cast(base.GetOwner().FindComponent(SCR_CoverageRadioComponent));
			if (radio)
			{
				float range = -1;
				if (radio.TransceiversCount() > 0 && radio.GetTransceiver(0))
					range = radio.GetTransceiver(0).GetRange();

				radioState = string.Format("powered %1, key '%2', range %3", radio.IsPowered(), radio.GetEncryptionKey(), range);
			}

			bool bluforCoverage, opforCoverage;
			if (blufor)
				bluforCoverage = base.IsHQRadioTrafficPossible(blufor);
			if (opfor)
				opforCoverage = base.IsHQRadioTrafficPossible(opfor);

			Print(string.Format("[COA_AAS]   %1: owner %2, HQ %3, init %4, %5, BLUFOR coverage %6, OPFOR coverage %7", base.GetBaseName(), owner, base.IsHQ(), base.IsInitialized(), radioState, bluforCoverage, opforCoverage), LogLevel.NORMAL);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void ResolveChain()
	{
		m_aChain.Clear();
		if (!m_aObjectiveChain)
			return;

		BaseWorld world = GetGame().GetWorld();
		foreach (string baseName : m_aObjectiveChain)
		{
			IEntity entity = world.FindEntityByName(baseName);
			SCR_CampaignMilitaryBaseComponent base;
			if (entity)
				base = SCR_CampaignMilitaryBaseComponent.Cast(entity.FindComponent(SCR_CampaignMilitaryBaseComponent));

			if (!base)
			{
				Print(string.Format("[COA_AAS] Objective '%1' is not a Conflict base in this world - advance and secure is off", baseName), LogLevel.ERROR);
				m_aChain.Clear();
				return;
			}

			m_aChain.Insert(base);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void Setup()
	{
		ResolveChain();
		if (m_aChain.IsEmpty())
			return;

		// Before listening to ownership changes, so these hand-overs don't move the front
		ApplyPresetOwners();

		// The opening objective is the one neither side owns (the CHDKz-held centre), open to both sides.
		// Without one, BLUFOR opens on the first base it doesn't own.
		int liveIndex = -1;
		int attacker = -1;
		foreach (int i, SCR_CampaignMilitaryBaseComponent base : m_aChain)
		{
			int owner = GetFactionEnum(base.GetCampaignFaction());
			if (owner != SCR_ECampaignFaction.BLUFOR && owner != SCR_ECampaignFaction.OPFOR)
			{
				liveIndex = i;
				break;
			}
		}

		if (liveIndex == -1)
		{
			attacker = SCR_ECampaignFaction.BLUFOR;
			liveIndex = GetNextTarget(attacker);
			if (liveIndex == -1)
			{
				Print("[COA_AAS] BLUFOR already owns every objective - nothing to fight over", LogLevel.ERROR);
				return;
			}
		}

		m_bActive = true;
		SCR_CampaignMilitaryBaseComponent.GetOnFactionChangedExtended().Insert(OnBaseFactionChanged);
		SetLive(liveIndex, attacker);
		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);

		Notify("Advance and Secure", string.Format("First objective: %1", GetBaseName(liveIndex)));
	}

	//------------------------------------------------------------------------------------------------
	protected void SetLive(int index, int attacker)
	{
		m_iLiveIndex = index;
		m_iAttacker = attacker;
		m_fHeldFor = 0;
		m_bHoldWarned = false;
		m_iHoldRemaining = -1;
		m_bHoldPaused = false;
		Replication.BumpMe();

		// Anyone already standing on the new objective is counted from the next seizing evaluation
		if (m_aChain.IsIndexValid(index))
			m_aChain[index].RefreshSeizingTimers();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBaseFactionChanged(SCR_CampaignMilitaryBaseComponent base, Faction previousFaction, Faction faction)
	{
		if (!m_bActive || m_aChain.Find(base) != m_iLiveIndex)
			return;

		int captor = GetFactionEnum(SCR_CampaignFaction.Cast(faction));
		if (captor != SCR_ECampaignFaction.BLUFOR && captor != SCR_ECampaignFaction.OPFOR)
			return;

		string captured = GetBaseName(m_iLiveIndex);
		int next = m_iLiveIndex + GetDirection(captor);
		if (!m_aChain.IsIndexValid(next))
		{
			Victory(captor, captured);
			return;
		}

		SetLive(next, captor);
		Notify(string.Format("%1 secured %2", GetFactionName(captor), captured), string.Format("Next objective: %1", GetBaseName(next)));
	}

	//------------------------------------------------------------------------------------------------
	//! Defence clock: runs while the defending side owns the live objective and nobody is seizing it
	protected void Tick()
	{
		if (!m_bActive || m_iAttacker == -1 || m_fDefenseHoldTime <= 0 || !m_aChain.IsIndexValid(m_iLiveIndex))
			return;

		SCR_CampaignMilitaryBaseComponent base = m_aChain[m_iLiveIndex];
		int defender = GetFactionEnum(base.GetCampaignFaction());

		// Only a side defends - a CHDKz-held objective in the way just has to be taken
		if (defender != SCR_ECampaignFaction.BLUFOR && defender != SCR_ECampaignFaction.OPFOR || defender == m_iAttacker)
		{
			SetHoldState(-1, false);
			return;
		}

		bool paused = base.GetCapturingFaction() != null;
		if (!paused)
			m_fHeldFor += TICK_MS * 0.001;

		float remaining = m_fDefenseHoldTime - m_fHeldFor;
		SetHoldState(Math.Max(0, Math.Ceil(remaining)), paused);
		if (paused)
			return;

		if (m_fHoldWarningTime > 0 && !m_bHoldWarned && remaining <= m_fHoldWarningTime)
		{
			m_bHoldWarned = true;
			Notify(string.Format("%1 has %2 minutes to take %3", GetFactionName(m_iAttacker), Math.Ceil(remaining / 60), GetBaseName(m_iLiveIndex)), string.Format("Otherwise %1 counterattacks", GetFactionName(defender)));
		}

		if (remaining <= 0)
			Swap(defender);
	}

	//------------------------------------------------------------------------------------------------
	//! Replicates the defence clock for the UI, only when it changes
	protected void SetHoldState(int remaining, bool paused)
	{
		if (remaining == m_iHoldRemaining && paused == m_bHoldPaused)
			return;

		m_iHoldRemaining = remaining;
		m_bHoldPaused = paused;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! The defenders held: they attack the attackers' front base next
	protected void Swap(int newAttacker)
	{
		string held = GetBaseName(m_iLiveIndex);
		int next = m_iLiveIndex + GetDirection(newAttacker);
		if (!m_aChain.IsIndexValid(next))
		{
			// Nothing of the attackers' left in the line to counterattack - just restart the clock
			m_fHeldFor = 0;
			m_bHoldWarned = false;
			return;
		}

		SetLive(next, newAttacker);
		Notify(string.Format("%1 held %2", GetFactionName(newAttacker), held), string.Format("%1 counterattacks: %2", GetFactionName(newAttacker), GetBaseName(next)));
	}

	//------------------------------------------------------------------------------------------------
	protected void Victory(int winner, string lastObjective)
	{
		m_bActive = false;
		m_iLiveIndex = -1;
		Replication.BumpMe();
		GetGame().GetCallqueue().Remove(Tick);
		SCR_CampaignMilitaryBaseComponent.GetOnFactionChangedExtended().Remove(OnBaseFactionChanged);

		Notify(string.Format("%1 secured %2", GetFactionName(winner), lastObjective), string.Format("%1 has won the battle", GetFactionName(winner)));
		m_Campaign.COA_AAS_EndMatch(m_Campaign.GetFactionByEnum(winner));
	}

	//------------------------------------------------------------------------------------------------
	//! First base from the attacker's end of the line that the attacker doesn't own, -1 if none
	protected int GetNextTarget(int attacker)
	{
		int count = m_aChain.Count();
		for (int n = 0; n < count; n++)
		{
			int i = n;
			if (attacker == SCR_ECampaignFaction.OPFOR)
				i = count - 1 - n;

			if (GetFactionEnum(m_aChain[i].GetCampaignFaction()) != attacker)
				return i;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! BLUFOR pushes up the chain, OPFOR down it
	protected int GetDirection(int faction)
	{
		if (faction == SCR_ECampaignFaction.OPFOR)
			return -1;

		return 1;
	}

	//------------------------------------------------------------------------------------------------
	protected int GetFactionEnum(SCR_CampaignFaction faction)
	{
		if (!faction || !m_Campaign)
			return -1;

		if (faction == m_Campaign.GetFactionByEnum(SCR_ECampaignFaction.BLUFOR))
			return SCR_ECampaignFaction.BLUFOR;

		if (faction == m_Campaign.GetFactionByEnum(SCR_ECampaignFaction.OPFOR))
			return SCR_ECampaignFaction.OPFOR;

		if (faction == m_Campaign.GetFactionByEnum(SCR_ECampaignFaction.INDFOR))
			return SCR_ECampaignFaction.INDFOR;

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	protected string GetFactionName(int factionEnum)
	{
		SCR_CampaignFaction faction = m_Campaign.GetFactionByEnum(factionEnum);
		if (!faction)
			return "?";

		return faction.GetFactionName();
	}

	//------------------------------------------------------------------------------------------------
	protected string GetBaseName(int index)
	{
		if (!m_aChain.IsIndexValid(index))
			return "?";

		return m_aChain[index].GetBaseName();
	}

	//------------------------------------------------------------------------------------------------
	protected void Notify(string title, string subtitle)
	{
		Rpc(RpcDo_Notify, title, subtitle);

		// Broadcast RPCs skip the server - show it there too unless nobody plays on it
		if (RplSession.Mode() != RplMode.Dedicated)
			RpcDo_Notify(title, subtitle);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_Notify(string title, string subtitle)
	{
		SCR_PopUpNotification popup = SCR_PopUpNotification.GetInstance();
		if (popup)
			popup.PopupMsg(title, NOTIFICATION_DURATION, subtitle);
	}

	//------------------------------------------------------------------------------------------------
	void COA_AAS_ConflictComponent(IEntityComponentSource src, IEntity ent, IEntity parent)
	{
		s_Instance = this;
	}

	//------------------------------------------------------------------------------------------------
	void ~COA_AAS_ConflictComponent()
	{
		if (s_Instance == this)
			s_Instance = null;

		if (GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().Remove(Tick);
			GetGame().GetCallqueue().Remove(Setup);
			GetGame().GetCallqueue().Remove(RefreshMapIcons);
		}

		if (m_bActive)
			SCR_CampaignMilitaryBaseComponent.GetOnFactionChangedExtended().Remove(OnBaseFactionChanged);
	}
}
