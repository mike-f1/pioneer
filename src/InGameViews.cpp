#include "buildopts.h"

#include "InGameViews.h"

#include "DeathView.h"
#include "Game.h"
#include "ObjectViewerView.h"
#include "SectorView.h"
#include "ShipCpanel.h"
#include "SystemInfoView.h"
#include "SystemView.h"
#include "UIView.h"
#include "WorldView.h"
#include "galaxy/Galaxy.h"
#include "galaxy/SystemPath.h"

InGameViews::InGameViews(Game *game, const SystemPath &path, unsigned int cacheRadius) :
	m_currentView(nullptr),
	m_currentViewType(ViewType::NONE),
	m_sectorView(new SectorView(path, game->GetGalaxy(), cacheRadius)),
	m_galacticView(new UIView("GalacticView")),
	m_systemInfoView(new SystemInfoView(game)),
	m_systemView(new SystemView()),
	m_worldView(new WorldView(game)),
	m_deathView(new DeathView()),
	m_spaceStationView(new UIView("StationView")),
	m_infoView(new UIView("InfoView")),
	m_cpan(new ShipCpanel())
#if WITH_OBJECTVIEWER
	,m_objectViewerView(new ObjectViewerView(game))
#endif
{
}

InGameViews::InGameViews(const Json &jsonObj, Game *game, const SystemPath &path, unsigned int cacheRadius) :
	m_currentView(nullptr),
	m_currentViewType(ViewType::NONE)
{
	// Not loaded first as it should diminish initialization issue
	m_galacticView = new UIView("GalacticView");
	m_systemView = new SystemView();
	m_systemInfoView = new SystemInfoView(game);
	m_spaceStationView = new UIView("StationView");
	m_infoView = new UIView("InfoView");
	m_deathView = new DeathView();

#if WITH_OBJECTVIEWER
	m_objectViewerView = new ObjectViewerView(game);
#endif

	m_cpan = new ShipCpanel(jsonObj);
	m_sectorView = new SectorView(jsonObj, game->GetGalaxy(), cacheRadius);
	m_worldView = new WorldView(jsonObj, game);
}

void InGameViews::SaveToJson(Json &jsonObj)
{
	m_cpan->SaveToJson(jsonObj);
	m_sectorView->SaveToJson(jsonObj);
	m_worldView->SaveToJson(jsonObj);
}

InGameViews::~InGameViews()
{
#if WITH_OBJECTVIEWER
	delete m_objectViewerView;
#endif

	delete m_deathView;
	delete m_infoView;
	delete m_spaceStationView;
	delete m_systemInfoView;
	delete m_systemView;
	delete m_galacticView;
	delete m_worldView;
	delete m_sectorView;
	delete m_cpan;
}

#if WITH_OBJECTVIEWER
ObjectViewerView *InGameViews::GetObjectViewerView() const
{
	return m_objectViewerView;
}
#endif

void InGameViews::SetView(ViewType vt)
{
	if (m_currentViewType == vt) return;

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

bool InGameViews::DrawGui() {
	return (!IsWorldView() ? true : m_worldView->DrawGui());
}

void InGameViews::UpdateView(const float frameTime) {
	if (m_currentView != nullptr) m_currentView->Update(frameTime);
}

void InGameViews::Draw3DView() {
	if (m_currentView != nullptr) m_currentView->Draw3D();
}
