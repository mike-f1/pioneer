// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef DEATH_VIEW_H
#define DEATH_VIEW_H

#include "libs/RefCounted.h"
#include "View.h"

#include <memory>

class Camera;
class CameraContext;

class DeathView final: public View {
public:
	DeathView();
	virtual ~DeathView();

	void Init();

	virtual void Update(const float frameTime) override;
	virtual void Draw3D() override;

protected:
	virtual void OnSwitchTo() override;

private:
	RefCountedPtr<CameraContext> m_cameraContext;
	std::unique_ptr<Camera> m_camera;
	float m_cameraDist;
};

#endif
