// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Model.h"

#include "Animation.h"
#include "CollisionVisitor.h"
#include "FindNodeVisitor.h"
#include "Label3D.h"
#include "MatrixTransform.h"
#include "NodeCopyCache.h"
#include "Thruster.h"
#include "../GameSaveError.h"
#include "../JsonUtils.h"
#include "../StringF.h"
#include "../collider/CSGDefinitions.h"
#include "../graphics/Drawables.h"
#include "../graphics/Material.h"
#include "../graphics/RenderState.h"
#include "../graphics/Renderer.h"
#include "../graphics/RendererLocator.h"
#include "../graphics/TextureBuilder.h"
#include "../graphics/VertexArray.h"
#include "../graphics/VertexBuffer.h"
#include "../utils.h"

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
		m_debugFlags(0)
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
		m_Boxes(other.m_Boxes),
		m_name(other.m_name),
		m_curPatternIndex(other.m_curPatternIndex),
		m_curPattern(other.m_curPattern),
		m_debugFlags(0),
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
		if (m_debugFlags & DEBUG_WIREFRAME)
			RendererLocator::getRenderer()->SetWireFrameMode(true);

		if (params.nodemask & MASK_IGNORE) {
			m_root->Render(trans, &params);
		} else {
			params.nodemask = NODE_SOLID;
			m_root->Render(trans, &params);
			params.nodemask = NODE_TRANSPARENT;
			m_root->Render(trans, &params);
		}

		if (!m_debugFlags)
			return;

		if (m_debugFlags & DEBUG_WIREFRAME)
			RendererLocator::getRenderer()->SetWireFrameMode(false);

		if (m_debugFlags & DEBUG_BBOX) {
			RendererLocator::getRenderer()->SetTransform(trans);
			DrawAabb();
		}

		if (m_debugFlags & DEBUG_COLLMESH) {
			RendererLocator::getRenderer()->SetTransform(trans);
			DrawCollisionMesh();
			DrawCentralCylinder();
			DrawBoxes();
		}

		if (m_debugFlags & DEBUG_TAGS) {
			RendererLocator::getRenderer()->SetTransform(trans);
			DrawAxisIndicators(m_tagPoints);
		}

		if (m_debugFlags & DEBUG_DOCKING) {
			RendererLocator::getRenderer()->SetTransform(trans);
			DrawAxisIndicators(m_dockingPoints);
		}
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
		if (m_debugFlags & DEBUG_WIREFRAME)
			RendererLocator::getRenderer()->SetWireFrameMode(true);

		if (params.nodemask & MASK_IGNORE) {
			m_root->Render(trans, &params);
		} else {
			params.nodemask = NODE_SOLID;
			m_root->Render(trans, &params);
			params.nodemask = NODE_TRANSPARENT;
			m_root->Render(trans, &params);
		}
	}

	void Model::CreateAabbVB()
	{
		PROFILE_SCOPED()
		if (!m_collMesh) return;

		const Aabb aabb = m_collMesh->GetAabb();

		Graphics::MaterialDescriptor desc;
		m_aabbMat.Reset(RendererLocator::getRenderer()->CreateMaterial(desc));
		m_aabbMat->diffuse = Color::GREEN;

		m_state = RendererLocator::getRenderer()->CreateRenderState(Graphics::RenderStateDesc());

		m_aabbBox3D.reset(new Graphics::Drawables::Box3D(RendererLocator::getRenderer(), m_aabbMat, m_state, vector3f(aabb.min), vector3f(aabb.max)));
	}

	void Model::DrawAabb()
	{
		if (!m_collMesh) return;

		RendererLocator::getRenderer()->SetWireFrameMode(true);
		m_aabbBox3D->Draw(RendererLocator::getRenderer());
		RendererLocator::getRenderer()->SetWireFrameMode(false);
	}

	// Draw collision mesh as a wireframe overlay
	void Model::DrawCollisionMesh()
	{
		PROFILE_SCOPED()
		if (!m_collMesh) return;

		if (!m_collisionMeshVB.Valid()) {
			const std::vector<vector3f> &vertices = m_collMesh->GetGeomTreeVertices();
			const uint32_t *indices = m_collMesh->GetGeomTreeIndices();
			const unsigned int *triFlags = m_collMesh->GetGeomTreeTriFlags();
			const unsigned int numIndices = m_collMesh->GetGeomTreeNumTris() * 3;

			Graphics::VertexArray va(Graphics::ATTRIB_POSITION | Graphics::ATTRIB_DIFFUSE, numIndices * 3);
			int trindex = -1;
			for (unsigned int i = 0; i < numIndices; i++) {
				if (i % 3 == 0)
					trindex++;
				const unsigned int flag = triFlags[trindex];
				//show special geomflags in red
				va.Add(vertices[indices[i]], flag > 0 ? Color::RED : Color::WHITE);
			}

			//create buffer and upload data
			Graphics::VertexBufferDesc vbd;
			vbd.attrib[0].semantic = Graphics::ATTRIB_POSITION;
			vbd.attrib[0].format = Graphics::ATTRIB_FORMAT_FLOAT3;
			vbd.attrib[1].semantic = Graphics::ATTRIB_DIFFUSE;
			vbd.attrib[1].format = Graphics::ATTRIB_FORMAT_UBYTE4;
			vbd.numVertices = va.GetNumVerts();
			vbd.usage = Graphics::BUFFER_USAGE_STATIC;
			m_collisionMeshVB.Reset(RendererLocator::getRenderer()->CreateVertexBuffer(vbd));
			m_collisionMeshVB->Populate(va);
		}

		//might want to add some offset
		RendererLocator::getRenderer()->SetWireFrameMode(true);
		Graphics::RenderStateDesc rsd;
		rsd.cullMode = Graphics::CULL_NONE;
		RendererLocator::getRenderer()->DrawBuffer(m_collisionMeshVB.Get(), RendererLocator::getRenderer()->CreateRenderState(rsd), Graphics::vtxColorMaterial);
		RendererLocator::getRenderer()->SetWireFrameMode(false);
	}

	void Model::DrawCentralCylinder()
	{
		if (!m_centralCylinder) return;
		if (m_disk) {
			// TODO: Only for a 'CSG_Cylinder' aligned on Y axis
			Graphics::Renderer *r = RendererLocator::getRenderer();
			r->SetWireFrameMode(true);
			{
				Graphics::Renderer::MatrixTicket ticket(RendererLocator::getRenderer(), Graphics::MatrixMode::MODELVIEW);
				matrix4x4f mat = r->GetCurrentModelView();
				mat.Translate(0.0, m_centralCylinder->m_minH, 0.0);
				mat.RotateX(-M_PI / 2.0);
				r->SetTransform(mat);
				m_disk->Draw(r);
			}
			{
				Graphics::Renderer::MatrixTicket ticket(RendererLocator::getRenderer(), Graphics::MatrixMode::MODELVIEW);
				matrix4x4f mat = r->GetCurrentModelView();
				mat.Translate(0.0, m_centralCylinder->m_maxH, 0.0);
				mat.RotateX(-3.0 * M_PI / 2.0);
				r->SetTransform(mat);
				m_disk->Draw(r);
			}
			r->SetWireFrameMode(false);
			m_CCylConnectingLine->Draw(RendererLocator::getRenderer(), m_csg);
		}
	}

	void Model::DrawBoxes()
	{
		RendererLocator::getRenderer()->SetWireFrameMode(true);
		std::for_each(begin(m_csgBoxes), end(m_csgBoxes), [](const Graphics::Drawables::Box3D &box) {
				box.Draw(RendererLocator::getRenderer());
		});
		RendererLocator::getRenderer()->SetWireFrameMode(false);
	}

	void Model::DrawAxisIndicators(std::vector<Graphics::Drawables::Line3D> &lines)
	{
		for (auto i = lines.begin(); i != lines.end(); ++i)
			(*i).Draw(RendererLocator::getRenderer(), RendererLocator::getRenderer()->CreateRenderState(Graphics::RenderStateDesc()));
	}

	RefCountedPtr<CollMesh> Model::CreateCollisionMesh()
	{
		CollisionVisitor cv;
		m_root->Accept(cv);
		m_collMesh = cv.CreateCollisionMesh();
		m_boundingRadius = cv.GetBoundingRadius();
		return m_collMesh;
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
		return m_materials.at(Clamp(i, 0, int(m_materials.size()) - 1)).second;
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
			if (starts_with((*it)->GetName(), name)) {
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

	void Model::AddAxisIndicators(const std::vector<MatrixTransform *> &mts, std::vector<Graphics::Drawables::Line3D> &lines)
	{
		for (std::vector<MatrixTransform *>::const_iterator i = mts.begin(); i != mts.end(); ++i) {
			const matrix4x4f &trans = (*i)->GetTransform();
			const vector3f pos = trans.GetTranslate();
			const matrix3x3f &orient = trans.GetOrient();
			const vector3f x = orient.VectorX().Normalized();
			const vector3f y = orient.VectorY().Normalized();
			const vector3f z = orient.VectorZ().Normalized();

			Graphics::Drawables::Line3D lineX;
			lineX.SetStart(pos);
			lineX.SetEnd(pos + x);
			lineX.SetColor(Color::RED);

			Graphics::Drawables::Line3D lineY;
			lineY.SetStart(pos);
			lineY.SetEnd(pos + y);
			lineY.SetColor(Color::GREEN);

			Graphics::Drawables::Line3D lineZ;
			lineZ.SetStart(pos);
			lineZ.SetEnd(pos + z);
			lineZ.SetColor(Color::BLUE);

			lines.push_back(lineX);
			lines.push_back(lineY);
			lines.push_back(lineZ);
		}
	}

	void Model::SetDebugFlags(uint32_t flags)
	{
		m_debugFlags = flags;

		if (m_debugFlags & SceneGraph::Model::DEBUG_BBOX && m_tagPoints.empty()) {
			CreateAabbVB();
		}
		if (m_debugFlags & SceneGraph::Model::DEBUG_TAGS && m_tagPoints.empty()) {
			std::vector<MatrixTransform *> mts;
			FindTagsByStartOfName("tag_", mts);
			AddAxisIndicators(mts, m_tagPoints);
		}
		if (m_debugFlags & SceneGraph::Model::DEBUG_DOCKING && m_dockingPoints.empty()) {
			std::vector<MatrixTransform *> mts;
			FindTagsByStartOfName("entrance_", mts);
			AddAxisIndicators(mts, m_dockingPoints);
			FindTagsByStartOfName("loc_", mts);
			AddAxisIndicators(mts, m_dockingPoints);
			FindTagsByStartOfName("exit_", mts);
			AddAxisIndicators(mts, m_dockingPoints);
		}
		Graphics::Renderer *renderer = RendererLocator::getRenderer();
		if (m_debugFlags & SceneGraph::Model::DEBUG_COLLMESH && m_centralCylinder && !m_disk) {
			Graphics::RenderStateDesc rsd;
			rsd.cullMode = Graphics::FaceCullMode::CULL_NONE;
			m_csg = renderer->CreateRenderState(rsd);
			m_disk.reset(new Graphics::Drawables::Disk(renderer, m_csg, Color::BLUE, m_centralCylinder->m_diameter / 2.0));
			m_CCylConnectingLine.reset(new Graphics::Drawables::Line3D());
			m_CCylConnectingLine->SetStart(vector3f(0.f, m_centralCylinder->m_minH, 0.f));
			m_CCylConnectingLine->SetEnd(vector3f(0.f, m_centralCylinder->m_maxH, 0.f));
			m_CCylConnectingLine->SetColor(Color::BLUE);
		}

		if (m_debugFlags & SceneGraph::Model::DEBUG_COLLMESH && !m_Boxes.empty() && m_csgBoxes.empty()) {
			Graphics::RenderStateDesc rsd;
			rsd.cullMode = Graphics::FaceCullMode::CULL_NONE;
			m_csg = renderer->CreateRenderState(rsd);

			Graphics::MaterialDescriptor desc;
			m_boxes3DMat.Reset(RendererLocator::getRenderer()->CreateMaterial(desc));
			m_boxes3DMat->diffuse = Color::BLUE;

			std::for_each(begin(m_Boxes), end(m_Boxes), [&](const CSG_Box &box) {
				m_csgBoxes.push_back(Graphics::Drawables::Box3D(renderer, m_boxes3DMat, m_csg, box.m_min, box.m_max));
			});
		}
	}

	void Model::SetCentralCylinder(std::unique_ptr<CSG_CentralCylinder> centralcylinder) {
		m_centralCylinder = std::move(centralcylinder);
	}

	void Model::AddBox(std::unique_ptr<CSG_Box> box) {
		m_Boxes.push_back(*box.get());
	}
} // namespace SceneGraph
