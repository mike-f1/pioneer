#include "ModelDebug.h"

#include "CollMesh.h"
#include "MatrixTransform.h"
#include "Model.h"
#include "collider/CSGDefinitions.h"
#include "collider/GeomTree.h"
#include "graphics/Drawables.h"
#include "graphics/Material.h"
#include "graphics/RenderState.h"
#include "graphics/Renderer.h"
#include "graphics/RendererLocator.h"
#include "graphics/VertexArray.h"
#include "graphics/VertexBuffer.h"

#include "DynCollisionVisitor.h"
#include "CollisionVisitor.h"

namespace SceneGraph {
	ModelDebug::ModelDebug(Model *m, DebugFlags flags) :
		m_model(m),
		m_flags(DebugFlags::NONE)
	{
		UpdateFlags(flags);
	}

	ModelDebug::~ModelDebug()
	{
		//dtor
	}

	void ModelDebug::UpdateFlags(DebugFlags flags)
	{
		if (m_flags == flags) return;
		m_flags = flags;

		if (to_bool(m_flags & DebugFlags::BBOX)) {
			CreateAabbVB();
		}

		if (to_bool(m_flags & SceneGraph::DebugFlags::TAGS) && m_tagPoints.empty()) {
			std::vector<MatrixTransform *> mts;
			m_model->FindTagsByStartOfName("tag_", mts);
			AddAxisIndicators(mts, m_tagPoints);
		}

		if (to_bool(m_flags & DebugFlags::DOCKING) && m_dockingPoints.empty()) {
			std::vector<MatrixTransform *> mts;
			m_model->FindTagsByStartOfName("entrance_", mts);
			AddAxisIndicators(mts, m_dockingPoints);
			m_model->FindTagsByStartOfName("loc_", mts);
			AddAxisIndicators(mts, m_dockingPoints);
			m_model->FindTagsByStartOfName("exit_", mts);
			AddAxisIndicators(mts, m_dockingPoints);
		}

		Graphics::Renderer *renderer = RendererLocator::getRenderer();
		if (to_bool(m_flags & SceneGraph::DebugFlags::COLLMESH) && m_model->GetCentralCylinder() && !m_disk) {
			Graphics::RenderStateDesc rsd;
			rsd.cullMode = Graphics::FaceCullMode::CULL_NONE;
			m_csg = renderer->CreateRenderState(rsd);
			m_disk.reset(new Graphics::Drawables::Disk(renderer, m_csg, Color::BLUE, m_model->GetCentralCylinder()->m_diameter / 2.0));
			m_CCylConnectingLine.reset(new Graphics::Drawables::Line3D());
			m_CCylConnectingLine->SetStart(vector3f(0.f, m_model->GetCentralCylinder()->m_minH, 0.f));
			m_CCylConnectingLine->SetEnd(vector3f(0.f, m_model->GetCentralCylinder()->m_maxH, 0.f));
			m_CCylConnectingLine->SetColor(Color::BLUE);
		}

		if (to_bool(m_flags & SceneGraph::DebugFlags::COLLMESH) && !m_model->GetBoxes().empty() && m_csgBoxes.empty()) {
			Graphics::RenderStateDesc rsd;
			rsd.cullMode = Graphics::FaceCullMode::CULL_NONE;
			m_csg = renderer->CreateRenderState(rsd);

			Graphics::MaterialDescriptor desc;
			m_boxes3DMat.Reset(RendererLocator::getRenderer()->CreateMaterial(desc));
			m_boxes3DMat->diffuse = Color::BLUE;

			std::for_each(begin(m_model->GetBoxes()), end(m_model->GetBoxes()), [&](const CSG_Box &box) {
				m_csgBoxes.push_back(Graphics::Drawables::Box3D(renderer, m_boxes3DMat, m_csg, box.m_min, box.m_max));
			});
		}
	}

	void ModelDebug::Render(const matrix4x4f &trans)
	{
		if (to_bool(m_flags & DebugFlags::BBOX)) {
			RendererLocator::getRenderer()->SetTransform(trans);
			DrawAabb();
		}

		if (to_bool(m_flags & DebugFlags::TAGS)) {
			RendererLocator::getRenderer()->SetTransform(trans);
			DrawAxisIndicators(m_tagPoints);
		}

		if (to_bool(m_flags & DebugFlags::DOCKING)) {
			RendererLocator::getRenderer()->SetTransform(trans);
			DrawAxisIndicators(m_dockingPoints);
		}

		if (to_bool(m_flags & DebugFlags::COLLMESH)) {
			RendererLocator::getRenderer()->SetTransform(trans);
			DrawCollisionMesh();
			DrawCentralCylinder();
			DrawBoxes();
		}
	}

	void ModelDebug::CreateAabbVB()
	{
		if (!m_model->GetCollisionMesh()) return;

		const Aabb aabb = m_model->GetCollisionMesh()->GetAabb();

		Graphics::MaterialDescriptor desc;
		m_aabbMat.Reset(RendererLocator::getRenderer()->CreateMaterial(desc));
		m_aabbMat->diffuse = Color::GREEN;

		m_state = RendererLocator::getRenderer()->CreateRenderState(Graphics::RenderStateDesc());

		m_aabbBox3D.reset(new Graphics::Drawables::Box3D(RendererLocator::getRenderer(), m_aabbMat, m_state, vector3f(aabb.min), vector3f(aabb.max)));
	}

	void ModelDebug::DrawAabb()
	{
		if (!m_aabbBox3D) return;

		RendererLocator::getRenderer()->SetWireFrameMode(true);
		m_aabbBox3D->Draw(RendererLocator::getRenderer());
		RendererLocator::getRenderer()->SetWireFrameMode(false);
	}

	void ModelDebug::DrawAxisIndicators(std::vector<Graphics::Drawables::Line3D> &lines)
	{
		for (auto i = lines.begin(); i != lines.end(); ++i)
			(*i).Draw(RendererLocator::getRenderer(), RendererLocator::getRenderer()->CreateRenderState(Graphics::RenderStateDesc()));
	}

	void ModelDebug::AddAxisIndicators(const std::vector<MatrixTransform *> &mts, std::vector<Graphics::Drawables::Line3D> &lines)
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

	static RefCountedPtr<Graphics::VertexBuffer> createVertexBufferFor(const GeomTree *gt)
	{
		const std::vector<vector3f> &vertices = gt->GetVertices();
		const std::vector<uint32_t> &indices = gt->GetIndices();
		const std::vector<unsigned> &triFlags = gt->GetTriFlags();
		const unsigned int numIndices = gt->GetNumTris() * 3;

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
		RefCountedPtr<Graphics::VertexBuffer> collisionMeshVB(RendererLocator::getRenderer()->CreateVertexBuffer(vbd));
		collisionMeshVB->Populate(va);
		return collisionMeshVB;
	}

	// Draw collision mesh as a wireframe overlay
	void ModelDebug::DrawCollisionMesh()
	{
		RefCountedPtr<CollMesh> collMesh = m_model->GetCollisionMesh();
		if (!collMesh.Valid()) return;

		if (!m_collisionMeshVB.Valid()) {
			// Creating static buffers
			m_collisionMeshVB = createVertexBufferFor(collMesh->GetGeomTree());

			//have to figure out which collision geometries are responsible for which geomtrees
			printf("Creating VB\n");
			//SceneGraph::DynGeomFinder dgf;
			//m_model->GetRoot()->Accept(dgf);

			for (auto &dgt : collMesh->GetDynGeomTrees()) {
				printf("dgt.first =**; dgt.second = %p\n", dgt.second);
				//SceneGraph::CollisionGeometry *cg = dgf.FindCgForTree(dgt);
				//if (!cg) throw std::runtime_error { "No CollisionGeometry found" };
				m_dynCollisionMeshVB.push_back({dgt.first, createVertexBufferFor(dgt.second)});
				dgt.first.Print();

	//dynamic geoms
/*	for (auto *dgt : m_model->GetCollisionMesh()->GetDynGeomTrees()) {
		m_dynGeoms.emplace_back(std::make_unique<Geom>(dgt, GetOrient(), GetPosition(), this));
		auto &dynG = m_dynGeoms.back();
		dynG->m_animTransform = matrix4x4d::Identity();
		SceneGraph::CollisionGeometry *cg = dgf.GetCgForTree(dgt);
		if (cg)
			cg->SetGeom(dynG.get());
	}
*/
			}
		}

		// TODO: might want to add some offset
		RendererLocator::getRenderer()->SetWireFrameMode(true);
		Graphics::RenderStateDesc rsd;
		rsd.cullMode = Graphics::CULL_NONE;
		Graphics::RenderState *rs = RendererLocator::getRenderer()->CreateRenderState(rsd);
		RendererLocator::getRenderer()->DrawBuffer(m_collisionMeshVB.Get(), rs, Graphics::vtxColorMaterial);
		for (auto &dynVB : m_dynCollisionMeshVB) {
			Graphics::Renderer::MatrixTicket ticket(RendererLocator::getRenderer(), Graphics::MatrixMode::MODELVIEW);
			matrix4x4f prev = RendererLocator::getRenderer()->GetCurrentModelView();
			prev = prev * dynVB.first;
			RendererLocator::getRenderer()->SetTransform(prev);
			RendererLocator::getRenderer()->DrawBuffer(dynVB.second.Get(), rs, Graphics::vtxColorMaterial);
		}
		RendererLocator::getRenderer()->SetWireFrameMode(false);
	}

	void ModelDebug::DrawCentralCylinder()
	{
		if (!m_model->GetCentralCylinder()) return;
		if (m_disk) {
			// TODO: Only for a 'CSG_Cylinder' aligned on Y axis
			Graphics::Renderer *r = RendererLocator::getRenderer();
			r->SetWireFrameMode(true);
			{
				Graphics::Renderer::MatrixTicket ticket(RendererLocator::getRenderer(), Graphics::MatrixMode::MODELVIEW);
				matrix4x4f mat = r->GetCurrentModelView();
				mat.Translate(0.0, m_model->GetCentralCylinder()->m_minH, 0.0);
				mat.RotateX(-M_PI / 2.0);
				r->SetTransform(mat);
				m_disk->Draw(r);
			}
			{
				Graphics::Renderer::MatrixTicket ticket(RendererLocator::getRenderer(), Graphics::MatrixMode::MODELVIEW);
				matrix4x4f mat = r->GetCurrentModelView();
				mat.Translate(0.0, m_model->GetCentralCylinder()->m_maxH, 0.0);
				mat.RotateX(-3.0 * M_PI / 2.0);
				r->SetTransform(mat);
				m_disk->Draw(r);
			}
			r->SetWireFrameMode(false);
			m_CCylConnectingLine->Draw(RendererLocator::getRenderer(), m_csg);
		}
	}

	void ModelDebug::DrawBoxes()
	{
		RendererLocator::getRenderer()->SetWireFrameMode(true);
		std::for_each(begin(m_csgBoxes), end(m_csgBoxes), [](const Graphics::Drawables::Box3D &box) {
				box.Draw(RendererLocator::getRenderer());
		});
		RendererLocator::getRenderer()->SetWireFrameMode(false);
	}

} // namespace SceneGraph
