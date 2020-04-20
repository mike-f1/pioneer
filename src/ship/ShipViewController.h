// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#pragma once

#include "CameraController.h"
#include "InputFrame.h"
#include "InteractionController.h"
#include "utils.h"

class Ship;

namespace KeyBindings {
	struct ActionBinding;
	struct AxisBinding;
}

class ShipViewController : public InteractionController {
	friend class WorldView;
public:
	ShipViewController(WorldView *v) :
		InteractionController(v),
		m_camType(CAM_INTERNAL),
		headtracker_input_priority(false)
	{
		RegisterInputBindings();
	}

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
		using Action = KeyBindings::ActionBinding;
		using Axis = KeyBindings::AxisBinding;

		Axis *cameraYaw;
		Axis *cameraPitch;
		Axis *cameraRoll;
		Axis *cameraZoom;

		Axis *lookYaw;
		Axis *lookPitch;

		Action *frontCamera;
		Action *rearCamera;
		Action *leftCamera;
		Action *rightCamera;
		Action *topCamera;
		Action *bottomCamera;

		Action *cycleCameraMode;
		Action *resetCamera;

	} m_inputBindings;

	std::unique_ptr<InputFrame> m_inputFrame;

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
