#ifndef MODELDEBUG_H
#define MODELDEBUG_H

#include "libs/matrix4x4.h"
#include "libs/RefCounted.h"
#include <memory>
#include <vector>

namespace Graphics {
	class RenderState;
	class VertexBuffer;
	class Material;
	namespace Drawables {
		class Box3D;
		class Line3D;
		class Disk;
	} // namespace Drawables
} // namespace Graphics

namespace SceneGraph {
	class CollisionGeometry;
	class MatrixTransform;
	class Model;
	enum class DebugFlags;

	class ModelDebug {
	public:
		ModelDebug(Model *m, DebugFlags flags);
		~ModelDebug();

		void UpdateFlags(DebugFlags flags);

		void Render(const matrix4x4f &trans);

	private:
		Model *m_model;
		DebugFlags m_flags;

		void DrawCollisionMesh();
		void CreateAabbVB();
		void DrawAabb();
		void DrawCentralCylinder();
		void DrawBoxes();

		void DrawAxisIndicators(std::vector<Graphics::Drawables::Line3D> &lines);
		void AddAxisIndicators(const std::vector<MatrixTransform *> &mts, std::vector<Graphics::Drawables::Line3D> &lines);

		Graphics::RenderState *m_csg;
		Graphics::RenderState *m_state;
		std::unique_ptr<Graphics::Drawables::Box3D> m_aabbBox3D;
		RefCountedPtr<Graphics::Material> m_aabbMat;
		std::vector<Graphics::Drawables::Line3D> m_dockingPoints;
		std::vector<Graphics::Drawables::Line3D> m_tagPoints;
		std::unique_ptr<Graphics::Drawables::Disk> m_disk;
		std::unique_ptr<Graphics::Drawables::Line3D> m_CCylConnectingLine;
		std::vector<Graphics::Drawables::Box3D> m_csgBoxes;
		RefCountedPtr<Graphics::VertexBuffer> m_collisionMeshVB;
		std::vector<std::tuple<matrix4x4f, MatrixTransform *, RefCountedPtr<Graphics::VertexBuffer>>> m_dynCollisionMeshVB;
		RefCountedPtr<Graphics::Material> m_boxes3DMat;
	};
} // namespace SceneGraph

#endif // MODELDEBUG_H
