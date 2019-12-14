// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _OBJECTVIEWERVIEW_H
#define _OBJECTVIEWERVIEW_H

#include "RefCounted.h"
#include "UIView.h"
#include "matrix4x4.h"
#include <memory>

class Body;
class Camera;
class CameraContext;
class Game;

namespace Gui {
	class Label;
	class TextEntry;
	class VBox;
}

class ObjectViewerView : public UIView {
public:
	ObjectViewerView(Game *game);
	virtual void Update(const float frameTime) override;
	virtual void Draw3D() override;

protected:
	virtual void OnSwitchTo();

private:
	float m_viewingDist;
	Gui::Label *m_infoLabel;
	Gui::VBox *m_vbox;
	Body *m_lastTarget;
	matrix4x4d m_camRot;

	RefCountedPtr<CameraContext> m_cameraContext;
	std::unique_ptr<Camera> m_camera;

	Gui::TextEntry *m_sbodyMass;
	Gui::TextEntry *m_sbodyRadius;
	Gui::TextEntry *m_sbodySeed;
	Gui::TextEntry *m_sbodyVolatileGas;
	Gui::TextEntry *m_sbodyVolatileLiquid;
	Gui::TextEntry *m_sbodyVolatileIces;
	Gui::TextEntry *m_sbodyLife;
	Gui::TextEntry *m_sbodyVolcanicity;
	Gui::TextEntry *m_sbodyMetallicity;
	void OnChangeTerrain();
	void OnRandomSeed();
	void OnNextSeed();
	void OnPrevSeed();
};

#endif /* _OBJECTVIEWERVIEW_H */
