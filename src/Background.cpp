// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Background.h"

#include "FileSystem.h"
#include "Game.h"
#include "GameLocator.h"
#include "IniConfig.h"
#include "Player.h"
#include "Space.h"
#include "galaxy/Galaxy.h"
#include "galaxy/GalaxyEnums.h"
#include "galaxy/Sector.h"
#include "galaxy/StarSystem.h"
#include "graphics/Drawables.h"
#include "graphics/Material.h"
#include "graphics/Renderer.h"
#include "graphics/RendererLocator.h"
#include "graphics/RenderState.h"
#include "graphics/Texture.h"
#include "graphics/TextureBuilder.h"
#include "graphics/VertexArray.h"
#include "graphics/VertexBuffer.h"
#include "libs/StringF.h"
#include "libs/stringUtils.h"
#include "perlin.h"

#include <cstdint>
#include <iostream>
#include <sstream>

using namespace Graphics;

namespace {
	static const uint32_t BG_STAR_MAX = 500000;
	static const uint32_t BG_STAR_MIN = 50000;
	static RefCountedPtr<Graphics::Texture> s_defaultCubeMap;

	static uint32_t GetNumSkyboxes()
	{
		char filename[1024];
		snprintf(filename, sizeof(filename), "textures/skybox");
		std::vector<FileSystem::FileInfo> fileList;
		FileSystem::gameDataFiles.ReadDirectory(filename, fileList);

		const char *itemMask = "ub";

		uint32_t num_matching = 0;
		for (std::vector<FileSystem::FileInfo>::const_iterator it = fileList.begin(), itEnd = fileList.end(); it != itEnd; ++it) {
			if (stringUtils::starts_with((*it).GetName(), itemMask)) {
				++num_matching;
			}
		}
		return num_matching;
	}
} // namespace

namespace Background {

#pragma pack(push, 4)
	struct MilkyWayVert {
		vector3f pos;
		Color4ub col;
	};

	struct StarVert {
		vector3f pos;
		Color4ub col;
	};

	struct SkyboxVert {
		vector3f pos;
		vector2f uv;
	};
#pragma pack(pop)

	BackgroundElement::BackgroundElement() :
		m_rMin(0.0),
		m_rMax(0.0),
		m_gMin(0.0),
		m_gMax(0.0),
		m_bMin(0.0),
		m_bMax(0.0)
	{}

	BackgroundElement::~BackgroundElement()
	{}

	void BackgroundElement::SetIntensity(float intensity)
	{
		m_material->emissive = Color(intensity * 255, intensity * 255, intensity * 255);
	}

	UniverseBox::UniverseBox()
	{
		Init();
	}

	UniverseBox::~UniverseBox()
	{
	}

	void UniverseBox::Init()
	{
		// Load default cubemap
		if (!s_defaultCubeMap.Valid()) {
			TextureBuilder texture_builder = TextureBuilder::Cube("textures/skybox/default.dds");
			s_defaultCubeMap.Reset(texture_builder.GetOrCreateTexture(RendererLocator::getRenderer(), std::string("cube")));
		}

		// Create skybox geometry
		std::unique_ptr<Graphics::VertexArray> box(new VertexArray(ATTRIB_POSITION | ATTRIB_UV0, 36));
		const float vp = 1000.0f;
		// Top +Y
		box->Add(vector3f(-vp, vp, vp), vector2f(0.0f, 0.0f));
		box->Add(vector3f(-vp, vp, -vp), vector2f(0.0f, 1.0f));
		box->Add(vector3f(vp, vp, vp), vector2f(1.0f, 0.0f));
		box->Add(vector3f(vp, vp, vp), vector2f(1.0f, 0.0f));
		box->Add(vector3f(-vp, vp, -vp), vector2f(0.0f, 1.0f));
		box->Add(vector3f(vp, vp, -vp), vector2f(1.0f, 1.0f));
		// Bottom -Y
		box->Add(vector3f(-vp, -vp, -vp), vector2f(0.0f, 0.0f));
		box->Add(vector3f(-vp, -vp, vp), vector2f(0.0f, 1.0f));
		box->Add(vector3f(vp, -vp, -vp), vector2f(1.0f, 0.0f));
		box->Add(vector3f(vp, -vp, -vp), vector2f(1.0f, 0.0f));
		box->Add(vector3f(-vp, -vp, vp), vector2f(0.0f, 1.0f));
		box->Add(vector3f(vp, -vp, vp), vector2f(1.0f, 1.0f));
		// Front -Z
		box->Add(vector3f(-vp, vp, -vp), vector2f(0.0f, 0.0f));
		box->Add(vector3f(-vp, -vp, -vp), vector2f(0.0f, 1.0f));
		box->Add(vector3f(vp, vp, -vp), vector2f(1.0f, 0.0f));
		box->Add(vector3f(vp, vp, -vp), vector2f(1.0f, 0.0f));
		box->Add(vector3f(-vp, -vp, -vp), vector2f(0.0f, 1.0f));
		box->Add(vector3f(vp, -vp, -vp), vector2f(1.0f, 1.0f));
		// Back +Z
		box->Add(vector3f(vp, vp, vp), vector2f(0.0f, 0.0f));
		box->Add(vector3f(vp, -vp, vp), vector2f(0.0f, 1.0f));
		box->Add(vector3f(-vp, vp, vp), vector2f(1.0f, 0.0f));
		box->Add(vector3f(-vp, vp, vp), vector2f(1.0f, 0.0f));
		box->Add(vector3f(vp, -vp, vp), vector2f(0.0f, 1.0f));
		box->Add(vector3f(-vp, -vp, vp), vector2f(1.0f, 1.0f));
		// Right +X
		box->Add(vector3f(vp, vp, -vp), vector2f(0.0f, 0.0f));
		box->Add(vector3f(vp, -vp, -vp), vector2f(0.0f, 1.0f));
		box->Add(vector3f(vp, vp, vp), vector2f(1.0f, 0.0f));
		box->Add(vector3f(vp, vp, vp), vector2f(1.0f, 0.0f));
		box->Add(vector3f(vp, -vp, -vp), vector2f(0.0f, 1.0f));
		box->Add(vector3f(vp, -vp, vp), vector2f(1.0f, 1.0f));
		// Left -X
		box->Add(vector3f(-vp, vp, vp), vector2f(0.0f, 0.0f));
		box->Add(vector3f(-vp, -vp, vp), vector2f(0.0f, 1.0f));
		box->Add(vector3f(-vp, vp, -vp), vector2f(1.0f, 0.0f));
		box->Add(vector3f(-vp, vp, -vp), vector2f(1.0f, 0.0f));
		box->Add(vector3f(-vp, -vp, vp), vector2f(0.0f, 1.0f));
		box->Add(vector3f(-vp, -vp, -vp), vector2f(1.0f, 1.0f));

		Graphics::MaterialDescriptor desc;
		desc.effect = EffectType::SKYBOX;
		m_material.Reset(RendererLocator::getRenderer()->CreateMaterial(desc));
		m_material->texture0 = nullptr;

		//create buffer and upload data
		Graphics::VertexBufferDesc vbd;
		vbd.attrib[0].semantic = Graphics::ATTRIB_POSITION;
		vbd.attrib[0].format = Graphics::ATTRIB_FORMAT_FLOAT3;
		vbd.attrib[1].semantic = Graphics::ATTRIB_UV0;
		vbd.attrib[1].format = Graphics::ATTRIB_FORMAT_FLOAT2;
		vbd.numVertices = box->GetNumVerts();
		vbd.usage = Graphics::BUFFER_USAGE_STATIC;

		m_vertexBuffer.reset(RendererLocator::getRenderer()->CreateVertexBuffer(vbd));

		SkyboxVert *vtxPtr = m_vertexBuffer->Map<SkyboxVert>(Graphics::BUFFER_MAP_WRITE);
		assert(m_vertexBuffer->GetDesc().stride == sizeof(SkyboxVert));
		for (uint32_t i = 0; i < box->GetNumVerts(); i++) {
			vtxPtr[i].pos = box->position[i];
			vtxPtr[i].uv = box->uv0[i];
		}
		m_vertexBuffer->Unmap();

		SetIntensity(1.0f);

		m_numCubemaps = GetNumSkyboxes();
	}

	void UniverseBox::Draw(Graphics::RenderState *rs)
	{
		if (m_material->texture0)
			RendererLocator::getRenderer()->DrawBuffer(m_vertexBuffer.get(), rs, m_material.Get());
	}

	void UniverseBox::LoadCubeMap(Random &rand)
	{
		if (m_numCubemaps > 0) {
			const int new_ubox_index = rand.Int32(1, m_numCubemaps);
			if (new_ubox_index > 0) {
				// Load new one
				const std::string os = stringf("textures/skybox/ub%0{d}.dds", (new_ubox_index - 1));
				TextureBuilder texture_builder = TextureBuilder::Cube(os.c_str());
				m_cubemap.Reset(texture_builder.GetOrCreateTexture(RendererLocator::getRenderer(), std::string("cube")));
				m_material->texture0 = m_cubemap.Get();
			}
		} else {
			// use default cubemap
			m_cubemap.Reset();
			m_material->texture0 = s_defaultCubeMap.Get();
		}
	}

	Starfield::Starfield(Random &rand, float amount)
	{
		Init();
		Fill(rand, amount);
	}

	Starfield::~Starfield()
	{}

	void Starfield::Init()
	{
		Graphics::MaterialDescriptor desc;
		desc.effect = EffectType::STARFIELD;
		desc.textures = 1;
		desc.vertexColors = true;
		m_material.Reset(RendererLocator::getRenderer()->CreateMaterial(desc));
		m_material->emissive = Color::WHITE;
		m_material->texture0 = Graphics::TextureBuilder::Billboard("textures/star_point.png").GetOrCreateTexture(RendererLocator::getRenderer(), "billboard");

		Graphics::MaterialDescriptor descStreaks;
		descStreaks.effect = EffectType::VTXCOLOR;
		descStreaks.vertexColors = true;
		m_materialStreaks.Reset(RendererLocator::getRenderer()->CreateMaterial(descStreaks));
		m_materialStreaks->emissive = Color::WHITE;

		IniConfig cfg;
		cfg.Read(FileSystem::gameDataFiles, "configs/Starfield.ini");
		// NB: limit the ranges of all values loaded from the file
		m_rMin = Clamp(cfg.Float("rMin", 0.2), 0.2f, 1.0f);
		m_rMax = Clamp(cfg.Float("rMax", 0.9), 0.2f, 1.0f);
		m_gMin = Clamp(cfg.Float("gMin", 0.2), 0.2f, 1.0f);
		m_gMax = Clamp(cfg.Float("gMax", 0.9), 0.2f, 1.0f);
		m_bMin = Clamp(cfg.Float("bMin", 0.2), 0.2f, 1.0f);
		m_bMax = Clamp(cfg.Float("bMax", 0.9), 0.2f, 1.0f);
	}

	void Starfield::Fill(Random &rand, float amountOfBackgroundStars)
	{
		const uint32_t NUM_BG_STARS = Clamp(uint32_t(amountOfBackgroundStars * BG_STAR_MAX), BG_STAR_MIN, BG_STAR_MAX);
		m_hyperVtx.reset(new vector3f[BG_STAR_MAX * 3]);
		m_hyperCol.reset(new Color[BG_STAR_MAX * 3]);

		// setup the animated stars buffer (streaks in Hyperspace)
		{
			Graphics::VertexBufferDesc vbd;
			vbd.attrib[0].semantic = Graphics::ATTRIB_POSITION;
			vbd.attrib[0].format = Graphics::ATTRIB_FORMAT_FLOAT3;
			vbd.attrib[1].semantic = Graphics::ATTRIB_DIFFUSE;
			vbd.attrib[1].format = Graphics::ATTRIB_FORMAT_UBYTE4;
			vbd.usage = Graphics::BUFFER_USAGE_DYNAMIC;
			vbd.numVertices = NUM_BG_STARS * 2;
			m_animBuffer.reset(RendererLocator::getRenderer()->CreateVertexBuffer(vbd));
		}

		m_pointSprites.reset(new Graphics::Drawables::PointSprites);

		assert(sizeof(StarVert) == 16);
		std::unique_ptr<vector3f[]> stars(new vector3f[NUM_BG_STARS]);
		std::unique_ptr<Color[]> colors(new Color[NUM_BG_STARS]);
		std::unique_ptr<float[]> sizes(new float[NUM_BG_STARS]);
		//fill the array
		uint32_t num = 0;
		if (GameLocator::getGame() != nullptr && GameLocator::getGame()->GetSpace() != nullptr && GameLocator::getGame()->GetSpace()->GetStarSystem() != nullptr) {
			const SystemPath current = GameLocator::getGame()->GetSpace()->GetStarSystem()->GetPath();

			const double size = 1.0;
			const int32_t visibleRadius = 100; // lyrs
			const int32_t visibleRadiusSqr = (visibleRadius * visibleRadius);
			const int32_t sectorMin = -(visibleRadius / Sector::SIZE); // lyrs_radius / sector_size_in_lyrs
			const int32_t sectorMax = visibleRadius / Sector::SIZE; // lyrs_radius / sector_size_in_lyrs
			for (int32_t x = sectorMin; x < sectorMax; x++) {
				for (int32_t y = sectorMin; y < sectorMax; y++) {
					for (int32_t z = sectorMin; z < sectorMax; z++) {
						SystemPath sys(x, y, z);
						if (SystemPath::SectorDistanceSqr(sys, current) * Sector::SIZE >= visibleRadiusSqr)
							continue; // early out

						// this is fairly expensive
						RefCountedPtr<const Sector> sec = GameLocator::getGame()->GetGalaxy()->GetSector(sys);

						// add as many systems as we can
						const size_t numSystems = std::min(sec->m_systems.size(), (size_t)(NUM_BG_STARS - num));
						for (size_t systemIndex = 0; systemIndex < numSystems; systemIndex++) {
							const Sector::System *ss = &(sec->m_systems[systemIndex]);
							const vector3f distance = Sector::SIZE * vector3f(current.sectorX, current.sectorY, current.sectorZ) - ss->GetFullPosition();
							if (distance.Length() >= visibleRadius)
								continue; // too far

							// grab the approximate real colour
							Color col = GalaxyEnums::starRealColors[ss->GetStarType(0)];
							col.r = Clamp(col.r, uint8_t(m_rMin * 255), uint8_t(m_rMax * 255));
							col.g = Clamp(col.g, uint8_t(m_gMin * 255), uint8_t(m_gMax * 255));
							col.b = Clamp(col.b, uint8_t(m_bMin * 255), uint8_t(m_bMax * 255));
							//const Color col(Color::PINK); // debug pink

							// copy the data
							sizes[num] = size;
							stars[num] = distance.Normalized() * 1000.0f;
							colors[num] = col;

							//need to keep data around for HS anim - this is stupid
							m_hyperVtx[NUM_BG_STARS * 2 + num] = stars[num];
							m_hyperCol[NUM_BG_STARS * 2 + num] = col * 0.8f;
							num++;
						}
						if (num >= NUM_BG_STARS) {
							x = sectorMax;
							y = sectorMax;
							z = sectorMax;
							break;
						}
					}
				}
			}
		}
		Output("Stars picked from galaxy: %d\n", num);

		// fill out the remaining target count with generated points
		if (num < NUM_BG_STARS) {
			for (uint32_t i = num; i < NUM_BG_STARS; i++) {
				const double size = rand.Double(0.2, 0.9);
				const uint8_t colScale = size * 255;

				const Color col(
					rand.Double(m_rMin, m_rMax) * colScale,
					rand.Double(m_gMin, m_gMax) * colScale,
					rand.Double(m_bMin, m_bMax) * colScale,
					255);

				// this is proper random distribution on a sphere's surface
				const float theta = float(rand.Double(0.0, 2.0 * M_PI));
				const float u = float(rand.Double(-1.0, 1.0));

				sizes[i] = size;
				// squeeze the starfield a bit to get more density near horizon using matrix3x3f::Scale
				stars[i] = matrix3x3f::Scale(1.0, 0.4, 1.0) * (vector3f(sqrt(1.0f - u * u) * cos(theta), u, sqrt(1.0f - u * u) * sin(theta)).Normalized() * 1000.0f);
				colors[i] = col;

				//need to keep data around for HS anim - this is stupid
				m_hyperVtx[NUM_BG_STARS * 2 + i] = stars[i];
				m_hyperCol[NUM_BG_STARS * 2 + i] = col;

				num++;
			}
		}
		Output("Final stars number: %d\n", num);

		m_pointSprites->SetData(NUM_BG_STARS, stars.get(), colors.get(), sizes.get(), m_material.Get());

		Graphics::RenderStateDesc rsd;
		rsd.depthTest = false;
		rsd.depthWrite = false;
		rsd.blendMode = Graphics::BLEND_ALPHA;
		m_renderState = RendererLocator::getRenderer()->CreateRenderState(rsd);
	}

	void Starfield::Draw(Graphics::RenderState *rs)
	{
		// XXX would be nice to get rid of the Pi:: stuff here
		if (!GameLocator::getGame() || GameLocator::getGame()->GetPlayer()->GetFlightState() != Ship::HYPERSPACE) {
			m_pointSprites->Draw(RendererLocator::getRenderer(), m_renderState);
		} else {
			assert(sizeof(StarVert) == 16);
			assert(m_animBuffer->GetDesc().stride == sizeof(StarVert));
			auto vtxPtr = m_animBuffer->Map<StarVert>(Graphics::BUFFER_MAP_WRITE);

			// roughly, the multiplier gets smaller as the duration gets larger.
			// the time-looking bits in this are completely arbitrary - I figured
			// it out by tweaking the numbers until it looked sort of right
			const double mult = 0.0015 / (GameLocator::getGame()->GetPlayer()->GetHyperspaceDuration() / (60.0 * 60.0 * 24.0 * 7.0));

			const double hyperspaceProgress = GameLocator::getGame()->GetHyperspaceProgress();

			const int32_t NUM_STARS = m_animBuffer->GetDesc().numVertices / 2;

			const vector3d pz = GameLocator::getGame()->GetPlayer()->GetOrient().VectorZ(); //back vector
			for (int i = 0; i < NUM_STARS; i++) {
				vector3f v = m_hyperVtx[NUM_STARS * 2 + i] + vector3f(pz * hyperspaceProgress * mult);
				const Color &c = m_hyperCol[NUM_STARS * 2 + i];

				vtxPtr[i * 2].pos = m_hyperVtx[i * 2] = m_hyperVtx[NUM_STARS * 2 + i] + v;
				vtxPtr[i * 2].col = m_hyperCol[i * 2] = c;

				vtxPtr[i * 2 + 1].pos = m_hyperVtx[i * 2 + 1] = v;
				vtxPtr[i * 2 + 1].col = m_hyperCol[i * 2 + 1] = c;
			}
			m_animBuffer->Unmap();
			RendererLocator::getRenderer()->DrawBuffer(m_animBuffer.get(), rs, m_materialStreaks.Get(), Graphics::LINE_SINGLE);
		}
	}

	MilkyWay::MilkyWay()
	{
		//build milky way model in two strips (about 256 verts)
		std::unique_ptr<Graphics::VertexArray> bottom(new VertexArray(ATTRIB_POSITION | ATTRIB_DIFFUSE));
		std::unique_ptr<Graphics::VertexArray> top(new VertexArray(ATTRIB_POSITION | ATTRIB_DIFFUSE));

		const Color dark(Color::BLANK);
		const Color bright(13, 13, 13, 13);

		//bottom
		float theta;
		for (theta = 0.0; theta < 2.f * float(M_PI); theta += 0.1f) {
			bottom->Add(
				vector3f(100.0f * sin(theta), float(-40.0 - 30.0 * noise(vector3d(sin(theta), 1.0, cos(theta)))), 100.0f * cos(theta)),
				dark);
			bottom->Add(
				vector3f(100.0f * sin(theta), float(5.0 * noise(vector3d(sin(theta), 0.0, cos(theta)))), 100.0f * cos(theta)),
				bright);
		}
		theta = 2.f * float(M_PI);
		bottom->Add(
			vector3f(100.0f * sin(theta), float(-40.0 - 30.0 * noise(vector3d(sin(theta), 1.0, cos(theta)))), 100.0f * cos(theta)),
			dark);
		bottom->Add(
			vector3f(100.0f * sin(theta), float(5.0 * noise(vector3d(sin(theta), 0.0, cos(theta)))), 100.0f * cos(theta)),
			bright);
		//top
		for (theta = 0; theta < 2.f * float(M_PI); theta += 0.1f) {
			top->Add(
				vector3f(100.0f * sin(theta), float(5.0 * noise(vector3d(sin(theta), 0.0, cos(theta)))), 100.0f * cos(theta)),
				bright);
			top->Add(
				vector3f(100.0f * sin(theta), float(40.0 + 30.0 * noise(vector3d(sin(theta), -1.0, cos(theta)))), 100.0f * cos(theta)),
				dark);
		}
		theta = 2.f * float(M_PI);
		top->Add(
			vector3f(100.0f * sin(theta), float(5.0 * noise(vector3d(sin(theta), 0.0, cos(theta)))), 100.0f * cos(theta)),
			bright);
		top->Add(
			vector3f(100.0f * sin(theta), float(40.0 + 30.0 * noise(vector3d(sin(theta), -1.0, cos(theta)))), 100.0f * cos(theta)),
			dark);

		Graphics::MaterialDescriptor desc;
		desc.effect = EffectType::STARFIELD;
		desc.vertexColors = true;
		m_material.Reset(RendererLocator::getRenderer()->CreateMaterial(desc));
		m_material->emissive = Color::WHITE;

		Graphics::VertexBufferDesc vbd;
		vbd.attrib[0].semantic = Graphics::ATTRIB_POSITION;
		vbd.attrib[0].format = Graphics::ATTRIB_FORMAT_FLOAT3;
		vbd.attrib[1].semantic = Graphics::ATTRIB_DIFFUSE;
		vbd.attrib[1].format = Graphics::ATTRIB_FORMAT_UBYTE4;
		vbd.numVertices = bottom->GetNumVerts() + top->GetNumVerts();
		vbd.usage = Graphics::BUFFER_USAGE_STATIC;

		//two strips in one buffer, but seems to work ok without degenerate triangles
		m_vertexBuffer.reset(RendererLocator::getRenderer()->CreateVertexBuffer(vbd));
		assert(m_vertexBuffer->GetDesc().stride == sizeof(MilkyWayVert));
		auto vtxPtr = m_vertexBuffer->Map<MilkyWayVert>(Graphics::BUFFER_MAP_WRITE);
		for (uint32_t i = 0; i < top->GetNumVerts(); i++) {
			vtxPtr->pos = top->position[i];
			vtxPtr->col = top->diffuse[i];
			vtxPtr++;
		}
		for (uint32_t i = 0; i < bottom->GetNumVerts(); i++) {
			vtxPtr->pos = bottom->position[i];
			vtxPtr->col = bottom->diffuse[i];
			vtxPtr++;
		}
		m_vertexBuffer->Unmap();
	}

	void MilkyWay::Draw(Graphics::RenderState *rs)
	{
		assert(m_vertexBuffer);
		assert(m_material);
		RendererLocator::getRenderer()->DrawBuffer(m_vertexBuffer.get(), rs, m_material.Get(), Graphics::TRIANGLE_STRIP);
	}

	Container::Container(Random &rand, float amountOfBackgroundStars) :
		m_milkyWay(),
		m_starField(rand, amountOfBackgroundStars),
		m_universeBox(),
		m_drawFlags(DRAW_SKYBOX | DRAW_STARS)
	{
		Graphics::RenderStateDesc rsd;
		rsd.depthTest = false;
		rsd.depthWrite = false;
		m_renderState = RendererLocator::getRenderer()->CreateRenderState(rsd);
	}

	Container::~Container()
	{}

	void Container::Refresh(Random &rand, float amountOfBackgroundStars)
	{
		// always redo starfield, milkyway stays normal for now
		m_starField.Fill(rand, amountOfBackgroundStars);
		m_universeBox.LoadCubeMap(rand);
	}

	void Container::Draw(const matrix4x4d &transform)
	{
		PROFILE_SCOPED()
		RendererLocator::getRenderer()->SetTransform(transform);
		if (DRAW_SKYBOX & m_drawFlags) {
			m_universeBox.Draw(m_renderState);
		}
		if (DRAW_MILKY & m_drawFlags) {
			m_milkyWay.Draw(m_renderState);
		}
		if (DRAW_STARS & m_drawFlags) {
			RendererLocator::getRenderer()->SetTransform(transform);
			m_starField.Draw(m_renderState);
		}
	}

	void Container::SetIntensity(float intensity)
	{
		PROFILE_SCOPED()
		intensity = Clamp(intensity, 0.0f, 1.0f);
		m_universeBox.SetIntensity(intensity);
		m_starField.SetIntensity(intensity);
		m_milkyWay.SetIntensity(intensity);
	}

	void Container::SetDrawFlags(const uint32_t flags)
	{
		m_drawFlags = flags;
	}

} //namespace Background
