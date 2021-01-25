// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _SCENEGRAPH_GROUP_H
#define _SCENEGRAPH_GROUP_H

#include "Node.h"
#include <vector>

namespace SceneGraph {

	class Group : public Node {
	public:
		Group();
		Group(const Group &, NodeCopyCache *cache = 0);
		Node *Clone(NodeCopyCache *cache = 0) override;
		const char *GetTypeName() const override { return "Group"; }
		void Save(NodeDatabase &) override;
		static Group *Load(NodeDatabase &);

		void AddChild(Node *child);
		bool RemoveChild(Node *node); //true on success
		bool RemoveChildAt(unsigned int position); //true on success
		unsigned int GetNumChildren() const { return static_cast<uint32_t>(m_children.size()); }
		Node *GetChildAt(unsigned int);
		void Accept(NodeVisitor &v) override;
		void Traverse(NodeVisitor &v) override;
		void Render(const matrix4x4f &trans, const RenderData *rd) override;
		void Render(const std::vector<matrix4x4f> &trans, const RenderData *rd) override;
		Node *FindNode(const std::string &) override;

	protected:
		virtual ~Group();
		void RenderChildren(const matrix4x4f &trans, const RenderData *rd);
		void RenderChildren(const std::vector<matrix4x4f> &trans, const RenderData *rd);
		std::vector<Node *> m_children;
	};

} // namespace SceneGraph

#endif
