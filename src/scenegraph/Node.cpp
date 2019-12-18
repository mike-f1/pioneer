// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "Node.h"
#include "NodeVisitor.h"
#include "Serializer.h"
#include "graphics/Drawables.h"
#include "graphics/Renderer.h"
#include "graphics/RendererLocator.h"

namespace SceneGraph {

	Node::Node() :
		m_name(""),
		m_nodeMask(NODE_SOLID),
		m_nodeFlags(0)
	{
	}

	Node::Node(unsigned int nodemask) :
		m_name(""),
		m_nodeMask(nodemask),
		m_nodeFlags(0)
	{
	}

	Node::Node(const Node &node, NodeCopyCache *cache) :
		m_name(node.m_name),
		m_nodeMask(node.m_nodeMask),
		m_nodeFlags(node.m_nodeFlags)
	{
	}

	Node::~Node()
	{
	}

	void Node::Accept(NodeVisitor &v)
	{
		v.ApplyNode(*this);
	}

	void Node::Traverse(NodeVisitor &v)
	{
	}

	Node *Node::FindNode(const std::string &name)
	{
		if (m_name == name)
			return this;
		else
			return nullptr;
	}

	void Node::DrawAxes()
	{
		Graphics::Drawables::Axes3D *axes = Graphics::Drawables::GetAxes3DDrawable(RendererLocator::getRenderer());
		axes->Draw(RendererLocator::getRenderer());
	}

	void Node::Save(NodeDatabase &db)
	{
		db.wr->String(GetTypeName());
		db.wr->String(m_name.c_str());
		db.wr->Int32(m_nodeMask);
		db.wr->Int32(m_nodeFlags);
	}

} // namespace SceneGraph
