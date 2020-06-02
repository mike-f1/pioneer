#ifndef INGAMEVIEWS_H
#define INGAMEVIEWS_H

#include "buildopts.h"

#include "JsonFwd.h"
#include "libs/RefCounted.h"

#include <memory>

class Game;
class SystemPath;

class SectorView;
class UIView;
class SystemInfoView;
class SystemView;
class WorldView;
class DeathView;
class ShipCpanel;
class ObjectViewerView;
class View;

enum class ViewType {
	NONE,
	SECTOR,
	GALACTIC,
	SYSTEMINFO,
	SYSTEM,
	WORLD,
	DEATH,
	SPACESTATION,
	INFO,
	OBJECT
};

class InGameViews {
public:
	InGameViews(Game *game, const SystemPath &path, unsigned int cacheRadius);
	InGameViews(const Json &jsonObj, Game *game, const SystemPath &path, unsigned int cacheRadius);
	~InGameViews();

	InGameViews(const InGameViews &) = delete;
	InGameViews &operator = (const InGameViews &) = delete;

	void SaveToJson(Json &jsonObj);

	void SetView(ViewType vt);

	bool ShouldDrawGui();

	ViewType GetViewType() const { return m_currentViewType; }

	bool IsEmptyView() const { return nullptr == m_currentView; }
	bool IsSectorView() const { return ViewType::SECTOR == m_currentViewType; }
	bool IsGalacticView() const { return ViewType::GALACTIC == m_currentViewType; }
	bool IsSystemInfoView() const { return ViewType::SYSTEMINFO == m_currentViewType; }
	bool IsSystemView() const { return ViewType::SYSTEM == m_currentViewType; }
	bool IsWorldView() const { return ViewType::WORLD == m_currentViewType; }
	bool IsDeathView() const { return ViewType::DEATH == m_currentViewType; }
	bool IsSpaceStationView() const { return ViewType::SPACESTATION == m_currentViewType; }
	bool IsInfoView() const { return ViewType::INFO == m_currentViewType; }
	bool IsObjectView() const { return ViewType::OBJECT == m_currentViewType; }

	SectorView *GetSectorView() const { return m_sectorView; }
	UIView *GetGalacticView() const { return m_galacticView; }
	SystemInfoView *GetSystemInfoView() const { return m_systemInfoView; }
	SystemView *GetSystemView() const { return m_systemView; }
	WorldView *GetWorldView() const { return m_worldView.get(); }
	DeathView *GetDeathView() const { return m_deathView; }
	UIView *GetSpaceStationView() const { return m_spaceStationView; }
	UIView *GetInfoView() const { return m_infoView; }
	ShipCpanel *GetCpan() const { return m_cpan; }

	/* Only use #if WITH_OBJECTVIEWER */
	ObjectViewerView *GetObjectViewerView() const;

	void UpdateView(const float frameTime);
	void Draw3DView();

	void DrawUI(const float frameTime);

private:
	View *m_currentView;
	ViewType m_currentViewType;

	SectorView *m_sectorView;
	UIView *m_galacticView;
	SystemInfoView *m_systemInfoView;
	SystemView *m_systemView;
	std::unique_ptr<WorldView> m_worldView;
	DeathView *m_deathView;
	UIView *m_spaceStationView;
	UIView *m_infoView;
	ShipCpanel *m_cpan;

#ifdef WITH_OBJECTVIEWER
	/* Only use #if WITH_OBJECTVIEWER */
	ObjectViewerView *m_objectViewerView;
#endif
};

#endif // INGAMEVIEWS_H
