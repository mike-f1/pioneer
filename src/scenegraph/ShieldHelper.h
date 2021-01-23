#ifndef SHIELDHELPER_H
#define SHIELDHELPER_H

#include <string>

namespace SceneGraph {
	class Model;
}

namespace ShieldHelper
{
	static const std::string s_shieldGroupName("Shields");
	static const std::string s_matrixTransformName("_accMtx4");

	void ReparentShieldNodes(SceneGraph::Model *model);
}

#endif // SHIELDHELPER_H
