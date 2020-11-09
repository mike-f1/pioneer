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

	SectorView *GetSectorView() const { return m_sectorView.get(); }
	UIView *GetGalacticView() const { return m_galacticView.get(); }
	SystemInfoView *GetSystemInfoView() const { return m_systemInfoView.get(); }
	SystemView *GetSystemView() const { return m_systemView.get(); }
	WorldView *GetWorldView() const { return m_worldView.get(); }
	DeathView *GetDeathView() const { return m_deathView.get(); }
	UIView *GetSpaceStationView() const { return m_spaceStationView.get(); }
	UIView *GetInfoView() const { return m_infoView.get(); }
	ShipCpanel *GetCpan() const { return m_cpan.get(); }

	/* Only use #if WITH_OBJECTVIEWER */
	ObjectViewerView *GetObjectViewerView() const;

	void UpdateView(const float frameTime);
	void Draw3DView();

	void DrawUI(const float frameTime);

private:
	View *m_currentView;
	ViewType m_currentViewType;

	std::unique_ptr<SectorView> m_sectorView;
	std::unique_ptr<UIView> m_galacticView;
	std::unique_ptr<SystemInfoView> m_systemInfoView;
	std::unique_ptr<SystemView> m_systemView;
	std::unique_ptr<WorldView> m_worldView;
	std::unique_ptr<DeathView> m_deathView;
	std::unique_ptr<UIView> m_spaceStationView;
	std::unique_ptr<UIView> m_infoView;
	std::unique_ptr<ShipCpanel> m_cpan;

#ifdef WITH_OBJECTVIEWER
	/* Only use #if WITH_OBJECTVIEWER */
	std::unique_ptr<ObjectViewerView> m_objectViewerView;
#endif
};

#endif // INGAMEVIEWS_H
