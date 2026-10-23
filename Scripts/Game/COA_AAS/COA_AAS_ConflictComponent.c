//------------------------------------------------------------------------------------------------
//! Advance & Secure for Conflict.
//! The bases listed in m_aObjectiveChain (ordered from the BLUFOR HQ side to the OPFOR HQ side, HQs
//! not included) form a line where only one base - the live objective - can be seized at a time:
//!  - At the start the live objective is the base neither side owns (the CHDKz-held centre), open to both.
//!  - Whoever captures the live objective keeps attacking: the next base toward the enemy becomes live.
//!  - Tug of war: if the defenders hold the live objective for m_fDefenseHoldTime (the clock pauses
//!    while it is being seized), the roles swap and they counterattack the attackers' front base.
//!  - Owning every objective in the line (every capturable base - the HQs can't be taken) wins the match.
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

	[Attribute("1", desc: "Every objective in the line and both HQs count as in HQ radio range for both sides - no relays or radio network needed to capture, spawn or get supplies there", category: "Advance and Secure")]
	protected bool m_bCommsEverywhere;

	[Attribute("0", desc: "Players can respawn at the objectives in the line they own - else only at the HQs and FOBs", category: "Logistics")]
	protected bool m_bObjectiveRespawn;

	[Attribute("0", desc: "Supplies each objective in the line starts with - more has to be trucked in. -1 = Conflict's own (random) amount.", params: "-1 10000 1", category: "Logistics")]
	protected int m_iObjectiveStartingSupplies;

	[Attribute("0", desc: "Bases other than the HQs (objectives, FOBs) generate supplies by themselves - else only the HQs do and the rest is trucked in", category: "Logistics")]
	protected bool m_bBaseSupplyIncome;

	[Attribute("10000", desc: "Supplies each HQ/MOB can hold. Income stops while storage is full, so keep this above the HQ starting supplies. 0 = Conflict's own (its supply piles, ~1000 for the HQ tent).", params: "0 100000 1", category: "Logistics")]
	protected int m_iHQStorageCapacity;

	[Attribute("1", desc: "Command trucks can build FOBs (Conflict's establish base) anywhere in their side's territory, without a commander or task. Needs the frontline for the territory - without it, anywhere.", category: "Logistics")]
	protected bool m_bTerritoryFOBs;

	[Attribute("1", desc: "Only Crewman loadouts can drive or man the guns of armour (APCs, IFVs, tanks), and only Pilot loadouts can fly aircraft - passenger seats and door guns stay open to anyone (COA_AAS_VehicleRoles)", category: "Logistics")]
	protected bool m_bVehicleRoles;

	[Attribute("600", desc: "Ground around the live objective counted for the capture bonus [m]. Needs the frontline.", params: "0 3000 1", category: "Capture Bonus")]
	protected float m_fGroundRadius;

	[Attribute("0.25", desc: "Share of that ground the attackers must hold before seizing speeds up", params: "0 1 0.01", category: "Capture Bonus")]
	protected float m_fGroundMinShare;

	[Attribute("0.5", desc: "How much shorter seizing gets when the attackers hold all the ground around it (0.5 = half the time)", params: "0 0.9 0.01", category: "Capture Bonus")]
	protected float m_fGroundMaxSpeedup;

	[Attribute("1", desc: "Seizing the live objective takes longer on a full server and less time on a quiet one", category: "Capture Time")]
	protected bool m_bPopulationCaptureTime;

	[Attribute("20", desc: "At this many players or fewer, seizing takes the low multiplier of its time", params: "0 256 1", category: "Capture Time")]
	protected int m_iPopulationLowPlayers;

	[Attribute("0.5", desc: "Seizing time multiplier at the low player count (0.5 = half as long)", params: "0.1 5 0.05", category: "Capture Time")]
	protected float m_fPopulationLowMultiplier;

	[Attribute("100", desc: "At this many players or more, seizing takes the high multiplier of its time", params: "1 256 1", category: "Capture Time")]
	protected int m_iPopulationHighPlayers;

	[Attribute("1.5", desc: "Seizing time multiplier at the high player count (1.5 = half as long again)", params: "0.1 5 0.05", category: "Capture Time")]
	protected float m_fPopulationHighMultiplier;

	[Attribute("0.3", desc: "Scales all XP players earn (Conflict's rewards and the ones below) - lower = slower ranks and unlocks. Stacks with the server's own XP multiplier. Asset XP costs aren't scaled.", params: "0.05 5 0.05", category: "Rewards")]
	protected float m_fXPGainMultiplier;

	[Attribute("10", desc: "XP each player on a territory square gets when their side takes it", params: "0 1000 1", category: "Rewards")]
	protected int m_iSquareCaptureXP;

	[Attribute("10", desc: "Extra XP per 100 supplies delivered to an objective in the line or a FOB (on top of Conflict's own delivery XP)", params: "0 1000 1", category: "Rewards")]
	protected int m_iFrontSupplyXP;

	[Attribute("150", desc: "XP each player of the side near a FOB gets when it's built", params: "0 5000 1", category: "Rewards")]
	protected int m_iFOBBuiltXP;

	[Attribute("100", desc: "How close to a new FOB players must be to share its XP [m]", params: "0 1000 1", category: "Rewards")]
	protected float m_fFOBBuiltRadius;

	[Attribute(desc: "XP that powerful assets cost to request from vehicle depots and helipads, on top of their supplies. The first matching rule applies; anything unmatched costs no XP.", category: "Asset XP Cost")]
	protected ref array<ref COA_AAS_AssetXPCost> m_aAssetXPCosts;

	[Attribute("1", desc: "Track the territory each side holds as squares and show it on the map, with the front line between them", category: "Frontline")]
	protected bool m_bFrontline;

	[Attribute("100", desc: "Territory square size [m]", params: "25 1000 1", category: "Frontline")]
	protected float m_fFrontlineCellSize;

	[Attribute("1", desc: "The territory covers the whole terrain - else only the bases and the margin round them. Sea squares are nobody's.", category: "Frontline")]
	protected bool m_bFrontlineWholeMap;

	[Attribute("1500", desc: "Without whole map: how far the territory reaches past the outermost bases [m]", params: "0 10000 1", category: "Frontline")]
	protected float m_fFrontlineMargin;

	[Attribute("0", desc: "At the start each square goes to the nearest base's owner, if that base is within this distance [m]. 0 = no limit.", params: "0 20000 1", category: "Frontline")]
	protected float m_fFrontlineSeedRadius;

	[Attribute("30", desc: "How long one side's players must hold a square to take it - one away from their territory takes the shortest strip back to it along [s]", params: "1 600 1", category: "Frontline")]
	protected float m_fFrontlineCaptureTime;

	[Attribute("250", desc: "A captured base takes the squares within this distance with it [m]", params: "0 2000 1", category: "Frontline")]
	protected float m_fFrontlineBaseRadius;

	[Attribute("5", desc: "How often player positions are checked [s]", params: "1 60 1", category: "Frontline")]
	protected float m_fFrontlineTick;

	[Attribute("1", desc: "Players deep in enemy territory have their rough position pinged on the enemy's map - keeps the fighting at the front. Needs the frontline.", category: "Behind Enemy Lines")]
	protected bool m_bBehindLines;

	[Attribute("300", desc: "How far past their own territory players can go before they're pinged [m]", params: "0 5000 1", category: "Behind Enemy Lines")]
	protected float m_fBehindLinesGrace;

	[Attribute("1500", desc: "From this far past their own territory, pings come every min interval [m]", params: "0 10000 1", category: "Behind Enemy Lines")]
	protected float m_fBehindLinesDeep;

	[Attribute("60", desc: "Seconds between pings just past the grace distance", params: "5 600 1", category: "Behind Enemy Lines")]
	protected float m_fBehindLinesMaxInterval;

	[Attribute("15", desc: "Seconds between pings at the deep distance and beyond", params: "5 600 1", category: "Behind Enemy Lines")]
	protected float m_fBehindLinesMinInterval;

	[Attribute("400", desc: "Nobody is pinged this close to the live objective [m]", params: "0 3000 1", category: "Behind Enemy Lines")]
	protected float m_fBehindLinesObjectiveRadius;

	[Attribute("500", desc: "Nobody is pinged on the way from their side's ground to the live objective or the enemy's front-line objective: a straight push is always clear, and a detour up to this much longer than straight is too (flanks). Past the objective or wider still counts [m]", params: "0 5000 1", category: "Behind Enemy Lines")]
	protected float m_fBehindLinesApproachSlack;

	[Attribute("45", desc: "How long a ping stays on the enemy's map [s]", params: "5 600 1", category: "Behind Enemy Lines")]
	protected float m_fBehindLinesPingDuration;

	[Attribute("1", desc: "CHDKz patrol the front line and now and then attack a base on it (COA_AAS_Insurgents). Needs the frontline.", category: "CHDKz Insurgents")]
	protected bool m_bInsurgents;

	[Attribute(desc: "Groups sent out on patrol, one picked at random each time", params: "et", category: "CHDKz Insurgents")]
	protected ref array<ResourceName> m_aInsurgentPatrolGroups;

	[Attribute(desc: "Groups sent in to attack, one picked at random each time", params: "et", category: "CHDKz Insurgents")]
	protected ref array<ResourceName> m_aInsurgentAttackGroups;

	[Attribute("10", desc: "Patrols along the front at a time. They only have soldiers while players are near, so this costs little.", params: "0 100 1", category: "CHDKz Insurgents")]
	protected int m_iInsurgentMaxPatrols;

	[Attribute("2", desc: "Attacks under way at a time", params: "0 10 1", category: "CHDKz Insurgents")]
	protected int m_iInsurgentMaxAttacks;

	[Attribute("400", desc: "Patrols are placed up to this far from the front line [m]", params: "0 3000 1", category: "CHDKz Insurgents")]
	protected float m_fInsurgentFrontDistance;

	[Attribute("250", desc: "Radius of a patrol's loop [m]", params: "50 2000 1", category: "CHDKz Insurgents")]
	protected float m_fInsurgentPatrolRadius;

	[Attribute("1000", desc: "A patrol nobody is near is moved once the front is this far away from it [m]", params: "100 10000 1", category: "CHDKz Insurgents")]
	protected float m_fInsurgentRetireDistance;

	[Attribute("300", desc: "Shortest time between attacks [s]", params: "30 7200 1", category: "CHDKz Insurgents")]
	protected float m_fInsurgentAttackIntervalMin;

	[Attribute("600", desc: "Longest time between attacks [s]", params: "30 7200 1", category: "CHDKz Insurgents")]
	protected float m_fInsurgentAttackIntervalMax;

	[Attribute("600", desc: "Attackers start about this far from their target [m]", params: "100 3000 1", category: "CHDKz Insurgents")]
	protected float m_fInsurgentAttackDistance;

	[Attribute("800", desc: "Besides the live objective, bases this close to the front can be attacked [m]", params: "0 5000 1", category: "CHDKz Insurgents")]
	protected float m_fInsurgentAttackFrontRange;

	[Attribute("350", desc: "Insurgents never spawn this close to a player [m]", params: "0 2000 1", category: "CHDKz Insurgents")]
	protected float m_fInsurgentMinPlayerDistance;

	[Attribute(desc: "Groups that lie in wait beside roads behind the front, one picked at random each time", params: "et", category: "CHDKz Insurgents")]
	protected ref array<ResourceName> m_aInsurgentAmbushGroups;

	[Attribute("4", desc: "Road ambushes behind the front at a time. Dormant until players come near, like patrols.", params: "0 50 1", category: "CHDKz Insurgents")]
	protected int m_iInsurgentMaxAmbushes;

	[Attribute("500", desc: "Ambushes are at least this far behind the front [m]", params: "0 10000 1", category: "CHDKz Insurgents")]
	protected float m_fInsurgentAmbushMinBehind;

	[Attribute("3000", desc: "Ambushes are at most this far behind the front [m]", params: "0 20000 1", category: "CHDKz Insurgents")]
	protected float m_fInsurgentAmbushMaxBehind;

	[Attribute("900", desc: "An ambush nobody has come near in this long moves to another road [s]", params: "60 7200 1", category: "CHDKz Insurgents")]
	protected float m_fInsurgentAmbushLifetime;

	[Attribute("3", desc: "Only roads at least this wide are ambushed (skips footpaths) [m]", params: "0 20 0.5", category: "CHDKz Insurgents")]
	protected float m_fInsurgentAmbushMinRoadWidth;

	[Attribute("0", desc: "Insurgents can seize bases they don't hold (FOBs, objectives held by a side). Off: they fight there and kill, but only players take ground.", category: "CHDKz Insurgents")]
	protected bool m_bInsurgentsCanSeize;

	//! The territory, run-length encoded (COA_AAS_Territory) - empty until the frontline starts
	[RplProp(onRplName: "OnTerritoryReplicated")]
	protected string m_sTerritory;

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
	protected ref COA_AAS_MapLines m_MapLines;	// capture order on the map, where someone plays
	protected ref COA_AAS_Frontline m_Frontline;	// server: the territory
	protected ref COA_AAS_FrontlineMap m_FrontlineMap;	// where someone plays: the territory on the map
	protected ref COA_AAS_Territory m_Territory = new COA_AAS_Territory();	// everywhere: who holds where (FOB placement)
	protected bool m_bHasTerritory;
	protected int m_iTerritoryVersion;
	protected ref COA_AAS_BehindLines m_BehindLines;	// server: pings players deep in enemy territory
	protected ref COA_AAS_Insurgents m_Insurgents;	// server: CHDKz patrols and attacks along the front
	protected float m_fNextSpottedHint;	// where someone plays: world time [ms] the next "enemy spotted" hint may show

	protected static COA_AAS_ConflictComponent s_Instance;
	protected static const int TICK_MS = 1000;
	protected static const int SETUP_DELAY_MS = 2000;
	protected static const int MAP_REFRESH_DELAY_MS = 1500;
	protected static const float BEHIND_LINES_TICK = 2;
	protected static const float BEHIND_LINES_HINT_DURATION = 5;
	protected static const float SPOTTED_HINT_COOLDOWN_MS = 30000;
	protected static const int TASK_SYNC_DELAY_MS = 1000;
	protected static const int POPULATION_RETIME_DELAY_MS = 1000;
	protected static const int HQ_TOP_UP_DELAY_MS = 6000;		// after Setup: the services spawn 4 s after bases init
	protected static const int HQ_TOP_UP_RETRY_MS = 2000;
	protected static const int HQ_TOP_UP_ATTEMPTS = 5;
	protected static const int FOB_PILES_DELAY_MS = 5000;
	protected static const int RADIO_KEY_DELAY_MS = 1000;
	protected int m_iHQTopUpAttempts;
	protected ref array<SCR_CampaignMilitaryBaseComponent> m_aHQsEnlarged = {};	// HQs whose storage was raised (m_iHQStorageCapacity)
	protected static const int TASK_SYNC_INTERVAL_MS = 5000;
	protected static const ResourceName HOLD_TASK_PREFAB = "{A14BCD8C52D8F686}Prefabs/Tasks/Campaign/HoldCampaignMilitaryBaseTaskEntity.et";
	protected int m_iTaskSerial;
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
	//! Is the base always in HQ radio range (m_bCommsEverywhere) - the line and the HQs. Everywhere,
	//! from the start: the server's spawn points and supplies and every map read it. By entity name, as
	//! Conflict asks while the world is still loading (before every base in the line exists).
	bool HasComms(notnull SCR_CampaignMilitaryBaseComponent base)
	{
		if (!m_bCommsEverywhere)
			return false;

		return base.IsHQ() || base.GetBuiltByPlayers() || IsObjective(base);
	}

	//------------------------------------------------------------------------------------------------
	bool IsCommsEverywhere()
	{
		return m_bCommsEverywhere;
	}

	//------------------------------------------------------------------------------------------------
	//! Is the base one of the objectives in the line - by entity name, safe while the world loads
	bool IsObjective(notnull SCR_CampaignMilitaryBaseComponent base)
	{
		return m_aObjectiveChain && m_aObjectiveChain.Contains(base.GetOwner().GetName());
	}

	//------------------------------------------------------------------------------------------------
	//! How much of its usual time seizing the live objective takes for the faction
	//! (COA_AAS_SCR_CampaignSeizingComponent): the server's population scales it, and holding the ground
	//! around it shortens it. 1 = Conflict's own time.
	float GetCaptureTimeFactor(notnull SCR_CampaignMilitaryBaseComponent base, Faction faction)
	{
		if (!IsLiveObjective(base))
			return 1;

		return GetPopulationCaptureFactor() * GetGroundCaptureFactor(base, faction);
	}

	//------------------------------------------------------------------------------------------------
	//! Seizing time multiplier for the players on the server: the low multiplier at the low count or fewer,
	//! the high one at the high count or more, straight between
	float GetPopulationCaptureFactor()
	{
		if (!m_bPopulationCaptureTime || m_iPopulationHighPlayers <= m_iPopulationLowPlayers)
			return 1;

		float players = GetGame().GetPlayerManager().GetPlayerCount();
		float range = m_iPopulationHighPlayers - m_iPopulationLowPlayers;
		float t = Math.Clamp((players - m_iPopulationLowPlayers) / range, 0, 1);
		return Math.Lerp(m_fPopulationLowMultiplier, m_fPopulationHighMultiplier, t);
	}

	//------------------------------------------------------------------------------------------------
	//! Capture bonus: shorter the more of the ground around the objective the faction holds. 1 = no bonus.
	protected float GetGroundCaptureFactor(notnull SCR_CampaignMilitaryBaseComponent base, Faction faction)
	{
		if (m_fGroundMaxSpeedup <= 0 || m_fGroundRadius <= 0 || !m_bHasTerritory)
			return 1;

		int side = COA_AAS_Territory.OwnerOf(m_Campaign, faction);
		if (side == COA_AAS_ETerritoryOwner.NONE)
			return 1;

		float share = GroundShare(base.GetOwner().GetOrigin(), side);
		float t = 1;
		if (m_fGroundMinShare < 1)
			t = Math.Clamp((share - m_fGroundMinShare) / (1 - m_fGroundMinShare), 0, 1);
		else if (share < 1)
			t = 0;

		return 1 - m_fGroundMaxSpeedup * t;
	}

	//------------------------------------------------------------------------------------------------
	//! Share of the land squares within m_fGroundRadius of the position the side holds
	float GroundShare(vector position, int side)
	{
		int cx, cz;
		m_Territory.CellAt(position, cx, cz);
		float cell = m_Territory.m_fCell;
		int reach = Math.Ceil(m_fGroundRadius / cell);
		float radiusSq = m_fGroundRadius * m_fGroundRadius;

		int land;
		float held;
		for (int z = cz - reach; z <= cz + reach; z++)
		{
			for (int x = cx - reach; x <= cx + reach; x++)
			{
				int owner = m_Territory.OwnerAt(x, z);
				if (owner == COA_AAS_ETerritoryOwner.NONE)
					continue;

				vector centre = m_Territory.CellCentre(x, z);
				if (vector.DistanceSqXZ(centre, position) > radiusSq)
					continue;

				land++;
				if (owner == side)
					held++;
			}
		}

		if (land == 0)
			return 0;

		return held / land;
	}

	//------------------------------------------------------------------------------------------------
	//! Server: bring both HQs up to the game mode's starting supplies. Conflict's own fill happens while only
	//! the HQ tent's storage exists, so it stops at that; the service storages (COA_AAS_BaseServicesComponent)
	//! register a few seconds later - retried until every HQ has it, a few times at most.
	protected void TopUpHQs()
	{
		int target = m_Campaign.COA_AAS_GetHQStartingSupplies();
		bool belowTarget;
		array<int> sides = {SCR_ECampaignFaction.BLUFOR, SCR_ECampaignFaction.OPFOR};
		foreach (int side : sides)
		{
			SCR_CampaignFaction faction = m_Campaign.GetFactionByEnum(side);
			if (!faction || !faction.GetMainBase())
				continue;

			SCR_CampaignMilitaryBaseComponent hq = faction.GetMainBase();

			// Room above the starting amount, or income has nowhere to go (COA_AAS_SupplyStorage)
			if (m_iHQStorageCapacity > 0 && !m_aHQsEnlarged.Contains(hq))
			{
				if (COA_AAS_SupplyStorage.RaiseCapacity(hq, Math.Max(m_iHQStorageCapacity, target)))
					m_aHQsEnlarged.Insert(hq);
				else
					belowTarget = true;
			}

			if (hq.GetSupplies() >= target)
				continue;

			hq.SetSupplies(target);
			belowTarget = true;
		}

		m_iHQTopUpAttempts++;
		if (belowTarget && m_iHQTopUpAttempts < HQ_TOP_UP_ATTEMPTS)
			GetGame().GetCallqueue().CallLater(TopUpHQs, HQ_TOP_UP_RETRY_MS);
	}

	//------------------------------------------------------------------------------------------------
	//! Server (COA_AAS_Frontline): these players stood on a square their side just took
	void OnSquareTaken(array<int> playerIds)
	{
		if (!playerIds || m_iSquareCaptureXP <= 0)
			return;

		foreach (int playerId : playerIds)
		{
			AwardXP(playerId, m_iSquareCaptureXP);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server: supplies unloaded - extra XP when it was at an objective in the line or a FOB
	protected void OnSuppliesDelivered(int playerId, int amount, int total)
	{
		if (m_iFrontSupplyXP <= 0 || amount <= 0 || !m_Campaign)
			return;

		IEntity player = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		SCR_CampaignMilitaryBaseManager baseManager = m_Campaign.GetBaseManager();
		if (!player || !baseManager)
			return;

		SCR_CampaignMilitaryBaseComponent base = baseManager.FindClosestBase(player.GetOrigin());
		if (!base || base.IsHQ() || vector.DistanceXZ(player.GetOrigin(), base.GetOwner().GetOrigin()) > base.GetRadius() + 50)
			return;

		if (!IsObjective(base) && !base.GetBuiltByPlayers())
			return;

		int xp = Math.Round(m_iFrontSupplyXP * amount / 100);
		if (xp > 0)
			AwardXP(playerId, xp);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a FOB went up - the side's players around it built it
	protected void OnBaseBuilt(SCR_CampaignMilitaryBaseComponent base, Faction faction)
	{
		// Its supply pile stays visible when empty, once the HQ composition is up (COA_AAS_SupplyStorage)
		if (base)
			GetGame().GetCallqueue().CallLater(ShowSupplyPiles, FOB_PILES_DELAY_MS, false, base);

		if (m_iFOBBuiltXP <= 0 || !base || !faction)
			return;

		float radiusSq = m_fFOBBuiltRadius * m_fFOBBuiltRadius;
		vector position = base.GetOwner().GetOrigin();
		array<int> playerIds = {};
		GetGame().GetPlayerManager().GetPlayers(playerIds);
		foreach (int playerId : playerIds)
		{
			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId));
			if (!character || character.GetFaction() != faction || vector.DistanceSqXZ(character.GetOrigin(), position) > radiusSq)
				continue;

			AwardXP(playerId, m_iFOBBuiltXP);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowSupplyPiles(SCR_CampaignMilitaryBaseComponent base)
	{
		if (base)
			COA_AAS_SupplyStorage.KeepPilesVisible(base);
	}

	//------------------------------------------------------------------------------------------------
	protected void AwardXP(int playerId, int xp)
	{
		// Custom XP skips Conflict's multiplier, so scale it here
		xp = Math.Round(xp * m_fXPGainMultiplier);
		if (xp <= 0)
			return;

		SCR_XPHandlerComponent handler = SCR_XPHandlerComponent.Cast(GetGameMode().FindComponent(SCR_XPHandlerComponent));
		if (handler)
			handler.AwardXP(playerId, SCR_EXPRewards.CUSTOM_19, 1, false, xp);
	}

	//------------------------------------------------------------------------------------------------
	//! Scales all XP players earn (COA_AAS_SCR_XPHandlerComponent)
	float GetXPGainMultiplier()
	{
		return m_fXPGainMultiplier;
	}

	//------------------------------------------------------------------------------------------------
	//! XP an entry costs from a depot or helipad (first matching rule), 0 if none match
	int GetAssetXPCost(notnull SCR_EntityCatalogEntry entry)
	{
		if (!m_aAssetXPCosts)
			return 0;

		foreach (COA_AAS_AssetXPCost rule : m_aAssetXPCosts)
		{
			if (rule && rule.Matches(entry))
				return rule.GetXPCost();
		}

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	bool IsObjectiveRespawnAllowed()
	{
		return m_bObjectiveRespawn;
	}

	//------------------------------------------------------------------------------------------------
	//! \return supplies an objective starts with, -1 to leave it to Conflict
	int GetObjectiveStartingSupplies()
	{
		return m_iObjectiveStartingSupplies;
	}

	//------------------------------------------------------------------------------------------------
	bool IsBaseSupplyIncomeAllowed()
	{
		return m_bBaseSupplyIncome;
	}

	//------------------------------------------------------------------------------------------------
	bool AreTerritoryFOBsEnabled()
	{
		return m_bTerritoryFOBs;
	}

	//------------------------------------------------------------------------------------------------
	bool AreVehicleRolesEnabled()
	{
		return m_bVehicleRoles;
	}

	//------------------------------------------------------------------------------------------------
	//! Is a position in the faction's territory. True everywhere when there's no frontline to ask.
	bool IsInTerritory(vector position, Faction faction)
	{
		if (!m_bFrontline || !m_bHasTerritory)
			return true;

		int side = COA_AAS_Territory.OwnerOf(m_Campaign, faction);
		return side != COA_AAS_ETerritoryOwner.NONE && m_Territory.OwnerAtPosition(position) == side;
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
		{
			m_Campaign.GetOnFactionAssignedLocalPlayer().Insert(OnLocalPlayerFactionAssigned);
			m_MapLines = new COA_AAS_MapLines(this);
			if (m_bFrontline)
			{
				m_FrontlineMap = new COA_AAS_FrontlineMap();
				OnTerritoryReplicated();
			}
		}

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

		// Conflict fills an HQ when its tent goes up, before the services (and most of the storage) exist
		m_iHQTopUpAttempts = 0;
		GetGame().GetCallqueue().CallLater(TopUpHQs, HQ_TOP_UP_DELAY_MS);

		SCR_CampaignNetworkComponent.GetOnSuppliesDelivered().Insert(OnSuppliesDelivered);
		if (m_Campaign.GetBaseManager())
			m_Campaign.GetBaseManager().GetOnBaseBuilt().Insert(OnBaseBuilt);

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
		StartFrontline();
		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
		GetGame().GetCallqueue().CallLater(SyncTasks, TASK_SYNC_INTERVAL_MS, true);

		// The score bar from the start: each side's objectives (later changes re-count via the base manager)
		if (m_Campaign.GetBaseManager())
			m_Campaign.GetBaseManager().EvaluateControlPoints();

		Notify("Advance and Secure", string.Format("First objective: %1", GetBaseName(liveIndex)));
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the territory, over the objective chain and both HQs
	protected void StartFrontline()
	{
		if (!m_bFrontline)
			return;

		array<SCR_CampaignMilitaryBaseComponent> bases = {};
		bases.InsertAll(m_aChain);
		SCR_CampaignFaction blufor = m_Campaign.GetFactionByEnum(SCR_ECampaignFaction.BLUFOR);
		if (blufor && blufor.GetMainBase())
			bases.Insert(blufor.GetMainBase());

		SCR_CampaignFaction opfor = m_Campaign.GetFactionByEnum(SCR_ECampaignFaction.OPFOR);
		if (opfor && opfor.GetMainBase())
			bases.Insert(opfor.GetMainBase());

		m_Frontline = new COA_AAS_Frontline(this, m_Campaign, m_fFrontlineCellSize, m_bFrontlineWholeMap, m_fFrontlineMargin, m_fFrontlineSeedRadius, m_fFrontlineCaptureTime, m_fFrontlineBaseRadius, m_fFrontlineTick);
		m_Frontline.Start(bases);

		if (m_bBehindLines)
			m_BehindLines = new COA_AAS_BehindLines(this, m_Campaign, m_fBehindLinesGrace, m_fBehindLinesDeep, m_fBehindLinesMaxInterval, m_fBehindLinesMinInterval, m_fBehindLinesObjectiveRadius, m_fBehindLinesApproachSlack, BEHIND_LINES_TICK);

		if (m_bInsurgents)
		{
			m_Insurgents = new COA_AAS_Insurgents(this, m_Campaign, m_aInsurgentPatrolGroups, m_aInsurgentAttackGroups, m_iInsurgentMaxPatrols, m_iInsurgentMaxAttacks,
				m_fInsurgentFrontDistance, m_fInsurgentPatrolRadius, m_fInsurgentRetireDistance, m_fInsurgentAttackIntervalMin, m_fInsurgentAttackIntervalMax,
				m_fInsurgentAttackDistance, m_fInsurgentAttackFrontRange, m_fInsurgentMinPlayerDistance);
			m_Insurgents.SetAmbushes(m_aInsurgentAmbushGroups, m_iInsurgentMaxAmbushes, m_fInsurgentAmbushMinBehind, m_fInsurgentAmbushMaxBehind,
				m_fInsurgentAmbushLifetime, m_fInsurgentAmbushMinRoadWidth);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Server (COA_AAS_Insurgents): CHDKz are moving on a base - both sides hear of it
	void NotifyInsurgentAttack(string baseName)
	{
		string insurgents = "CHDKz";
		SCR_CampaignFaction faction = m_Campaign.GetFactionByEnum(SCR_ECampaignFaction.INDFOR);
		if (faction)
			insurgents = faction.GetFactionName();

		Notify(string.Format("%1 attack", insurgents), string.Format("%1 are moving on %2", insurgents, baseName));
	}

	//------------------------------------------------------------------------------------------------
	//! Whether CHDKz standing at the base count toward seizing it - always where they hold it, elsewhere
	//! only when m_bInsurgentsCanSeize
	bool InsurgentsCountAt(notnull SCR_CampaignMilitaryBaseComponent base, Faction faction)
	{
		if (m_bInsurgentsCanSeize || !m_Campaign || !faction)
			return true;

		if (faction != m_Campaign.GetFactionByEnum(SCR_ECampaignFaction.INDFOR))
			return true;

		return base.GetFaction() == faction;
	}

	//------------------------------------------------------------------------------------------------
	//! Who holds where, null until the frontline has published it
	COA_AAS_Territory GetTerritory()
	{
		if (!m_bHasTerritory)
			return null;

		return m_Territory;
	}

	//------------------------------------------------------------------------------------------------
	//! Goes up every time the territory changes
	int GetTerritoryVersion()
	{
		return m_iTerritoryVersion;
	}

	//------------------------------------------------------------------------------------------------
	//! Server (COA_AAS_BehindLines): a player of the side is deep in enemy territory, roughly here
	void RevealBehindLines(int side, vector position)
	{
		Rpc(RpcDo_RevealBehindLines, side, position);
		if (RplSession.Mode() != RplMode.Dedicated)
			RpcDo_RevealBehindLines(side, position);
	}

	//------------------------------------------------------------------------------------------------
	//! Everyone: only the other side's players see it
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RevealBehindLines(int side, vector position)
	{
		int localSide = COA_AAS_Territory.OwnerOf(m_Campaign, SCR_FactionManager.SGetLocalPlayerFaction());
		if (localSide == COA_AAS_ETerritoryOwner.NONE || localSide == side)
			return;

		if (m_FrontlineMap)
			m_FrontlineMap.AddPing(position, side, m_fBehindLinesPingDuration);

		// Tell them to look - not for every ping
		float now = GetGame().GetWorld().GetWorldTime();
		if (now < m_fNextSpottedHint)
			return;

		m_fNextSpottedHint = now + SPOTTED_HINT_COOLDOWN_MS;
		SCR_PopUpNotification popup = SCR_PopUpNotification.GetInstance();
		if (popup)
			popup.PopupMsg("ENEMY BEHIND OUR LINES", BEHIND_LINES_HINT_DURATION, "Their rough position is on the map");
	}

	//------------------------------------------------------------------------------------------------
	//! Server (COA_AAS_BehindLines): the player went deep into enemy territory, or came back
	void NotifyBehindLines(int playerId, bool behind)
	{
		Rpc(RpcDo_NotifyBehindLines, playerId, behind);
		if (RplSession.Mode() != RplMode.Dedicated)
			RpcDo_NotifyBehindLines(playerId, behind);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_NotifyBehindLines(int playerId, bool behind)
	{
		PlayerController controller = GetGame().GetPlayerController();
		if (!controller || controller.GetPlayerId() != playerId)
			return;

		SCR_PopUpNotification popup = SCR_PopUpNotification.GetInstance();
		if (!popup)
			return;

		if (behind)
			popup.PopupMsg("BEHIND ENEMY LINES", BEHIND_LINES_HINT_DURATION, "The enemy is being told roughly where you are - the deeper you go, the more often");
		else
			popup.PopupMsg("BACK NEAR FRIENDLY LINES", BEHIND_LINES_HINT_DURATION, "Your position is no longer being reported");
	}

	//------------------------------------------------------------------------------------------------
	//! Server (COA_AAS_Frontline): new territory for everyone
	void SetTerritory(string territory)
	{
		m_sTerritory = territory;
		Replication.BumpMe();

		// Replication callbacks don't run where the change was made - a player hosting needs it too
		OnTerritoryReplicated();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnTerritoryReplicated()
	{
		m_bHasTerritory = !m_sTerritory.IsEmpty() && m_Territory.Decode(m_sTerritory);
		m_iTerritoryVersion++;

		if (m_FrontlineMap)
			m_FrontlineMap.SetTerritory(m_sTerritory);

		// Server: a capture under way speeds up (or slows) as the ground around it changes hands
		if (m_Campaign && !m_Campaign.IsProxy())
			RetimeLiveCapture();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: a capture under way picks up a new capture time (ground or population changed)
	protected void RetimeLiveCapture()
	{
		SCR_CampaignMilitaryBaseComponent live = GetLiveObjective();
		if (live && live.GetCapturingFaction())
			live.RefreshSeizingTimers();
	}

	//------------------------------------------------------------------------------------------------
	//! Server: the soldier's radios carry their own side's encryption key. Loadouts from other mods don't
	//! always - Chernarussian Factions gives the CDF the US AN/PRC-68 - and with the same key both sides
	//! hear each other whenever their squads share a frequency.
	override void OnPlayerSpawnFinalize_S(SCR_SpawnRequestComponent requestComponent, SCR_SpawnHandlerComponent handlerComponent, SCR_SpawnData data, IEntity entity)
	{
		super.OnPlayerSpawnFinalize_S(requestComponent, handlerComponent, data, entity);

		// The gadget manager registers the radios once the inventory is up
		if (entity)
			GetGame().GetCallqueue().CallLater(KeyRadios, RADIO_KEY_DELAY_MS, false, entity);
	}

	//------------------------------------------------------------------------------------------------
	protected void KeyRadios(IEntity entity)
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(entity);
		if (!character || !character.GetFaction())
			return;

		string key = character.GetFaction().GetFactionRadioEncryptionKey();
		SCR_GadgetManagerComponent gadgets = SCR_GadgetManagerComponent.GetGadgetManager(entity);
		if (key.IsEmpty() || !gadgets)
			return;

		array<SCR_GadgetComponent> radios = {};
		radios.Copy(gadgets.GetGadgetsByType(EGadgetType.RADIO));
		radios.InsertAll(gadgets.GetGadgetsByType(EGadgetType.RADIO_BACKPACK));
		foreach (SCR_GadgetComponent gadget : radios)
		{
			if (!gadget)
				continue;

			BaseRadioComponent radio = BaseRadioComponent.Cast(gadget.GetOwner().FindComponent(BaseRadioComponent));
			if (radio && radio.GetEncryptionKey() != key)
				radio.SetEncryptionKey(key);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerConnected(int playerId)
	{
		super.OnPlayerConnected(playerId);
		OnPopulationChanged();
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		super.OnPlayerDisconnected(playerId, cause, timeout);
		OnPopulationChanged();
	}

	//------------------------------------------------------------------------------------------------
	//! A moment later, once the player manager counts the change
	protected void OnPopulationChanged()
	{
		if (!m_bPopulationCaptureTime || !m_bActive || !m_Campaign || m_Campaign.IsProxy())
			return;

		GetGame().GetCallqueue().Remove(RetimeLiveCapture);
		GetGame().GetCallqueue().CallLater(RetimeLiveCapture, POPULATION_RETIME_DELAY_MS);
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

		// A moment later, once Conflict has completed or cancelled the tasks on the base that just fell
		GetGame().GetCallqueue().Remove(SyncTasks);
		GetGame().GetCallqueue().CallLater(SyncTasks, TASK_SYNC_DELAY_MS);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: each side gets one task, on the live objective - seize it, or defend it if they own it.
	//! Any other seize or hold task is cancelled (Conflict hands out seize tasks for every base in HQ radio
	//! range, and drops a hold task by itself after a while without fighting - this puts it back).
	protected void SyncTasks()
	{
		SCR_TaskSystem taskSystem = SCR_TaskSystem.GetInstance();
		if (!taskSystem || !m_Campaign || m_Campaign.IsProxy())
			return;

		SCR_CampaignMilitaryBaseComponent live;
		if (m_bActive && m_aChain.IsIndexValid(m_iLiveIndex))
			live = m_aChain[m_iLiveIndex];

		array<int> sides = {SCR_ECampaignFaction.BLUFOR, SCR_ECampaignFaction.OPFOR};
		foreach (int side : sides)
		{
			SCR_CampaignFaction faction = m_Campaign.GetFactionByEnum(side);
			if (!faction)
				continue;

			SCR_CampaignMilitaryBaseComponent seizeTarget, holdTarget;
			if (live && live.GetCampaignFaction() == faction)
				holdTarget = live;
			else
				seizeTarget = live;

			if (!KeepOnlyTask(taskSystem, faction, SCR_SeizeCampaignMilitaryBaseTaskEntity, seizeTarget) && seizeTarget)
			{
				SCR_CampaignFactionCommanderHandlerComponent handler = SCR_CampaignFactionCommanderHandlerComponent.Cast(SCR_FactionCommanderHandlerComponent.GetInstance());
				if (handler)
					handler.CreateSeizeTask(faction, seizeTarget, -1);
			}

			if (!KeepOnlyTask(taskSystem, faction, SCR_HoldCampaignMilitaryBaseTaskEntity, holdTarget) && holdTarget)
				CreateHoldTask(taskSystem, faction, holdTarget);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Cancels the faction's open tasks of the class except one on the target
	//! \return whether the faction already has one on the target
	protected bool KeepOnlyTask(notnull SCR_TaskSystem taskSystem, notnull SCR_CampaignFaction faction, typename taskClass, SCR_CampaignMilitaryBaseComponent target)
	{
		array<SCR_Task> tasks = {};
		taskSystem.GetTasksByStateFiltered(tasks, SCR_ETaskState.CREATED | SCR_ETaskState.ASSIGNED, faction.GetFactionKey(), -1, taskClass);

		bool kept;
		foreach (SCR_Task task : tasks)
		{
			SCR_CampaignMilitaryBaseTaskEntity baseTask = SCR_CampaignMilitaryBaseTaskEntity.Cast(task);
			if (!baseTask)
				continue;

			if (target && !kept && baseTask.GetMilitaryBase() == target)
			{
				kept = true;
				continue;
			}

			taskSystem.SetTaskState(task, SCR_ETaskState.CANCELLED);
			taskSystem.DeleteTask(task);
		}

		return kept;
	}

	//------------------------------------------------------------------------------------------------
	//! The same defend task Conflict's commander can give, for every group of the faction
	protected void CreateHoldTask(notnull SCR_TaskSystem taskSystem, notnull SCR_CampaignFaction faction, notnull SCR_CampaignMilitaryBaseComponent base)
	{
		SCR_GroupTaskManagerComponent groupTaskManager = SCR_GroupTaskManagerComponent.GetInstance();
		if (!groupTaskManager || !groupTaskManager.CanCreateNewTaskWithResourceName(HOLD_TASK_PREFAB, faction))
			return;

		m_iTaskSerial++;
		string taskId = string.Format("%1_COA_AAS_Hold_%2_%3", faction.GetFactionKey(), base.GetCallsign(), m_iTaskSerial);
		SCR_CampaignMilitaryBaseTaskEntity task = SCR_CampaignMilitaryBaseTaskEntity.Cast(taskSystem.CreateTask(HOLD_TASK_PREFAB, taskId, "", "", base.GetOwner().GetOrigin()));
		if (!task)
			return;

		SCR_TaskUIInfo info = task.GetTaskUIInfo();
		if (info)
			task.SetTaskName(info.GetName(), {base.GetFormattedBaseNameWithCallsign(faction)});

		task.SetMilitaryBaseCallSign(base.GetCallsign());
		taskSystem.SetTaskOwnership(task, SCR_ETaskOwnership.EXECUTOR);
		taskSystem.SetTaskVisibility(task, SCR_ETaskVisibility.GROUP);
		taskSystem.AddTaskFaction(task, faction.GetFactionKey());
		groupTaskManager.SetGroupTask(task, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Is the AAS line set up for this world - it then owns the seize tasks (COA_AAS_SCR_CampaignMilitaryBaseComponent)
	bool HasObjectiveChain()
	{
		return m_aObjectiveChain && !m_aObjectiveChain.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! How many objectives make up the line - what a side must hold to win
	int GetObjectiveCount()
	{
		if (!m_aObjectiveChain)
			return 0;

		return m_aObjectiveChain.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Objectives in the line the faction holds - the score in Conflict's top bar
	//! (COA_AAS_SCR_CampaignMilitaryBaseManager)
	int CountObjectivesHeld(Faction faction)
	{
		int held;
		foreach (SCR_CampaignMilitaryBaseComponent base : m_aChain)
		{
			if (base && faction && base.GetFaction() == faction)
				held++;
		}

		return held;
	}

	//------------------------------------------------------------------------------------------------
	//! Win: one side owns every capturable base (the whole line - the HQs can't be taken). Checked on any
	//! change in the line, however it happened (seizing, Game Master), not just the live objective.
	protected void OnBaseFactionChanged(SCR_CampaignMilitaryBaseComponent base, Faction previousFaction, Faction faction)
	{
		int index = m_aChain.Find(base);
		if (!m_bActive || index == -1)
			return;

		int captor = GetFactionEnum(SCR_CampaignFaction.Cast(faction));
		string captured = GetBaseName(index);
		if ((captor == SCR_ECampaignFaction.BLUFOR || captor == SCR_ECampaignFaction.OPFOR) && OwnsWholeLine(captor))
		{
			Victory(captor, captured);
			return;
		}

		if (index != m_iLiveIndex || (captor != SCR_ECampaignFaction.BLUFOR && captor != SCR_ECampaignFaction.OPFOR))
			return;

		// Onwards to the next base toward the enemy the captor doesn't own (there is one - else they'd own the line)
		int next = index + GetDirection(captor);
		while (m_aChain.IsIndexValid(next) && GetFactionEnum(m_aChain[next].GetCampaignFaction()) == captor)
		{
			next += GetDirection(captor);
		}

		// The rest of the line toward the enemy is theirs, but not all of it behind - start from the near end
		if (!m_aChain.IsIndexValid(next))
			next = GetNextTarget(captor);

		if (!m_aChain.IsIndexValid(next))
			return;

		SetLive(next, captor);
		Notify(string.Format("%1 secured %2", GetFactionName(captor), captured), string.Format("Next objective: %1", GetBaseName(next)));
	}

	//------------------------------------------------------------------------------------------------
	protected bool OwnsWholeLine(int side)
	{
		foreach (SCR_CampaignMilitaryBaseComponent base : m_aChain)
		{
			if (GetFactionEnum(base.GetCampaignFaction()) != side)
				return false;
		}

		return !m_aChain.IsEmpty();
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
		GetGame().GetCallqueue().Remove(SyncTasks);
		SCR_CampaignMilitaryBaseComponent.GetOnFactionChangedExtended().Remove(OnBaseFactionChanged);
		SyncTasks();

		// The map keeps the territory as it ended
		m_Frontline = null;
		m_BehindLines = null;
		if (m_Insurgents)
		{
			m_Insurgents.Stop();
			m_Insurgents = null;
		}

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
			GetGame().GetCallqueue().Remove(SyncTasks);
			GetGame().GetCallqueue().Remove(TopUpHQs);
			GetGame().GetCallqueue().Remove(RetimeLiveCapture);
			GetGame().GetCallqueue().Remove(ShowSupplyPiles);
			GetGame().GetCallqueue().Remove(Setup);
			GetGame().GetCallqueue().Remove(RefreshMapIcons);
		}

		if (m_bActive)
			SCR_CampaignMilitaryBaseComponent.GetOnFactionChangedExtended().Remove(OnBaseFactionChanged);

		SCR_CampaignNetworkComponent.GetOnSuppliesDelivered().Remove(OnSuppliesDelivered);
		if (m_Campaign && m_Campaign.GetBaseManager())
			m_Campaign.GetBaseManager().GetOnBaseBuilt().Remove(OnBaseBuilt);
	}
}
