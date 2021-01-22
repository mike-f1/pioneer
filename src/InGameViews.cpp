
#include "InGameViews.h"

#include "DeathView.h"
#include "Game.h"
#include "GameLocator.h"
#include "ObjectViewerView.h"
#include "SectorView.h"
#include "ShipCpanel.h"
#include "SystemInfoView.h"
#include "SystemView.h"
#include "UIView.h"
#include "WorldView.h"
#include "galaxy/Galaxy.h"
#include "galaxy/SystemPath.h"
#include "input/InputFrame.h"
#include "input/KeyBindings.h"

std::unique_ptr<InputFrame> InGameViews::m_inputFrame;

InGameViews::InGameViews(Game *game, const SystemPath &path, unsigned int cacheRadius) :
	m_currentView(nullptr),
	m_currentViewType(ViewType::NONE),
	m_shouldDrawGui(true),
	m_shouldDrawLabels(true),
	m_sectorView(std::make_unique<SectorView>(path, game->GetGalaxy(), cacheRadius)),
	m_galacticView(std::make_unique<UIView>("GalacticView")),
	m_systemInfoView(std::make_unique<SystemInfoView>(game)),
	m_systemView(std::make_unique<SystemView>()),
	m_worldView(std::make_unique<WorldView>(game)),
	m_deathView(std::make_unique<DeathView>()),
	m_spaceStationView(std::make_unique<UIView>("StationView")),
	m_infoView(std::make_unique<UIView>("InfoView")),
	m_cpan(std::make_unique<ShipCpanel>())
#if WITH_OBJECTVIEWER
	,m_objectViewerView(std::make_unique<ObjectViewerView>())
#endif
{
	AttachBindingCallback();
}

InGameViews::InGameViews(const Json &jsonObj, Game *game, const SystemPath &path, unsigned int cacheRadius) :
	m_currentView(nullptr),
	m_currentViewType(ViewType::NONE),
	m_shouldDrawGui(true),
	m_shouldDrawLabels(true)
{
	// Not loaded in initialization as it should diminish initialization issue
	m_galacticView = std::make_unique<UIView>("GalacticView");
	m_systemView = std::make_unique<SystemView>();
	m_systemInfoView  = std::make_unique<SystemInfoView>(game);
	m_spaceStationView = std::make_unique<UIView>("StationView");
	m_infoView = std::make_unique<UIView>("InfoView");
	m_deathView = std::make_unique<DeathView>();

#if WITH_OBJECTVIEWER
	m_objectViewerView = std::make_unique<ObjectViewerView>();
#endif

	m_cpan = std::make_unique<ShipCpanel>(jsonObj);
	m_sectorView  = std::make_unique<SectorView>(jsonObj, game->GetGalaxy(), cacheRadius);
	m_worldView = std::make_unique<WorldView>(jsonObj, game);

	AttachBindingCallback();
}

void InGameViews::SaveToJson(Json &jsonObj)
{
	m_cpan->SaveToJson(jsonObj);
	m_sectorView->SaveToJson(jsonObj);
	m_worldView->SaveToJson(jsonObj);
}

InGameViews::~InGameViews()
{
	m_inputFrame->RemoveCallbacks();
}

void InGameViews::RegisterInputBindings()
{
	using namespace KeyBindings;

	m_inputFrame = std::make_unique<InputFrame>("GeneralView");

	BindingPage &page = InputFWD::GetBindingPage("General");
	BindingGroup &group = page.GetBindingGroup("Miscellaneous");

	m_viewBindings.toggleHudMode = m_inputFrame->AddActionBinding("BindToggleHudMode", group, ActionBinding(SDLK_TAB));

	BindingGroup &group_tc = page.GetBindingGroup("TimeControl");

	m_viewBindings.increaseTimeAcceleration = m_inputFrame->AddActionBinding("BindIncreaseTimeAcceleration", group_tc, ActionBinding(SDLK_PAGEUP));
	m_viewBindings.decreaseTimeAcceleration = m_inputFrame->AddActionBinding("BindDecreaseTimeAcceleration", group_tc, ActionBinding(SDLK_PAGEDOWN));
	m_viewBindings.setTimeAccel1x = m_inputFrame->AddActionBinding("Speed1x", group_tc, ActionBinding(SDLK_PAGEDOWN));
	m_viewBindings.setTimeAccel10x = m_inputFrame->AddActionBinding("Speed10x", group_tc, ActionBinding(SDLK_PAGEDOWN));
	m_viewBindings.setTimeAccel100x = m_inputFrame->AddActionBinding("Speed100x", group_tc, ActionBinding(SDLK_PAGEDOWN));
	m_viewBindings.setTimeAccel1000x = m_inputFrame->AddActionBinding("Speed1000x", group_tc, ActionBinding(SDLK_PAGEDOWN));
	m_viewBindings.setTimeAccel10000x = m_inputFrame->AddActionBinding("Speed10000x", group_tc, ActionBinding(SDLK_PAGEDOWN));

	m_inputFrame->SetActive(true);
}

void InGameViews::AttachBindingCallback()
{
	m_inputFrame->AddCallbackFunction("BindToggleHudMode", [&](bool down) {
		if (down) return;
		if (ViewType::WORLD == m_currentViewType) {
			// triple swithes as we would show also "only labels" for WorldView... :P
			// TODO: a tri-state with [full; minimal; none]ui as a request for every view
			if (m_shouldDrawGui && m_shouldDrawLabels)
				m_shouldDrawLabels = false;
			else if (m_shouldDrawGui && !m_shouldDrawLabels)
				m_shouldDrawGui = false;
			else if (!m_shouldDrawGui) {
				m_shouldDrawGui = true;
				m_shouldDrawLabels = true;
			}
		} else {
			m_shouldDrawGui = !m_shouldDrawGui;
			m_shouldDrawLabels = true;
		}
	});
	m_inputFrame->AddCallbackFunction("BindIncreaseTimeAcceleration", [](bool down) {
		if (down) return;
		GameLocator::getGame()->RequestTimeAccelInc();
	});
	m_inputFrame->AddCallbackFunction("BindDecreaseTimeAcceleration",  [](bool down) {
		if (down) return;
		GameLocator::getGame()->RequestTimeAccelDec();
	});
	m_inputFrame->AddCallbackFunction("Speed1x",  [](bool down) {
		if (down) return;
		GameLocator::getGame()->RequestTimeAccel(Game::TIMEACCEL_1X);
	});
	m_inputFrame->AddCallbackFunction("Speed10x",  [](bool down) {
		if (down) return;
		GameLocator::getGame()->RequestTimeAccel(Game::TIMEACCEL_10X);
	});
	m_inputFrame->AddCallbackFunction("Speed100x",  [](bool down) {
		if (down) return;
		GameLocator::getGame()->RequestTimeAccel(Game::TIMEACCEL_100X);
	});
	m_inputFrame->AddCallbackFunction("Speed1000x",  [](bool down) {
		if (down) return;
		GameLocator::getGame()->RequestTimeAccel(Game::TIMEACCEL_1000X);
	});
	m_inputFrame->AddCallbackFunction("Speed10000x",  [](bool down) {
		if (down) return;
		GameLocator::getGame()->RequestTimeAccel(Game::TIMEACCEL_10000X);
	});
}

#if WITH_OBJECTVIEWER
ObjectViewerView *InGameViews::GetObjectViewerView() const
{
	return m_objectViewerView.get();
}
#endif

void InGameViews::SetView(ViewType vt)
{
	if (m_currentViewType == vt) return;

	m_shouldDrawGui = m_shouldDrawLabels = true;

	if (m_currentView) m_currentView->Detach();
	switch (vt) {
	case ViewType::NONE: m_currentViewType = ViewType::NONE; m_currentView = nullptr; break;
	case ViewType::SECTOR: m_currentViewType = ViewType::SECTOR; m_currentView = GetSectorView(); break;
	case ViewType::GALACTIC: m_currentViewType = ViewType::GALACTIC; m_currentView = GetGalacticView(); break;
	case ViewType::SYSTEMINFO: m_currentViewType = ViewType::SYSTEMINFO; m_currentView = GetSystemInfoView(); break;
	case ViewType::SYSTEM: m_currentViewType = ViewType::SYSTEM; m_currentView = GetSystemView(); break;
	case ViewType::WORLD: m_currentViewType = ViewType::WORLD; m_currentView = GetWorldView(); break;
	case ViewType::DEATH: m_currentViewType = ViewType::DEATH; m_currentView = GetDeathView(); break;
	case ViewType::SPACESTATION: m_currentViewType = ViewType::SPACESTATION; m_currentView = GetSpaceStationView(); break;
	case ViewType::INFO: m_currentViewType = ViewType::INFO; m_currentView = GetInfoView(); break;
#if WITH_OBJECTVIEWER
	case ViewType::OBJECT: m_currentViewType = ViewType::OBJECT;  m_currentView = GetObjectViewerView(); break;
#endif // WITH_OBJECTVIEWER
	}
	if (m_currentView) m_currentView->Attach();
}

bool InGameViews::HandleEscKey()
{
	switch (m_currentViewType) {
	case ViewType::OBJECT:
	case ViewType::SPACESTATION:
	case ViewType::INFO:
	case ViewType::SECTOR: SetView(ViewType::WORLD); break;
	case ViewType::GALACTIC:
	case ViewType::SYSTEMINFO:
	case ViewType::SYSTEM: SetView(ViewType::SECTOR); break;
	case ViewType::NONE:
	case ViewType::DEATH: break;
	case ViewType::WORLD: return true;
	};
	return false;
}

void InGameViews::UpdateView(const float frameTime) {
	if (m_currentView) m_currentView->Update(frameTime);
}

void InGameViews::Draw3DView() {
	if (m_currentView) m_currentView->Draw3D();
}

void InGameViews::DrawUI(const float frameTime) {
	if (!m_shouldDrawGui || m_currentViewType == ViewType::DEATH) return;
	if (m_currentView != nullptr) m_currentView->DrawUI(frameTime);
}
