// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Shields.h"

#include "GameSaveError.h"
#include "JsonUtils.h"
#include "graphics/Material.h"
#include "graphics/Renderer.h"
#include "graphics/RendererLocator.h"
#include "graphics/RenderState.h"
#include "libs/utils.h"
#include "scenegraph/FindNodeVisitor.h"
#include "scenegraph/MatrixTransform.h"
#include "scenegraph/Model.h"
#include "scenegraph/StaticGeometry.h"
#include "scenegraph/ShieldHelper.h"

#include <sstream>
#include <SDL_timer.h>

namespace {
	static RefCountedPtr<Graphics::Material> s_matShield;
	static ShieldRenderParameters s_renderParams;

	static RefCountedPtr<Graphics::Material> GetGlobalShieldMaterial()
	{
		return s_matShield;
	}
} // namespace

typedef std::vector<Shields::Shield>::iterator ShieldIterator;

//static
bool Shields::s_initialised = false;

Shields::Shield::Shield(const Color3ub &_colour, const matrix4x4f &matrix, SceneGraph::StaticGeometry *_sg) :
	m_colour(_colour),
	m_matrix(matrix),
	m_mesh(_sg)
{}

Shields::Hits::Hits(const vector3d &_pos, const uint32_t _start, const uint32_t _end) :
	pos(_pos),
	start(_start),
	end(_end)
{}

void Shields::Init()
{
	assert(!s_initialised);

	// create our global shield material
	Graphics::MaterialDescriptor desc;
	desc.textures = 0;
	desc.lighting = true;
	desc.alphaTest = false;
	desc.effect = Graphics::EffectType::SHIELD;
	s_matShield.Reset(RendererLocator::getRenderer()->CreateMaterial(desc));
	s_matShield->diffuse = Color(1.0f, 1.0f, 1.0f, 1.0f);

	s_initialised = true;
}

void Shields::Uninit()
{
	assert(s_initialised);

	s_initialised = false;
}

Shields::Shields(SceneGraph::Model *model) :
	m_enabled(false)
{
	assert(s_initialised);

	using SceneGraph::MatrixTransform;
	using SceneGraph::Node;
	using SceneGraph::StaticGeometry;

	//This will find all matrix transforms meant for shields.
	SceneGraph::FindNodeVisitor shieldFinder(SceneGraph::FindNodeVisitor::MATCH_NAME_ENDSWITH, ShieldHelper::s_matrixTransformName);
	model->GetRoot()->Accept(shieldFinder);
	const std::vector<Node *> &results = shieldFinder.GetResults();

	//Store pointer to the shields for later.
	for (unsigned int i = 0; i < results.size(); i++) {
		MatrixTransform *mt = dynamic_cast<MatrixTransform *>(results.at(i));
		assert(mt);

		for (uint32_t iChild = 0; iChild < mt->GetNumChildren(); ++iChild) {
			Node *node = mt->GetChildAt(iChild);
			if (node) {
				RefCountedPtr<StaticGeometry> sg(dynamic_cast<StaticGeometry *>(node));
				assert(sg.Valid());
				sg->SetNodeMask(SceneGraph::NODE_TRANSPARENT);

				Graphics::RenderStateDesc rsd;
				rsd.blendMode = Graphics::BLEND_ALPHA;
				rsd.depthWrite = false;
				sg->SetRenderState(RendererLocator::getRenderer()->CreateRenderState(rsd));

				// set the material
				for (uint32_t iMesh = 0; iMesh < sg->GetNumMeshes(); ++iMesh) {
					StaticGeometry::Mesh &rMesh = sg->GetMeshAt(iMesh);
					rMesh.material = GetGlobalShieldMaterial();
				}

				m_shields.push_back(Shield(Color3ub(255), mt->GetTransform(), sg.Get()));
			}
		}
	}
}

Shields::~Shields()
{
}

void Shields::SaveToJson(Json &jsonObj)
{
	Json shieldsObj({}); // Create JSON object to contain shields data.

	shieldsObj["enabled"] = m_enabled;
	shieldsObj["num_shields"] = m_shields.size();

	Json shieldArray = Json::array(); // Create JSON array to contain shield data.
	for (ShieldIterator it = m_shields.begin(); it != m_shields.end(); ++it) {
		Json shieldArrayEl({}); // Create JSON object to contain shield.
		shieldArrayEl["color"] = it->m_colour;
		shieldArrayEl["mesh_name"] = it->m_mesh->GetName();
		shieldArray.push_back(shieldArrayEl); // Append shield object to array.
	}
	shieldsObj["shield_array"] = shieldArray; // Add shield array to shields object.

	jsonObj["shields"] = shieldsObj; // Add shields object to supplied object.
}

void Shields::LoadFromJson(const Json &jsonObj)
{
	try {
		Json shieldsObj = jsonObj["shields"];

		m_enabled = shieldsObj["enabled"];
		assert(shieldsObj["num_shields"].get<unsigned int>() == m_shields.size());

		Json shieldArray = shieldsObj["shield_array"].get<Json::array_t>();

		for (unsigned int i = 0; i < shieldArray.size(); ++i) {
			Json shieldArrayEl = shieldArray[i];
			for (ShieldIterator it = m_shields.begin(); it != m_shields.end(); ++it) {
				if (shieldArrayEl["mesh_name"] == it->m_mesh->GetName()) {
					it->m_colour = shieldArrayEl["color"];
					break;
				}
			}
		}
	} catch (Json::type_error &) {
		Output("Loading error in '%s' in function '%s' \n", __FILE__, __func__);
		throw SavedGameCorruptException();
	}
}

void Shields::Update(const float coolDown, const float shieldStrength)
{
	// update hits on the shields
	const uint32_t tickTime = SDL_GetTicks();
	{
		HitIterator it = m_hits.begin();
		while (it != m_hits.end()) {
			if (tickTime > it->end) {
				it = m_hits.erase(it);
			} else {
				++it;
			}
		}
	}

	if (!m_enabled) {
		for (ShieldIterator it = m_shields.begin(); it != m_shields.end(); ++it) {
			it->m_mesh->SetNodeMask(0x0);
		}
		return;
	}

	// setup the render params
	if (shieldStrength > 0.0f) {
		s_renderParams.strength = shieldStrength;
		s_renderParams.coolDown = coolDown;

		uint32_t numHits = m_hits.size();
		for (uint32_t i = 0; i < numHits && i < MAX_SHIELD_HITS; ++i) {
			const Hits &hit = m_hits[i];
			s_renderParams.hitPos[i] = vector3f(hit.pos.x, hit.pos.y, hit.pos.z);

			//Calculate the impact's radius dependant on time
			uint32_t dif1 = hit.end - hit.start;
			uint32_t dif2 = tickTime - hit.start;
			//Range from start (0.0) to end (1.0)
			float dif = float(dif2 / (dif1 * 1.0f));

			s_renderParams.radii[i] = dif;
		}
		s_renderParams.numHits = m_hits.size();
	}

	// update the shield visibility
	for (ShieldIterator it = m_shields.begin(); it != m_shields.end(); ++it) {
		if (shieldStrength > 0.0f) {
			it->m_mesh->SetNodeMask(SceneGraph::NODE_TRANSPARENT);

			GetGlobalShieldMaterial()->specialParameter0 = &s_renderParams;
		} else {
			it->m_mesh->SetNodeMask(0x0);
		}
	}
}

void Shields::SetColor(const Color3ub &inCol)
{
	for (ShieldIterator it = m_shields.begin(); it != m_shields.end(); ++it) {
		it->m_colour = inCol;
	}
}

void Shields::AddHit(const vector3d &hitPos)
{
	uint32_t tickTime = SDL_GetTicks();
	m_hits.push_back(Hits(hitPos, tickTime, tickTime + 1000));
}

SceneGraph::StaticGeometry *Shields::GetFirstShieldMesh()
{
	for (ShieldIterator it = m_shields.begin(); it != m_shields.end(); ++it) {
		if (it->m_mesh) {
			return it->m_mesh.Get();
		}
	}

	return nullptr;
}
