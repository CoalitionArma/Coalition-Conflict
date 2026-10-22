//------------------------------------------------------------------------------------------------
//! Server: keeps every player's XP (and so their rank) across reconnects, match ends and server
//! restarts. XP is written to a small file in the server's profile folder, one "identity;xp" line per
//! player, keyed by their Bohemia identity. A joining player gets their saved XP back a few seconds
//! after Conflict has set them up, and a hint saying so once they deploy. Delete the file to wipe
//! everyone's rank; edit a line to change one.
[ComponentEditorProps(category: "COALITION/Conflict", description: "Saves players' XP between server restarts")]
class COA_AAS_RankStoreComponentClass : SCR_BaseGameModeComponentClass
{
}

class COA_AAS_RankStoreComponent : SCR_BaseGameModeComponent
{
	[Attribute("1", desc: "Save players' XP and give it back when they join", category: "Rank Store")]
	protected bool m_bEnabled;

	[Attribute("$profile:COA_AAS_Ranks.txt", desc: "Where the XP is kept ($profile: is the server's profile folder)", category: "Rank Store")]
	protected string m_sFile;

	[Attribute("3", desc: "Seconds after a player joins before their XP is given back - after Conflict's own starting rank", params: "0.5 60 0.5", category: "Rank Store")]
	protected float m_fRestoreDelay;

	[Attribute("15", desc: "Seconds XP changes are gathered before the file is written", params: "1 600 1", category: "Rank Store")]
	protected float m_fSaveDelay;

	protected ref map<string, int> m_mXP = new map<string, int>();				// identity -> XP
	protected ref map<int, string> m_mIdentities = new map<int, string>();		// playerId -> identity
	protected ref array<int> m_aRestored = {};		// players whose XP is back - only their changes are kept
	protected bool m_bLoaded;
	protected bool m_bDirty;

	// Where the player plays: the "rank restored" hint waiting for them to deploy
	protected static const int HINT_POLL_MS = 1000;
	protected static const int HINT_POLL_ATTEMPTS = 600;		// gives up waiting after 10 min and shows it anyway
	protected static const float HINT_DURATION = 10;
	protected int m_iHintRank;
	protected int m_iHintXP;
	protected int m_iHintAttempts;

	//------------------------------------------------------------------------------------------------
	override void OnGameModeStart()
	{
		super.OnGameModeStart();

		if (m_bEnabled && Replication.IsServer())
			Load();
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerAuditSuccess(int playerId)
	{
		super.OnPlayerAuditSuccess(playerId);

		if (!m_bEnabled || !Replication.IsServer())
			return;

		string identity = SCR_PlayerIdentityUtils.GetPlayerIdentityId(playerId);
		if (identity.IsEmpty() || identity == UUID.NULL_UUID)
			return;

		m_mIdentities.Set(playerId, identity);
		GetGame().GetCallqueue().CallLater(Restore, m_fRestoreDelay * 1000, false, playerId);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		if (m_bEnabled && Replication.IsServer() && m_aRestored.Contains(playerId))
		{
			SCR_PlayerXPHandlerComponent xp = GetXPHandler(playerId);
			if (xp)
			{
				Record(playerId, xp.GetPlayerXP());
				xp.GetOnPlayerXPChanged().Remove(OnXPChanged);
			}

			m_aRestored.RemoveItem(playerId);
		}

		m_mIdentities.Remove(playerId);
		super.OnPlayerDisconnected(playerId, cause, timeout);
	}

	//------------------------------------------------------------------------------------------------
	override void OnGameModeEnd(SCR_GameModeEndData data)
	{
		super.OnGameModeEnd(data);

		if (m_bEnabled && Replication.IsServer())
			Save();
	}

	//------------------------------------------------------------------------------------------------
	//! The player's saved XP back (none saved: they start a record with what they have)
	protected void Restore(int playerId)
	{
		string identity = m_mIdentities.Get(playerId);
		SCR_PlayerXPHandlerComponent xp = GetXPHandler(playerId);
		if (identity.IsEmpty() || !xp || m_aRestored.Contains(playerId))
			return;

		int saved;
		if (m_mXP.Find(identity, saved))
		{
			int difference = saved - xp.GetPlayerXP();
			if (difference != 0)
				xp.AddPlayerXP(SCR_EXPRewards.UNDEFINED, addDirectly: difference);

			NotifyRestored(playerId, xp.GetPlayerRankByXP(), xp.GetPlayerXP());
		}
		else
		{
			Record(playerId, xp.GetPlayerXP());
		}

		m_aRestored.Insert(playerId);
		xp.GetOnPlayerXPChanged().Insert(OnXPChanged);
	}

	//------------------------------------------------------------------------------------------------
	//! Server: tell the player their rank came back
	protected void NotifyRestored(int playerId, SCR_ECharacterRank rank, int xp)
	{
		Rpc(RpcDo_NotifyRestored, playerId, rank, xp);

		// Broadcast RPCs skip the server - a player hosting needs it too
		if (RplSession.Mode() != RplMode.Dedicated)
			RpcDo_NotifyRestored(playerId, rank, xp);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_NotifyRestored(int playerId, int rank, int xp)
	{
		PlayerController controller = GetGame().GetPlayerController();
		if (!controller || controller.GetPlayerId() != playerId)
			return;

		m_iHintRank = rank;
		m_iHintXP = xp;
		m_iHintAttempts = 0;
		GetGame().GetCallqueue().Remove(ShowRestoredHint);
		GetGame().GetCallqueue().CallLater(ShowRestoredHint, HINT_POLL_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Where the player plays: the hint, once they're in the world (the deploy menu would hide it)
	protected void ShowRestoredHint()
	{
		m_iHintAttempts++;
		PlayerController controller = GetGame().GetPlayerController();
		if ((!controller || !controller.GetControlledEntity()) && m_iHintAttempts < HINT_POLL_ATTEMPTS)
			return;

		GetGame().GetCallqueue().Remove(ShowRestoredHint);

		string rankName = typename.EnumToString(SCR_ECharacterRank, m_iHintRank);
		SCR_Faction faction = SCR_Faction.Cast(SCR_FactionManager.SGetLocalPlayerFaction());
		if (faction && faction.GetRanks())
		{
			string factionRank = faction.GetRanks().GetRankName(m_iHintRank);
			if (!factionRank.IsEmpty())
				rankName = WidgetManager.Translate(factionRank);
		}

		SCR_HintManagerComponent.ShowCustomHint(
			string.Format("Welcome back, %1. Your rank and %2 XP were restored from your last session.", rankName, m_iHintXP),
			"Rank restored", HINT_DURATION);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnXPChanged(int playerId, int currentXP, int XPToAdd, SCR_EXPRewards rewardId)
	{
		if (m_aRestored.Contains(playerId))
			Record(playerId, currentXP);
	}

	//------------------------------------------------------------------------------------------------
	protected void Record(int playerId, int xp)
	{
		string identity = m_mIdentities.Get(playerId);
		if (identity.IsEmpty())
			return;

		int current;
		if (m_mXP.Find(identity, current) && current == xp)
			return;

		m_mXP.Set(identity, xp);
		if (m_bDirty)
			return;

		m_bDirty = true;
		GetGame().GetCallqueue().CallLater(Save, m_fSaveDelay * 1000);
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_PlayerXPHandlerComponent GetXPHandler(int playerId)
	{
		PlayerController controller = GetGame().GetPlayerManager().GetPlayerController(playerId);
		if (!controller)
			return null;

		return SCR_PlayerXPHandlerComponent.Cast(controller.FindComponent(SCR_PlayerXPHandlerComponent));
	}

	//------------------------------------------------------------------------------------------------
	protected void Load()
	{
		m_bLoaded = true;
		if (!FileIO.FileExists(m_sFile))
		{
			Print("[COA_AAS] Rank store: no " + m_sFile + " yet - starting a new one", LogLevel.NORMAL);
			return;
		}

		FileHandle file = FileIO.OpenFile(m_sFile, FileMode.READ);
		if (!file)
		{
			Print("[COA_AAS] Rank store: can't read " + m_sFile, LogLevel.ERROR);
			return;
		}

		string line;
		array<string> parts = {};
		while (file.ReadLine(line) > -1)
		{
			parts.Clear();
			line.Split(";", parts, true);
			if (parts.Count() < 2)
				continue;

			string identity = parts[0].Trim();
			if (!identity.IsEmpty())
				m_mXP.Set(identity, parts[1].Trim().ToInt());
		}

		file.Close();
		Print(string.Format("[COA_AAS] Rank store: %1 players loaded from %2", m_mXP.Count(), m_sFile), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Written to a temporary file first, so a crash mid-write never loses the old one
	protected void Save()
	{
		m_bDirty = false;
		GetGame().GetCallqueue().Remove(Save);
		if (!m_bLoaded)
			return;

		string temporary = m_sFile + ".tmp";
		FileHandle file = FileIO.OpenFile(temporary, FileMode.WRITE);
		if (!file)
		{
			Print("[COA_AAS] Rank store: can't write " + temporary, LogLevel.ERROR);
			return;
		}

		foreach (string identity, int xp : m_mXP)
		{
			file.WriteLine(identity + ";" + xp.ToString());
		}

		file.Close();
		if (FileIO.CopyFile(temporary, m_sFile))
			FileIO.DeleteFile(temporary);
		else
			Print("[COA_AAS] Rank store: can't replace " + m_sFile, LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	void ~COA_AAS_RankStoreComponent()
	{
		if (m_bDirty && GetGame())
			Save();

		if (GetGame() && GetGame().GetCallqueue())
		{
			GetGame().GetCallqueue().Remove(Save);
			GetGame().GetCallqueue().Remove(Restore);
			GetGame().GetCallqueue().Remove(ShowRestoredHint);
		}
	}
}
