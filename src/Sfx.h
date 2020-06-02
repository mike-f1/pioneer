// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _SFX_H
#define _SFX_H

#include "FrameId.h"
#include "JsonFwd.h"
#include "libs/vector3.h"
#include "libs/vector2.h"

#include <deque>

class Body;
class Frame;

namespace Graphics {
	class Material;
	class RenderState;
	enum class EffectType;
}

enum SFX_TYPE {
	TYPE_EXPLOSION = 1,
	TYPE_DAMAGE,
	TYPE_SMOKE,
	TYPE_NONE
};

class Sfx {
public:
	friend class SfxManager;
	Sfx() = delete;
	Sfx(const vector3d &pos, const vector3d &vel, const float speed, const SFX_TYPE type);
	Sfx(const Json &jsonObj);
	void SetPosition(const vector3d &p);
	const vector3d &GetPosition() const { return m_pos; }

	float AgeBlend() const;

private:
	void TimeStepUpdate(const float timeStep);
	void SaveToJson(Json &jsonObj);

	vector3d m_pos;
	vector3d m_vel;
	float m_age;
	float m_speed;
	enum SFX_TYPE m_type;
};

class SfxManager {
public:
	friend class Sfx;

	static void Add(const Body *, SFX_TYPE);
	static void AddExplosion(Body *);
	static void AddThrustSmoke(const Body *b, float speed, const vector3d &adjustpos);
	static void TimeStepAll(const float timeStep, FrameId f);
	static void RenderAll(FrameId f, const FrameId camFrame);
	static void ToJson(Json &jsonObj, const FrameId f);
	static void FromJson(const Json &jsonObj, FrameId f);

	//create shared models
	static void Init();
	static void Uninit();
	static std::unique_ptr<Graphics::Material> damageParticle;
	static std::unique_ptr<Graphics::Material> ecmParticle;
	static std::unique_ptr<Graphics::Material> smokeParticle;
	static std::unique_ptr<Graphics::Material> explosionParticle;
	static Graphics::RenderState *alphaState;
	static Graphics::RenderState *additiveAlphaState;
	static Graphics::RenderState *alphaOneState;

	SfxManager();

private:
	template <typename... Args>
	void AddInstance(SFX_TYPE type, Args&&... args) { m_instances[type].emplace_back(std::forward<Args>(args)...); }

	void Cleanup();
	size_t GetNumberInstances(const SFX_TYPE t) const { return m_instances[t].size(); }
	Sfx &GetInstanceByIndex(const SFX_TYPE t, const size_t i) { return m_instances[t][i]; }

	// types
	struct MaterialData {
		MaterialData();
		Graphics::EffectType effect;
		uint32_t num_textures;
		int num_imgs_wide;
		float coord_downscale;
	};

	// methods
	static SfxManager *AllocSfxInFrame(FrameId f);
	static vector2f CalculateOffset(const enum SFX_TYPE, const Sfx &);
	static bool SplitMaterialData(const std::string &spec, MaterialData &output);

	// static members
	static MaterialData m_materialData[TYPE_NONE];

	// members
	// per-frame
	std::deque<Sfx> m_instances[TYPE_NONE];
};

#endif /* _SFX_H */
