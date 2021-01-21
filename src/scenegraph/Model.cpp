// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Model.h"

#include "Animation.h"
#include "CollisionVisitor.h"
#include "CollMesh.h"
#include "FindNodeVisitor.h"
#include "Label3D.h"
#include "MatrixTransform.h"
#include "ModelDebug.h"
#include "NodeCopyCache.h"
#include "Thruster.h"
#include "GameSaveError.h"
#include "JsonUtils.h"
#include "collider/CSGDefinitions.h"
#include "graphics/Material.h"
#include "graphics/Renderer.h"
#include "graphics/RendererLocator.h"
#include "graphics/TextureBuilder.h"
#include "libs/StringF.h"
#include "libs/stringUtils.h"
#include "libs/utils.h"

namespace SceneGraph {

	class LabelUpdateVisitor : public NodeVisitor {
	public:
		virtual void ApplyLabel(Label3D &l)
		{
			l.SetText(label);
		}

		std::string label;
	};

	Model::Model(const std::string &name) :
		m_boundingRadius(10.f),
		m_name(name),
		m_curPatternIndex(0),
		m_curPattern(0),
		m_debugFlags(DebugFlags::NONE)
	{
		m_root.Reset(new Group());
		m_root->SetName(name);
		ClearDecals();
	}

	Model::Model(const Model &other) :
		m_boundingRadius(other.m_boundingRadius),
		m_materials(other.m_materials),
		m_patterns(other.m_patterns),
		m_collMesh(other.m_collMesh) //might have to make this per-instance at some point
		,
		m_name(other.m_name),
		m_curPatternIndex(other.m_curPatternIndex),
		m_curPattern(other.m_curPattern),
		m_debugFlags(DebugFlags::NONE),
		m_Boxes(other.m_Boxes),
		m_mounts(other.m_mounts)
	{
		//selective copying of node structure
		NodeCopyCache cache;
		m_root.Reset(dynamic_cast<Group *>(other.m_root->Clone(&cache)));

		//materials are shared by meshes
		for (unsigned int i = 0; i < MAX_DECAL_MATERIALS; i++)
			m_decalMaterials[i] = other.m_decalMaterials[i];
		ClearDecals();

		//create unique color texture, if used
		//patterns are shared
		if (SupportsPatterns()) {
			std::vector<Color> colors;
			colors.reserve(3);
			colors.insert(end(colors), {Color::RED, Color::GREEN, Color::BLUE});
			SetColors(colors);
			SetPattern(0);
		}

		//animations need to be copied and retargeted
		m_animations.reserve(other.m_animations.size());
		m_animations = other.m_animations;
		std::for_each(begin(m_animations), end(m_animations), [this](Animation &anim) {
			anim.UpdateChannelTargets(m_root.Get());
		});

		//m_tags needs to be updated
		for (TagContainer::const_iterator it = other.m_tags.begin(); it != other.m_tags.end(); ++it) {
			MatrixTransform *t = dynamic_cast<MatrixTransform *>(m_root->FindNode((*it)->GetName()));
			assert(t != 0);
			m_tags.push_back(t);
		}
	}

	Model::~Model()
	{
	}

	std::unique_ptr<Model> Model::MakeInstance() const
	{
		std::unique_ptr<Model> m(new Model(*this));
		return m;
	}

	void Model::Render(const matrix4x4f &trans, const RenderData *rd)
	{
		PROFILE_SCOPED()
		//update color parameters (materials are shared by model instances)
		if (m_curPattern) {
			for (MaterialContainer::const_iterator it = m_materials.begin(); it != m_materials.end(); ++it) {
				if ((*it).second->GetDescriptor().usePatterns) {
					(*it).second->texture5 = m_colorMap.GetTexture();
					(*it).second->texture4 = m_curPattern;
				}
			}
		}

		//update decals (materials and geometries are shared)
		for (unsigned int i = 0; i < MAX_DECAL_MATERIALS; i++)
			if (m_decalMaterials[i])
				m_decalMaterials[i]->texture0 = m_curDecals[i];

		//Override renderdata if this model is called from ModelNode
		RenderData params = (rd != 0) ? (*rd) : m_renderData;

		RendererLocator::getRenderer()->SetTransform(trans);

		//using the entire model bounding radius for all nodes at the moment.
		//BR could also be a property of Node.
		params.boundingRadius = GetDrawClipRadius();

		//render in two passes, if this is the top-level model
		if (to_bool(m_debugFlags & DebugFlags::WIREFRAME))
			RendererLocator::getRenderer()->SetWireFrameMode(true);

		if (params.nodemask & MASK_IGNORE) {
			m_root->Render(trans, &params);
		} else {
			params.nodemask = NODE_SOLID;
			m_root->Render(trans, &params);
			params.nodemask = NODE_TRANSPARENT;
			m_root->Render(trans, &params);
		}

		if (to_bool(m_debugFlags & DebugFlags::WIREFRAME))
			RendererLocator::getRenderer()->SetWireFrameMode(false);

		if (!to_bool(m_debugFlags))
			return;

		m_modelDebug->Render(trans);
	}

	void Model::Render(const std::vector<matrix4x4f> &trans, const RenderData *rd)
	{
		PROFILE_SCOPED();

		//update color parameters (materials are shared by model instances)
		if (m_curPattern) {
			for (MaterialContainer::const_iterator it = m_materials.begin(); it != m_materials.end(); ++it) {
				if ((*it).second->GetDescriptor().usePatterns) {
					(*it).second->texture5 = m_colorMap.GetTexture();
					(*it).second->texture4 = m_curPattern;
				}
			}
		}

		//update decals (materials and geometries are shared)
		for (unsigned int i = 0; i < MAX_DECAL_MATERIALS; i++)
			if (m_decalMaterials[i])
				m_decalMaterials[i]->texture0 = m_curDecals[i];

		//Override renderdata if this model is called from ModelNode
		RenderData params = (rd != 0) ? (*rd) : m_renderData;

		//using the entire model bounding radius for all nodes at the moment.
		//BR could also be a property of Node.
		params.boundingRadius = GetDrawClipRadius();

		//render in two passes, if this is the top-level model
		if (to_bool(m_debugFlags & DebugFlags::WIREFRAME))
			RendererLocator::getRenderer()->SetWireFrameMode(true);

		if (params.nodemask & MASK_IGNORE) {
			m_root->Render(trans, &params);
		} else {
			params.nodemask = NODE_SOLID;
			m_root->Render(trans, &params);
			params.nodemask = NODE_TRANSPARENT;
			m_root->Render(trans, &params);
		}

		if (to_bool(m_debugFlags & DebugFlags::WIREFRAME))
			RendererLocator::getRenderer()->SetWireFrameMode(false);
	}

	RefCountedPtr<CollMesh> Model::CreateCollisionMesh()
	{
		CollisionVisitor cv;
		m_root->Accept(cv);
		m_collMesh = cv.CreateCollisionMesh();
		m_boundingRadius = cv.GetBoundingRadius();
		return m_collMesh;
	}

	RefCountedPtr<CollMesh> Model::GetCollisionMesh() const
	{
		return m_collMesh;
	}

	void Model::SetCollisionMesh(RefCountedPtr<CollMesh> collMesh)
	{
		m_collMesh.Reset(collMesh.Get());
	}

	RefCountedPtr<Group> Model::GetRoot()
	{
		return m_root;
	}

	RefCountedPtr<Graphics::Material> Model::GetMaterialByName(const std::string &name) const
	{
		for (auto it : m_materials) {
			if (it.first == name)
				return it.second;
		}
		return RefCountedPtr<Graphics::Material>(); //return invalid
	}

	RefCountedPtr<Graphics::Material> Model::GetMaterialByIndex(const int i) const
	{
		return m_materials.at(std::clamp(i, 0, int(m_materials.size()) - 1)).second;
	}

	const MatrixTransform *Model::GetTagByIndex(const unsigned int i) const
	{
		if (m_tags.empty() || i > m_tags.size() - 1) return 0;
		return m_tags.at(i);
	}

	const MatrixTransform *Model::FindTagByName(const std::string &name) const
	{
		for (TagContainer::const_iterator it = m_tags.begin();
			 it != m_tags.end();
			 ++it) {
			assert(!(*it)->GetName().empty()); //tags must have a name
			if ((*it)->GetName() == name) return (*it);
		}
		return 0;
	}

	void Model::FindTagsByStartOfName(const std::string &name, TVecMT &outNameMTs) const
	{
		for (TagContainer::const_iterator it = m_tags.begin();
			 it != m_tags.end();
			 ++it) {
			assert(!(*it)->GetName().empty()); //tags must have a name
			if (stringUtils::starts_with((*it)->GetName(), name)) {
				outNameMTs.push_back((*it));
			}
		}
		return;
	}

	void Model::AddTag(const std::string &name, MatrixTransform *node)
	{
		if (FindTagByName(name)) return;
		node->SetName(name);
		node->SetNodeFlags(node->GetNodeFlags() | NODE_TAG);
		m_root->AddChild(node);
		m_tags.push_back(node);
	}

	void Model::SetPattern(unsigned int index)
	{
		if (m_patterns.empty() || index > m_patterns.size() - 1) return;
		const Pattern &pat = m_patterns.at(index);
		m_colorMap.SetSmooth(pat.smoothColor);
		m_curPatternIndex = index;
		m_curPattern = pat.texture.Get();
	}

	void Model::SetColors(const std::vector<Color> &colors)
	{
		assert(colors.size() == 3); //primary, seconday, trim
		m_colorMap.Generate(RendererLocator::getRenderer(), colors.at(0), colors.at(1), colors.at(2));
	}

	void Model::SetDecalTexture(Graphics::Texture *t, unsigned int index)
	{
		index = std::min(index, MAX_DECAL_MATERIALS - 1);
		if (m_decalMaterials[index])
			m_curDecals[index] = t;
	}

	void Model::SetLabel(const std::string &text)
	{
		LabelUpdateVisitor vis;
		vis.label = text;
		m_root->Accept(vis);
	}

	std::vector<Mount> Model::GetGunTags() const {
		std::vector<Mount> mounts = m_mounts;
		return mounts;
	}

	void Model::ClearDecals()
	{
		Graphics::Texture *t = Graphics::TextureBuilder::GetTransparentTexture(RendererLocator::getRenderer());
		for (unsigned int i = 0; i < MAX_DECAL_MATERIALS; i++)
			m_curDecals[i] = t;
	}

	void Model::ClearDecal(unsigned int index)
	{
		index = std::min(index, MAX_DECAL_MATERIALS - 1);
		if (m_decalMaterials[index])
			m_curDecals[index] = Graphics::TextureBuilder::GetTransparentTexture(RendererLocator::getRenderer());
	}

	bool Model::SupportsDecals()
	{
		for (unsigned int i = 0; i < MAX_DECAL_MATERIALS; i++)
			if (m_decalMaterials[i].Valid()) return true;

		return false;
	}

	bool Model::SupportsPatterns()
	{
		for (MaterialContainer::const_iterator it = m_materials.begin(), itEnd = m_materials.end();
			 it != itEnd;
			 ++it) {
			//Set pattern only on a material that supports it
			if ((*it).second->GetDescriptor().usePatterns)
				return true;
		}

		return false;
	}

	Animation *Model::FindAnimation(const std::string &name)
	{
		for (Animation &anim : m_animations) {
			if (anim.GetName() == name) return &anim;
		}
		return nullptr;
	}

	void Model::UpdateAnimations()
	{
		// XXX WIP. Assuming animations are controlled manually by SetProgress.
		for (Animation &anim : m_animations)
			anim.Interpolate();
	}

	void Model::SetThrust(const vector3f &lin, const vector3f &ang)
	{
		m_renderData.linthrust[0] = lin.x;
		m_renderData.linthrust[1] = lin.y;
		m_renderData.linthrust[2] = lin.z;

		m_renderData.angthrust[0] = ang.x;
		m_renderData.angthrust[1] = ang.y;
		m_renderData.angthrust[2] = ang.z;
	}

	void Model::SetThrusterColor(const vector3f &dir, const Color &color)
	{
		assert(m_root != nullptr);

		FindNodeVisitor thrusterFinder(FindNodeVisitor::MATCH_NAME_FULL, "thrusters");
		m_root->Accept(thrusterFinder);
		const std::vector<Node *> &results = thrusterFinder.GetResults();
		Group *thrusters = static_cast<Group *>(results.at(0));

		for (unsigned int i = 0; i < thrusters->GetNumChildren(); i++) {
			MatrixTransform *mt = static_cast<MatrixTransform *>(thrusters->GetChildAt(i));
			Thruster *my_thruster = static_cast<Thruster *>(mt->GetChildAt(0));
			if (my_thruster == nullptr) continue;
			float dot = my_thruster->GetDirection().Dot(dir);
			if (dot > 0.99) my_thruster->SetColor(color);
		}
	}

	void Model::SetThrusterColor(const std::string &name, const Color &color)
	{
		assert(m_root != nullptr);

		FindNodeVisitor thrusterFinder(FindNodeVisitor::MATCH_NAME_FULL, name);
		m_root->Accept(thrusterFinder);
		const std::vector<Node *> &results = thrusterFinder.GetResults();

		//Hope there's only 1 result...
		Thruster *my_thruster = static_cast<Thruster *>(results.at(0));
		if (my_thruster != nullptr) my_thruster->SetColor(color);
	}

	void Model::SetThrusterColor(const Color &color)
	{
		assert(m_root != nullptr);

		FindNodeVisitor thrusterFinder(FindNodeVisitor::MATCH_NAME_FULL, "thrusters");
		m_root->Accept(thrusterFinder);
		const std::vector<Node *> &results = thrusterFinder.GetResults();
		Group *thrusters = static_cast<Group *>(results.at(0));

		for (unsigned int i = 0; i < thrusters->GetNumChildren(); i++) {
			MatrixTransform *mt = static_cast<MatrixTransform *>(thrusters->GetChildAt(i));
			Thruster *my_thruster = static_cast<Thruster *>(mt->GetChildAt(0));
			assert(my_thruster != nullptr);
			my_thruster->SetColor(color);
		}
	}

	class SaveVisitorJson : public NodeVisitor {
	public:
		SaveVisitorJson(Json &jsonObj) :
			m_jsonArray(jsonObj) {}

		void ApplyMatrixTransform(MatrixTransform &node)
		{
			const matrix4x4f &m = node.GetTransform();
			Json matrixTransformObj({}); // Create JSON object to contain matrix transform data.
			matrixTransformObj["m"] = m;
			m_jsonArray.push_back(matrixTransformObj); // Append matrix transform object to array.
		}

	private:
		Json &m_jsonArray;
	};

	void Model::SaveToJson(Json &jsonObj) const
	{
		Json modelObj({}); // Create JSON object to contain model data.

		Json visitorArray = Json::array(); // Create JSON array to contain visitor data.
		SaveVisitorJson sv(visitorArray);
		m_root->Accept(sv);
		modelObj["visitor"] = visitorArray; // Add visitor array to model object.

		Json animationArray = Json::array(); // Create JSON array to contain animation data.
		for (auto i : m_animations)
			animationArray.push_back(i.GetProgress());
		modelObj["animations"] = animationArray; // Add animation array to model object.

		modelObj["cur_pattern_index"] = m_curPatternIndex;

		jsonObj["model"] = modelObj; // Add model object to supplied object.
	}

	class LoadVisitorJson : public NodeVisitor {
	public:
		LoadVisitorJson(const Json &jsonObj) :
			m_jsonArray(jsonObj),
			m_arrayIndex(0) {}

		void ApplyMatrixTransform(MatrixTransform &node)
		{
			node.SetTransform(m_jsonArray[m_arrayIndex++]["m"]);
		}

	private:
		const Json &m_jsonArray;
		unsigned int m_arrayIndex;
	};

	void Model::LoadFromJson(const Json &jsonObj)
	{
		try {
			Json modelObj = jsonObj["model"];

			Json visitorArray = modelObj["visitor"].get<Json::array_t>();
			LoadVisitorJson lv(visitorArray);
			m_root->Accept(lv);

			Json animationArray = modelObj["animations"].get<Json::array_t>();
			assert(m_animations.size() == animationArray.size());
			unsigned int arrayIndex = 0;
			for (auto i : m_animations)
				i.SetProgress(animationArray[arrayIndex++]);
			UpdateAnimations();

			SetPattern(modelObj["cur_pattern_index"]);
		} catch (Json::type_error &) {
			Output("Loading error in '%s' in function '%s' \n", __FILE__, __func__);
			throw SavedGameCorruptException();
		}
	}

	std::string Model::GetNameForMaterial(Graphics::Material *mat) const
	{
		for (auto it : m_materials) {
			Graphics::Material *modelMat = it.second.Get();
			if (modelMat == mat) return it.first;
		}

		//check decal materials
		for (uint32_t i = 0; i < MAX_DECAL_MATERIALS; i++) {
			if (m_decalMaterials[i].Valid() && m_decalMaterials[i].Get() == mat)
				return stringf("decal_%0{u}", i + 1);
		}

		return "unknown";
	}

	void Model::SetDebugFlags(DebugFlags flags)
	{
		m_debugFlags = flags;
		if (to_bool(m_debugFlags & SceneGraph::DebugFlags::NONE)) {
			m_modelDebug.reset();
		} else {
			if (!m_modelDebug) {
				m_modelDebug.reset(new ModelDebug(this, m_debugFlags));
			} else {
				m_modelDebug->UpdateFlags(m_debugFlags);
			}
		}
	}

	void Model::SetCentralCylinder(std::unique_ptr<CSG_CentralCylinder> centralcylinder) {
		m_centralCylinder = std::move(centralcylinder);
	}

	void Model::AddBox(std::unique_ptr<CSG_Box> box) {
		m_Boxes.push_back(*box.get());
	}
} // namespace SceneGraph
