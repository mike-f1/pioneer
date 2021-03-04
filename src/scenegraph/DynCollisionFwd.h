#include "libs/matrix4x4.h"
#include "libs/RefCounted.h"

#include <tuple>

namespace SceneGraph {
	class MatrixTransform;
}

namespace Graphics {
	class VertexBuffer;
}

class GeomTree;

// Some shared definitions used for dynamic collisions usage.

// 0: static transform; 1: reference to animated transform; 2: related GeomTree
using TupleForDynCollision = std::tuple<matrix4x4f, SceneGraph::MatrixTransform *, GeomTree *>;

// 0: static transform; 1: reference to animated transform; 2: related VertexBuffer
using TupleForDynCollisionVB = std::tuple<matrix4x4f, SceneGraph::MatrixTransform *, RefCountedPtr<Graphics::VertexBuffer>>;
