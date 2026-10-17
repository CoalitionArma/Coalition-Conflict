//------------------------------------------------------------------------------------------------
//! Welcome screen ("Deployment Setup") for Conflict: Advance & Secure, in the COALITION menu style.
//! Plugs into the vanilla welcome menu through SCR_WelcomeScreenComponent.m_BaseLayout: it hides the
//! vanilla header, restyles the card and fills WelcomeContent with COA_AAS_WelcomeScreen.layout, which
//! COA_AAS_WelcomeScreenView keeps live (forces, front line, defence clock).
[BaseContainerProps(), SCR_ContainerActionTitle()]
class COA_AAS_WelcomeScreenLayout : SCR_DeployMenuBaseScreenLayout
{
	[Attribute("{9C2E61B4A7D3F058}UI/layouts/COA_AAS/COA_AAS_WelcomeScreen.layout", UIWidgets.ResourceNamePicker, params: "layout")]
	protected ResourceName m_sScreenLayout;

	[Attribute("{3F8A0D52C6B1E947}UI/layouts/COA_AAS/COA_AAS_WelcomeFaction.layout", UIWidgets.ResourceNamePicker, params: "layout")]
	protected ResourceName m_sFactionLayout;

	[Attribute("{B61D47E9A2C35F80}UI/layouts/COA_AAS/COA_AAS_WelcomeNode.layout", UIWidgets.ResourceNamePicker, params: "layout")]
	protected ResourceName m_sNodeLayout;

	[Attribute("{E07C9A3B5D1F2864}UI/layouts/COA_AAS/COA_AAS_WelcomeRule.layout", UIWidgets.ResourceNamePicker, params: "layout")]
	protected ResourceName m_sRuleLayout;

	protected ref COA_AAS_WelcomeScreenView m_View;

	// Card background in the COALITION palette (linear)
	protected static const ref Color CARD_BACKGROUND = new Color(0.0033, 0.0037, 0.0052, 0.94);

	//------------------------------------------------------------------------------------------------
	override void InitContent(SCR_WelcomeScreenMenu menu)
	{
		Widget menuRoot = menu.GetRootWidget();
		Widget content = menuRoot.FindAnyWidget("WelcomeContent");
		if (!content)
			return;

		// Our layout carries its own header (mission, elapsed time, players)
		Widget header = menuRoot.FindAnyWidget("Header");
		if (header)
			header.SetVisible(false);

		ImageWidget background = ImageWidget.Cast(menuRoot.FindAnyWidget("Background"));
		if (background)
			background.SetColor(CARD_BACKGROUND);

		Widget screen = GetGame().GetWorkspace().CreateWidgets(m_sScreenLayout, content);
		if (!screen)
			return;

		if (m_View)
			m_View.Stop();

		m_View = new COA_AAS_WelcomeScreenView(screen, m_sFactionLayout, m_sNodeLayout, m_sRuleLayout);
	}
}
