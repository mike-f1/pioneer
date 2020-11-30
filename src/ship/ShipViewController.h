// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#pragma once

#include "CameraController.h"
#include "InteractionController.h"
#include "input/InputFwd.h"
#include "libs/utils.h"

#include <sigc++/sigc++.h>

class InputFrame;
class Ship;

class ShipViewController : public InteractionController {
	friend class WorldView;
public:
	ShipViewController(WorldView *v);

	~ShipViewController();

	void Update(const float frameTime) override;
	void Activated() override;
	void Deactivated() override;

	enum CamType {
		CAM_INTERNAL,
		CAM_EXTERNAL,
		CAM_SIDEREAL,
		CAM_FLYBY
	};
	void SetCamType(enum CamType);
	enum CamType GetCamType() const { return m_camType; }
	CameraController *GetCameraController() const { return m_activeCameraController; }

	sigc::signal<void> onChangeCamType;

	void Init(Ship *ship);
	void LoadFromJson(const Json &jsonObj);
	void SaveToJson(Json &jsonObj);

	// Here temporarely because of initialization order
	void SetCamType(Ship *ship, enum CamType c);

	struct InputBinding {
		AxisId cameraYaw;
		AxisId cameraPitch;
		AxisId cameraRoll;
		AxisId cameraZoom;

		AxisId lookYaw;
		AxisId lookPitch;

		ActionId frontCamera;
		ActionId rearCamera;
		ActionId leftCamera;
		ActionId rightCamera;
		ActionId topCamera;
		ActionId bottomCamera;

		ActionId cycleCameraMode;
		ActionId resetCamera;
	} m_inputBindings;

	std::unique_ptr<InputFrame> m_inputFrame;
	std::unique_ptr<InputFrame> m_shipViewFrame;

private:
	void ChangeInternalCameraMode(InternalCameraController::Mode m);

	void RegisterInputBindings();

	void OnCamReset(bool down);

	enum CamType m_camType;

	std::unique_ptr<InternalCameraController> m_internalCameraController;
	std::unique_ptr<ExternalCameraController> m_externalCameraController;
	std::unique_ptr<SiderealCameraController> m_siderealCameraController;
	std::unique_ptr<FlyByCameraController> m_flybyCameraController;
	CameraController *m_activeCameraController; //one of the above

	bool headtracker_input_priority;
};
