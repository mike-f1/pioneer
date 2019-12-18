// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _NAVLIGHTS_H
#define _NAVLIGHTS_H

#include "SDL_stdinc.h"
#include "JsonFwd.h"
#include "RefCounted.h"

#include "graphics/VertexArray.h"

namespace Graphics {
	class Renderer;
	class RenderState;
	class VertexBuffer;
} // namespace Graphics

namespace SceneGraph {
	class Model;
	class Billboard;
} // namespace SceneGraph

/*
 * Blinking navigation lights for ships and stations
 */
class NavLights {
public:
	enum LightColor {
		NAVLIGHT_RED = 0,
		NAVLIGHT_GREEN = 1,
		NAVLIGHT_BLUE = 2,
		NAVLIGHT_YELLOW = 3,
		NAVLIGHT_OFF = 15
	};

	struct LightBulb {
		LightBulb(Uint8 group, Uint8 mask, Uint8 color, SceneGraph::Billboard *bb);
		Uint8 group;
		Uint8 mask; //bitmask: 00001111 light on half the period, 11111111 light on the entire period etc...
		Uint8 color;
		SceneGraph::Billboard *billboard;
	};

	NavLights(SceneGraph::Model *, float period = 2.f);
	virtual ~NavLights();
	virtual void SaveToJson(Json &jsonObj);
	virtual void LoadFromJson(const Json &jsonObj);

	void SetEnabled(bool on) { m_enabled = on; }
	void Update(float time);
	void Render();
	void SetColor(unsigned int group, LightColor);
	void SetMask(unsigned int group, uint8_t mask);

	static void Init();
	static void Uninit();

protected:
	std::map<Uint32, std::vector<LightBulb>> m_groupLights;
	float m_time;
	float m_period;
	bool m_enabled;

	Graphics::VertexArray m_billboardTris;
	RefCountedPtr<Graphics::VertexBuffer> m_billboardVB;
	Graphics::RenderState *m_billboardRS;
};

#endif
