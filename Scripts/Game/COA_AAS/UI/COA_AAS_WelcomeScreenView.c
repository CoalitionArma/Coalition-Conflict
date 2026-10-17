//------------------------------------------------------------------------------------------------
//! Fills and refreshes the Advance & Secure welcome screen (COA_AAS_WelcomeScreen.layout):
//!  - header: mission name/description, elapsed time, players online
//!  - forces: a card per side with players, objectives held and what it is doing right now
//!  - front line: the objective chain north (OPFOR HQ) to south (BLUFOR HQ) coloured by owner, the
//!    live objective highlighted, plus the defence clock
//!  - how it works: the rules, using this mission's hold time
//! Everything it shows is replicated, so it works on any client. Stops itself when the screen closes.
class COA_AAS_WelcomeScreenView : Managed
{
	protected Widget m_wRoot;
	protected ref COA_AAS_WelcomeScreenHandler m_Handler;

	protected RichTextWidget m_wTitle;
	protected RichTextWidget m_wSubtitle;
	protected RichTextWidget m_wTimeText;
	protected RichTextWidget m_wPlayersText;
	protected RichTextWidget m_wStatusKicker;
	protected RichTextWidget m_wStatusTitle;
	protected RichTextWidget m_wStatusDetail;
	protected Widget m_wClock;
	protected RichTextWidget m_wClockText;
	protected RichTextWidget m_wClockLabel;

	protected ref array<Widget> m_aCards = {};
	protected ref array<int> m_aCardFactions = {};		// SCR_ECampaignFaction per card
	protected ref array<Widget> m_aNodes = {};
	protected ref array<int> m_aNodeChainIndex = {};	// chain index per row, -1 for HQ rows
	protected ref array<int> m_aNodeHQFaction = {};		// SCR_ECampaignFaction of HQ rows, -1 otherwise

	protected Widget m_wLiveNode;
	protected bool m_bRunning;

	protected static const int REFRESH_MS = 500;
	protected static const int PULSE_MS = 50;

	// COALITION palette (linear)
	protected static const ref Color COLOR_TEXT = new Color(0.8632, 0.8879, 0.9301, 1);
	protected static const ref Color COLOR_MUTED = new Color(0.3968, 0.4564, 0.6038, 1);
	protected static const ref Color COLOR_ACCENT = new Color(0.5841, 0.0369, 0.0369, 1);
	protected static const ref Color COLOR_CARD = new Color(0.011, 0.0137, 0.0212, 1);
	protected static const ref Color COLOR_NEUTRAL = new Color(0.25, 0.27, 0.31, 1);

	//------------------------------------------------------------------------------------------------
	void COA_AAS_WelcomeScreenView(notnull Widget root, ResourceName factionLayout, ResourceName nodeLayout, ResourceName ruleLayout)
	{
		m_wRoot = root;
		m_Handler = new COA_AAS_WelcomeScreenHandler();
		m_Handler.m_View = this;
		root.AddHandler(m_Handler);

		m_wTitle = RichTextWidget.Cast(root.FindAnyWidget("Title"));
		m_wSubtitle = RichTextWidget.Cast(root.FindAnyWidget("Subtitle"));
		m_wTimeText = RichTextWidget.Cast(root.FindAnyWidget("TimeText"));
		m_wPlayersText = RichTextWidget.Cast(root.FindAnyWidget("PlayersText"));
		m_wStatusKicker = RichTextWidget.Cast(root.FindAnyWidget("StatusKicker"));
		m_wStatusTitle = RichTextWidget.Cast(root.FindAnyWidget("StatusTitle"));
		m_wStatusDetail = RichTextWidget.Cast(root.FindAnyWidget("StatusDetail"));
		m_wClock = root.FindAnyWidget("Clock");
		m_wClockText = RichTextWidget.Cast(root.FindAnyWidget("ClockText"));
		m_wClockLabel = RichTextWidget.Cast(root.FindAnyWidget("ClockLabel"));

		FillHeader();
		BuildCards(root.FindAnyWidget("FactionList"), factionLayout);
		BuildNodes(root.FindAnyWidget("NodeList"), nodeLayout);
		BuildRules(root.FindAnyWidget("RuleList"), ruleLayout);

		Refresh();
		PlayEntrance();

		m_bRunning = true;
		GetGame().GetCallqueue().CallLater(Refresh, REFRESH_MS, true);
		GetGame().GetCallqueue().CallLater(Pulse, PULSE_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	void Stop()
	{
		if (!m_bRunning)
			return;

		m_bRunning = false;
		GetGame().GetCallqueue().Remove(Refresh);
		GetGame().GetCallqueue().Remove(Pulse);
	}

	//------------------------------------------------------------------------------------------------
	void ~COA_AAS_WelcomeScreenView()
	{
		if (GetGame() && GetGame().GetCallqueue())
			Stop();
	}

	//------------------------------------------------------------------------------------------------
	protected void FillHeader()
	{
		SCR_MissionHeader header = SCR_MissionHeader.Cast(GetGame().GetMissionHeader());
		if (!header)
			return;

		if (m_wTitle && !header.m_sName.IsEmpty())
			m_wTitle.SetText(header.m_sName);

		if (m_wSubtitle)
			m_wSubtitle.SetText(header.m_sDescription);
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildCards(Widget list, ResourceName layout)
	{
		if (!list)
			return;

		array<int> order = {SCR_ECampaignFaction.BLUFOR, SCR_ECampaignFaction.OPFOR, SCR_ECampaignFaction.INDFOR};
		foreach (int factionEnum : order)
		{
			SCR_CampaignFaction faction = GetFaction(factionEnum);
			if (!faction)
				continue;

			Widget card = GetGame().GetWorkspace().CreateWidgets(layout, list);
			if (!card)
				continue;

			SetText(card, "FactionName", faction.GetFactionName());
			SetImageColor(card, "Strip", faction.GetFactionColor());

			ImageWidget flag = ImageWidget.Cast(card.FindAnyWidget("Flag"));
			if (flag && !faction.GetFactionFlag().IsEmpty())
				flag.LoadImageTexture(0, faction.GetFactionFlag());

			m_aCards.Insert(card);
			m_aCardFactions.Insert(factionEnum);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! North at the top: OPFOR HQ, the chain from its OPFOR end down, BLUFOR HQ
	protected void BuildNodes(Widget list, ResourceName layout)
	{
		COA_AAS_ConflictComponent aas = COA_AAS_ConflictComponent.GetInstance();
		if (!list || !aas)
			return;

		AddNode(list, layout, -1, SCR_ECampaignFaction.OPFOR);
		for (int i = aas.GetChainCount() - 1; i >= 0; i--)
		{
			AddNode(list, layout, i, -1);
		}
		AddNode(list, layout, -1, SCR_ECampaignFaction.BLUFOR);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddNode(Widget list, ResourceName layout, int chainIndex, int hqFaction)
	{
		Widget node = GetGame().GetWorkspace().CreateWidgets(layout, list);
		if (!node)
			return;

		int number = chainIndex + 1;
		if (chainIndex >= 0)
			SetText(node, "NodeIndex", number.ToString(2));
		else
			SetText(node, "NodeIndex", "HQ");

		m_aNodes.Insert(node);
		m_aNodeChainIndex.Insert(chainIndex);
		m_aNodeHQFaction.Insert(hqFaction);
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildRules(Widget list, ResourceName layout)
	{
		if (!list)
			return;

		COA_AAS_ConflictComponent aas = COA_AAS_ConflictComponent.GetInstance();
		string hold = "15:00";
		if (aas)
			hold = FormatClock(aas.GetDefenseHoldTime());

		AddRule(list, layout, "01", "One objective at a time", "Only the LIVE objective can be seized. Everywhere else, attackers don't count - not even to contest it.");
		AddRule(list, layout, "02", "Take it to advance", "Whoever captures the live objective keeps the initiative: the next objective toward the enemy goes live.");
		AddRule(list, layout, "03", "Hold it to strike back", string.Format("Defenders who hold the live objective for %1 turn the attack around and push on the attackers' front line. The clock pauses while it is being seized.", hold));
		AddRule(list, layout, "04", "Break through to win", "Capture the last objective before the enemy HQ and the battle is yours.");
	}

	//------------------------------------------------------------------------------------------------
	protected void AddRule(Widget list, ResourceName layout, string number, string title, string body)
	{
		Widget rule = GetGame().GetWorkspace().CreateWidgets(layout, list);
		if (!rule)
			return;

		SetText(rule, "RuleNumber", number);
		SetText(rule, "RuleTitle", title);
		SetText(rule, "RuleBody", body);
	}

	//------------------------------------------------------------------------------------------------
	protected void Refresh()
	{
		if (!m_wRoot)
		{
			Stop();
			return;
		}

		RefreshHeader();

		COA_AAS_ConflictComponent aas = COA_AAS_ConflictComponent.GetInstance();
		if (!aas)
			return;

		SCR_CampaignMilitaryBaseComponent live = aas.GetLiveObjective();
		RefreshStatus(aas, live);
		RefreshCards(aas, live);
		RefreshNodes(aas, live);
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshHeader()
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (m_wTimeText && gameMode)
		{
			int elapsed = gameMode.GetElapsedTime();
			m_wTimeText.SetText(SCR_FormatHelper.FormatTime(elapsed));
		}

		if (!m_wPlayersText)
			return;

		int players = GetGame().GetPlayerManager().GetPlayerCount();
		int limit;
		ServerInfo serverInfo = GetGame().GetServerInfo();
		if (serverInfo)
			limit = serverInfo.GetPlayerLimit();

		if (limit > 0)
			m_wPlayersText.SetText(string.Format("%1 / %2", players, limit));
		else
			m_wPlayersText.SetText(players.ToString());
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshStatus(COA_AAS_ConflictComponent aas, SCR_CampaignMilitaryBaseComponent live)
	{
		if (!live)
		{
			SetWidgetText(m_wStatusKicker, "FRONT LINE");
			SetWidgetText(m_wStatusTitle, "Standing by");
			SetWidgetText(m_wStatusDetail, "The objectives go live once Conflict has set up its bases.");
			if (m_wClock)
				m_wClock.SetVisible(false);
			return;
		}

		SetWidgetText(m_wStatusKicker, "LIVE OBJECTIVE");
		SetWidgetText(m_wStatusTitle, live.GetBaseName());

		SCR_CampaignFaction owner = live.GetCampaignFaction();
		SCR_CampaignFaction attacker = GetFaction(aas.GetAttacker());
		string ownerName = "nobody";
		if (owner)
			ownerName = owner.GetFactionNameUpperCase();

		string detail;
		if (!attacker)
			detail = string.Format("Held by %1. The first side to take it starts the push.", ownerName);
		else if (aas.GetHoldRemaining() >= 0)
			detail = string.Format("%1 attacking, %2 defending.", attacker.GetFactionNameUpperCase(), ownerName);
		else
			detail = string.Format("%1 advancing. Held by %2.", attacker.GetFactionNameUpperCase(), ownerName);

		SetWidgetText(m_wStatusDetail, detail);

		int remaining = aas.GetHoldRemaining();
		if (m_wClock)
			m_wClock.SetVisible(remaining >= 0);

		if (remaining < 0)
			return;

		SetWidgetText(m_wClockText, FormatClock(remaining));
		if (aas.IsHoldPaused())
			SetWidgetText(m_wClockLabel, "PAUSED - CONTESTED");
		else
			SetWidgetText(m_wClockLabel, "UNTIL COUNTERATTACK");
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshCards(COA_AAS_ConflictComponent aas, SCR_CampaignMilitaryBaseComponent live)
	{
		int total = aas.GetChainCount();
		string liveName;
		SCR_CampaignFaction liveOwner;
		if (live)
		{
			liveName = live.GetBaseName();
			liveName.ToUpper();
			liveOwner = live.GetCampaignFaction();
		}

		SCR_CampaignFaction attacker = GetFaction(aas.GetAttacker());

		foreach (int i, Widget card : m_aCards)
		{
			SCR_CampaignFaction faction = GetFaction(m_aCardFactions[i]);
			if (!faction)
				continue;

			int held;
			for (int b = 0; b < total; b++)
			{
				SCR_CampaignMilitaryBaseComponent base = aas.GetChainBase(b);
				if (base && base.GetCampaignFaction() == faction)
					held++;
			}

			string role;
			Color roleColor = COLOR_MUTED;
			if (m_aCardFactions[i] == SCR_ECampaignFaction.INDFOR)
			{
				if (held > 0)
					role = "AI  /  HOLDING GROUND";
				else
					role = "AI  /  DRIVEN OUT";

				SetText(card, "FactionStats", string.Format("%1 of %2 objectives", held, total));
			}
			else
			{
				if (!live)
					role = "STANDING BY";
				else if (!attacker)
					role = "RACE FOR " + liveName;
				else if (attacker == faction)
				{
					role = "ATTACKING " + liveName;
					roleColor = COLOR_ACCENT;
				}
				else if (liveOwner == faction)
					role = "DEFENDING " + liveName;
				else
					role = "HOLDING THE LINE";

				SetText(card, "FactionStats", string.Format("%1 players  /  %2 of %3 objectives", faction.GetPlayerCount(), held, total));
			}

			SetText(card, "FactionRole", role);
			RichTextWidget roleText = RichTextWidget.Cast(card.FindAnyWidget("FactionRole"));
			if (roleText)
				roleText.SetColor(roleColor);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshNodes(COA_AAS_ConflictComponent aas, SCR_CampaignMilitaryBaseComponent live)
	{
		m_wLiveNode = null;

		foreach (int i, Widget node : m_aNodes)
		{
			SCR_CampaignMilitaryBaseComponent base;
			string name;
			if (m_aNodeChainIndex[i] >= 0)
			{
				base = aas.GetChainBase(m_aNodeChainIndex[i]);
				if (base)
					name = base.GetBaseName();
			}
			else
			{
				SCR_CampaignFaction hqFaction = GetFaction(m_aNodeHQFaction[i]);
				if (hqFaction)
				{
					base = hqFaction.GetMainBase();
					name = hqFaction.GetFactionNameUpperCase() + " HQ";
					if (base)
						name = base.GetBaseName();
				}
			}

			SCR_CampaignFaction owner;
			if (base)
				owner = base.GetCampaignFaction();
			else
				owner = GetFaction(m_aNodeHQFaction[i]);

			bool isLive = base && base == live;
			SetText(node, "NodeName", name);

			if (owner)
			{
				SetText(node, "NodeOwner", owner.GetFactionNameUpperCase());
				SetImageColor(node, "Track", owner.GetFactionColor());
			}
			else
			{
				SetText(node, "NodeOwner", "");
				SetImageColor(node, "Track", COLOR_NEUTRAL);
			}

			Widget badge = node.FindAnyWidget("LiveBadge");
			if (badge)
				badge.SetVisible(isLive);

			// HQ rows read as end caps, the live row stands out
			RichTextWidget nameText = RichTextWidget.Cast(node.FindAnyWidget("NodeName"));
			if (nameText)
			{
				if (m_aNodeChainIndex[i] < 0)
					nameText.SetColor(COLOR_MUTED);
				else
					nameText.SetColor(COLOR_TEXT);
			}

			if (isLive)
				m_wLiveNode = node;
			else
				SetImageColor(node, "NodeBg", COLOR_CARD);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The live row breathes so it reads at a glance
	protected void Pulse()
	{
		if (!m_wRoot)
		{
			Stop();
			return;
		}

		if (!m_wLiveNode)
			return;

		float wave = 0.5 + 0.5 * Math.Sin(System.GetTickCount() * 0.004);

		ImageWidget background = ImageWidget.Cast(m_wLiveNode.FindAnyWidget("NodeBg"));
		if (background)
			background.SetColor(new Color(COLOR_ACCENT.R(), COLOR_ACCENT.G(), COLOR_ACCENT.B(), 0.18 + 0.22 * wave));

		Widget badge = m_wLiveNode.FindAnyWidget("LiveBadge");
		if (badge)
			badge.SetOpacity(0.65 + 0.35 * wave);
	}

	//------------------------------------------------------------------------------------------------
	//! Header, divider and the three columns fade in one after another
	protected void PlayEntrance()
	{
		array<string> names = {"Header", "Divider", "ForcesCol", "FrontCol", "RulesCol"};
		foreach (int i, string name : names)
		{
			Widget w = m_wRoot.FindAnyWidget(name);
			if (!w)
				continue;

			w.SetOpacity(0);
			WidgetAnimationOpacity fade = AnimateWidget.Opacity(w, 1, 3.3);
			if (!fade)
			{
				w.SetOpacity(1);
				continue;
			}

			fade.SetCurve(EAnimationCurve.EASE_OUT_CUBIC);
			fade.SetDelay(i * 0.07);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_CampaignFaction GetFaction(int factionEnum)
	{
		if (factionEnum < 0)
			return null;

		SCR_GameModeCampaign campaign = SCR_GameModeCampaign.GetInstance();
		if (!campaign)
			return null;

		return campaign.GetFactionByEnum(factionEnum);
	}

	//------------------------------------------------------------------------------------------------
	protected static string FormatClock(float seconds)
	{
		int total = Math.Max(0, Math.Ceil(seconds));
		int minutes = total / 60;
		int secs = total % 60;
		return string.Format("%1:%2", minutes.ToString(2), secs.ToString(2));
	}

	//------------------------------------------------------------------------------------------------
	protected static void SetText(Widget parent, string name, string value)
	{
		SetWidgetText(RichTextWidget.Cast(parent.FindAnyWidget(name)), value);
	}

	//------------------------------------------------------------------------------------------------
	protected static void SetWidgetText(RichTextWidget widget, string value)
	{
		if (widget)
			widget.SetText(value);
	}

	//------------------------------------------------------------------------------------------------
	protected static void SetImageColor(Widget parent, string name, Color color)
	{
		Widget widget = parent.FindAnyWidget(name);
		if (widget && color)
			widget.SetColor(color);
	}
}

//------------------------------------------------------------------------------------------------
//! Stops the view's refresh loops when the welcome screen's widgets are destroyed
class COA_AAS_WelcomeScreenHandler : ScriptedWidgetComponent
{
	COA_AAS_WelcomeScreenView m_View;

	//------------------------------------------------------------------------------------------------
	override void HandlerDeattached(Widget w)
	{
		if (m_View)
			m_View.Stop();

		super.HandlerDeattached(w);
	}
}
