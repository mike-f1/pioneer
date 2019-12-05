#ifndef INGAMEVIEWS_H
#define INGAMEVIEWS_H

#include "JsonFwd.h"
#include "RefCounted.h"
#include "galaxy/GalaxyCache.h"
#include "galaxy/SystemPath.h"

#include <SDL_events.h>

class Game;

class SectorView;
class UIView;
class SystemInfoView;
class SystemView;
class WorldView;
class DeathView;
class ShipCpanel;
class ObjectViewerView;
class View;

namespace Graphics {
	class Renderer;
}

enum class ViewType {
	NONE,
	SECTOR,
	GALACTIC,
	SYSTEMINFO,
	SYSTEM,
	WORLD,
	DEATH,
	OBJECT
};

class InGameViews {
public:
	InGameViews(Game *game, const SystemPath &path, RefCountedPtr<SectorCache::Slave> sectorCache);
	InGameViews(const Json &jsonObj, Game *game, const SystemPath &path, RefCountedPtr<SectorCache::Slave> sectorCache);
	~InGameViews();

	void SaveToJson(Json &jsonObj);

	void InitInGameViews(Game *game, const SystemPath &path);
	void CreateInGameViews(const SystemPath &path);
	void LoadViewsFromJson(const Json &jsonObj, const SystemPath &path);
	void DestroyInGameViews();

	void LoadFromJson(const Json &jsonObj, Game *game, const SystemPath &path);

	void SetRenderer(Graphics::Renderer *r);

	void SetView(ViewType vt);
	View *GetView() { return m_currentView; } // <-- Only for a check on template name in Pi::

	bool DrawGui();

	bool IsEmptyView() const { return nullptr == m_currentView; }
	bool IsSectorView() const { return ViewType::SECTOR == m_currentViewType; }
	bool IsGalacticView() const { return ViewType::GALACTIC == m_currentViewType; }
	bool IsSystemInfoView() const { return ViewType::SYSTEMINFO == m_currentViewType; }
	bool IsSystemView() const { return ViewType::SYSTEM == m_currentViewType; }
	bool IsWorldView() const { return ViewType::WORLD == m_currentViewType; }
	bool IsDeathView() const { return ViewType::DEATH == m_currentViewType; }
	bool IsObjectView() const { return ViewType::OBJECT == m_currentViewType; }

	SectorView *GetSectorView() const { return m_sectorView; }
	UIView *GetGalacticView() const { return m_galacticView; }
	SystemInfoView *GetSystemInfoView() const { return m_systemInfoView; }
	SystemView *GetSystemView() const { return m_systemView; }
	WorldView *GetWorldView() const { return m_worldView; }
	DeathView *GetDeathView() const { return m_deathView; }
	ShipCpanel *GetCpan() const { return m_cpan; }

	/* Only use #if WITH_OBJECTVIEWER */
	ObjectViewerView *GetObjectViewerView() const;

	void HandleSDLEvent(SDL_Event event);
	void UpdateView();
	void Draw3DView();

private:
	View *m_currentView;
	ViewType m_currentViewType;

	SectorView *m_sectorView;
	UIView *m_galacticView;
	SystemInfoView *m_systemInfoView;
	SystemView *m_systemView;
	WorldView *m_worldView;
	DeathView *m_deathView;
	ShipCpanel *m_cpan;

	/* Only use #if WITH_OBJECTVIEWER */
	ObjectViewerView *m_objectViewerView;
};

#endif // INGAMEVIEWS_H
