// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _MODELNODE_H
#define _MODELNODE_H
/*
 *	Use another model as a submodel
 */

#include "libs/matrix4x4.h"

#include <vector>

#include "Node.h"

namespace SceneGraph {

	class Model;

	class ModelNode final : public Node {
	public:
		ModelNode(Model *m);
		ModelNode(const ModelNode &, NodeCopyCache *cache = 0);
		Node *Clone(NodeCopyCache *cache = 0) override;
		const char *GetTypeName() const override { return "ModelNode"; }
		void Render(const matrix4x4f &trans, const RenderData *rd) override;
		void Render(const std::vector<matrix4x4f> &trans, const RenderData *rd) override;

	protected:
		virtual ~ModelNode() {}

	private:
		Model *m_model;
	};

} // namespace SceneGraph

#endif
