// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _SECTORVIEW_H
#define _SECTORVIEW_H

#include "UIView.h"
#include "galaxy/Sector.h"
#include "galaxy/SystemPath.h"
#include "graphics/Drawables.h"
#include "input/InputFwd.h"
#include <set>
#include <string>
#include <vector>

class Galaxy;
class InputFrame;

namespace KeyBindings {
	struct ActionBinding;
	struct AxisBinding;
}

namespace Graphics {
	class RenderState;
}

class SectorView : public UIView {
public:
	SectorView(const SystemPath &path, RefCountedPtr<Galaxy> galaxy, unsigned int cacheRadius);
	SectorView(const Json &jsonObj, RefCountedPtr<Galaxy> galaxy, unsigned int cacheRadius);
	virtual ~SectorView();

	virtual void Update(const float frameTime) override;
	virtual void Draw3D() override;
	virtual void DrawUI(const float frameTime) override;

	vector3f GetPosition() const { return m_pos; }
	SystemPath GetCurrent() const { return m_current; }
	SystemPath GetSelected() const { return m_selected; }
	void SetSelected(const SystemPath &path);
	SystemPath GetHyperspaceTarget() const { return m_hyperspaceTarget; }
	void SetHyperspaceTarget(const SystemPath &path);
	void FloatHyperspaceTarget();
	void LockHyperspaceTarget(bool lock);
	bool GetLockHyperspaceTarget() const { return !m_matchTargetToSelection; }
	void ResetHyperspaceTarget();
	void GotoSector(const SystemPath &path);
	void GotoSystem(const SystemPath &path);
	void GotoCurrentSystem() { GotoSystem(m_current); }
	void GotoSelectedSystem() { GotoSystem(m_selected); }
	void GotoHyperspaceTarget() { GotoSystem(m_hyperspaceTarget); }
	void SwapSelectedHyperspaceTarget();
	virtual void SaveToJson(Json &jsonObj) override;

	sigc::signal<void> onHyperspaceTargetChanged;

	double GetZoomLevel() const;
	void ZoomIn();
	void ZoomOut();
	vector3f GetCenterSector();
	double GetCenterDistance();
	void SetShowFactionColor(bool value) { m_showFactionColor = value; m_rebuildFarSector = true; }
	bool GetShowFactionColor() const { return m_showFactionColor; }
	void SetDrawUninhabitedLabels(bool value) { m_drawUninhabitedLabels = value; }
	bool GetDrawUninhabitedLabels() const { return m_drawUninhabitedLabels; }
	void SetDrawVerticalLines(bool value) { m_drawVerticalLines = value; }
	bool GetDrawVerticalLines() const { return m_drawVerticalLines; }
	void SetDrawOutRangeLabels(bool value) { m_drawOutRangeLabels = value; }
	bool GetDrawOutRangeLabels() const { return m_drawOutRangeLabels; }
	void SetAutomaticSystemSelection(bool value) { m_automaticSystemSelection = value; }
	bool GetAutomaticSystemSelection() const { return m_automaticSystemSelection; }
	std::vector<SystemPath> GetNearbyStarSystemsByName(std::string pattern);
	const std::set<const Faction *> &GetVisibleFactions() const { return m_visibleFactions; }
	const std::set<const Faction *> &GetHiddenFactions() const { return m_hiddenFactions; }
	void SetFactionVisible(const Faction *faction, bool visible);

	// HyperJump Route Planner
	bool MoveRouteItemUp(const std::vector<SystemPath>::size_type element);
	bool MoveRouteItemDown(const std::vector<SystemPath>::size_type element);
	void AddToRoute(const SystemPath &path);
	bool RemoveRouteItem(const std::vector<SystemPath>::size_type element);
	void ClearRoute();
	std::vector<SystemPath> GetRoute();
	void AutoRoute(const SystemPath &start, const SystemPath &target, std::vector<SystemPath> &outRoute) const;
	void SetDrawRouteLines(bool value) { m_drawRouteLines = value; }

protected:
	virtual void OnSwitchTo() override;
	virtual void OnSwitchFrom() override;

private:
	void InitDefaults();
	void InitObject(unsigned int cacheRadius);
	void RegisterInputBindings();

	void PrepareLegs(const matrix4x4f &trans, const vector3f &pos, int z_diff);
	void PrepareGrid(const matrix4x4f &trans, int radius);
	void DrawNearSectors(const matrix4x4f &modelview);
	void DrawNearSector(const int sx, const int sy, const int sz, const vector3f &playerAbsPos, const matrix4x4f &trans);

	void DrawFarSectors(const matrix4x4f &modelview);
	void BuildFarSector(RefCountedPtr<Sector> sec, const vector3f &origin, std::vector<vector3f> &points, std::vector<Color> &colors);
	void AddStarBillboard(const matrix4x4f &modelview, const vector3f &pos, const Color &col, float size);

	typedef std::pair<const Sector::System *,vector3d> t_systemAndPos;
	typedef std::vector<t_systemAndPos> t_systemsAndPosVector;

	void PutDiamonds(const t_systemsAndPosVector &homeworlds);

	void CollectSystems(RefCountedPtr<Sector> sec, const vector3f &origin, int drawRadius, SectorView::t_systemsAndPosVector &systems);

	t_systemsAndPosVector CollectHomeworlds(const vector3f &origin);

	t_systemsAndPosVector m_systems;

	void OnClickSystem(const SystemPath &path);

	void ShrinkCache();

	void OnMapLockHyperspaceToggle(bool down);
	void OnToggleSelectionFollowView(bool down);
	void UpdateBindings();

	RefCountedPtr<Galaxy> m_galaxy;

	bool m_inSystem;
	bool m_farMode;

	SystemPath m_current;
	SystemPath m_selected;

	vector3f m_pos;
	vector3f m_posMovingTo;

	float m_rotXDefault, m_rotZDefault, m_zoomDefault;

	float m_rotX, m_rotZ;
	float m_rotXMovingTo, m_rotZMovingTo;

	float m_zoom;
	float m_zoomClamped;
	float m_zoomMovingTo;

	SystemPath m_hyperspaceTarget;
	bool m_showFactionColor;
	bool m_matchTargetToSelection;
	bool m_automaticSystemSelection;
	bool m_drawUninhabitedLabels;
	bool m_drawOutRangeLabels;
	bool m_drawVerticalLines;

	float m_lastFrameTime;

	std::unique_ptr<Graphics::Drawables::Disk> m_disk;

	std::set<const Faction *> m_visibleFactions;
	std::set<const Faction *> m_hiddenFactions;

	struct SectorBinding {
		ActionId mapLockHyperspaceTarget;
		ActionId mapToggleSelectionFollowView;
		ActionId mapWarpToCurrent;
		ActionId mapWarpToSelected;
		ActionId mapWarpToHyperspaceTarget;
		ActionId mapViewReset;

		AxisId mapViewShiftLeftRight;
		AxisId mapViewShiftUpDown;
		AxisId mapViewShiftForwardBackward;

		AxisId mapViewZoom;

		AxisId mapViewRotateLeftRight;
		AxisId mapViewRotateUpDown;

	} m_sectorBindings;

	std::unique_ptr<InputFrame> m_inputFrame;

	RefCountedPtr<SectorCache::Slave> m_sectorCache;
	std::string m_previousSearch;

	float m_playerHyperspaceRange;
	Graphics::Drawables::Line3D m_selectedLine;
	Graphics::Drawables::Line3D m_secondLine;
	Graphics::Drawables::Line3D m_jumpLine;

	// HyperJump Route Planner Stuff
	std::vector<SystemPath> m_route;
	bool m_drawRouteLines;
	void PrepareRouteLines(const vector3f &playerAbsPos, const matrix4x4f &trans);

	Graphics::RenderState *m_solidState;
	Graphics::RenderState *m_alphaBlendState;
	Graphics::RenderState *m_jumpSphereState;
	RefCountedPtr<Graphics::Material> m_material; //flat colour
	RefCountedPtr<Graphics::Material> m_starMaterial;

	std::vector<vector3f> m_farstars;
	std::vector<Color> m_farstarsColor;

	vector3f m_secPosFar;
	int m_radiusFar;
	bool m_rebuildFarSector;

	int m_cacheXMin;
	int m_cacheXMax;
	int m_cacheYMin;
	int m_cacheYMax;
	int m_cacheZMin;
	int m_cacheZMax;

	std::unique_ptr<Graphics::VertexArray> m_lineVerts; // used for "legs"
	std::unique_ptr<Graphics::VertexArray> m_secLineVerts; // used for grid and route lines
	RefCountedPtr<Graphics::Material> m_fresnelMat;
	std::unique_ptr<Graphics::Drawables::Sphere3D> m_jumpSphere;
	std::unique_ptr<Graphics::VertexArray> m_starVerts;

	Graphics::Drawables::Lines m_lines;
	Graphics::Drawables::Lines m_sectorlines;
	Graphics::Drawables::Points m_farstarsPoints;
};

#endif /* _SECTORVIEW_H */
