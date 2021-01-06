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
		virtual Node *Clone(NodeCopyCache *cache = 0) override;
		virtual const char *GetTypeName() const override { return "Group"; }
		virtual void Save(NodeDatabase &) override;
		static Group *Load(NodeDatabase &);

		void AddChild(Node *child);
		bool RemoveChild(Node *node); //true on success
		bool RemoveChildAt(unsigned int position); //true on success
		unsigned int GetNumChildren() const { return static_cast<uint32_t>(m_children.size()); }
		Node *GetChildAt(unsigned int);
		virtual void Accept(NodeVisitor &v) override;
		virtual void Traverse(NodeVisitor &v) override;
		virtual void Render(const matrix4x4f &trans, const RenderData *rd) override;
		virtual void Render(const std::vector<matrix4x4f> &trans, const RenderData *rd) override;
		virtual Node *FindNode(const std::string &) override;

	protected:
		virtual ~Group();
		virtual void RenderChildren(const matrix4x4f &trans, const RenderData *rd);
		virtual void RenderChildren(const std::vector<matrix4x4f> &trans, const RenderData *rd);
		std::vector<Node *> m_children;
	};

} // namespace SceneGraph

#endif
