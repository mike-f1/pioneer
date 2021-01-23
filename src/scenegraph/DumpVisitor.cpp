// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "DumpVisitor.h"

#include "CollMesh.h"
#include "Group.h"
#include "LOD.h"
#include "Model.h"
#include "Node.h"
#include "StaticGeometry.h"
#include "graphics/VertexBuffer.h"
#include "libs/utils.h"
#include <iostream>
#include <sstream>

namespace SceneGraph {
	DumpVisitor::DumpVisitor(const Model *m) :
		m_level(0),
		m_stats()
	{
		//model statistics that cannot be visited)
		m_modelStats.collTriCount = m->GetCollisionMesh() ? m->GetCollisionMesh()->GetNumTriangles() : 0;
		m_modelStats.materialCount = m->GetNumMaterials();
	}

	std::vector<std::string> DumpVisitor::GetModelStatistics()
	{
		std::vector<std::string> lines;

		// Print collected statistics per lod
		if (m_lodStats.empty())
			m_lodStats.push_back(m_stats);

		std::vector<LodStatistics>::iterator it = m_lodStats.begin();
		unsigned int idx = 1;
		while (it != m_lodStats.end()) {
			lines.emplace_back("LOD " + std::to_string(idx));
			lines.emplace_back("  Nodes: " + std::to_string(it->nodeCount));
			lines.emplace_back(std::string("  Geoms: ") + std::to_string(it->opaqueGeomCount) + " opaque," + std::to_string(it->transGeomCount) + "  transparent");
			lines.emplace_back("  Triangles: " + std::to_string(it->triangles));
			++it;
			idx++;
		};

		lines.emplace_back("");
		lines.emplace_back("Materials: " + std::to_string(m_modelStats.materialCount));
		lines.emplace_back("Collision triangles: " + std::to_string(m_modelStats.collTriCount));

		return lines;
	}

	void DumpVisitor::ApplyNode(Node &n)
	{
		PutIndent();
		PutNodeName(n);

		m_stats.nodeCount++;
	}

	void DumpVisitor::ApplyGroup(Group &g)
	{
		PutIndent();
		PutNodeName(g);

		m_level++;
		g.Traverse(*this);
		m_level--;

		m_stats.nodeCount++;
	}

	void DumpVisitor::ApplyLOD(LOD &l)
	{
		ApplyNode(l);

		m_level++;
		for (unsigned int i = 0; i < l.GetNumChildren(); i++) {
			l.GetChildAt(i)->Accept(*this);
			m_lodStats.push_back(m_stats);
			memset(&m_stats, 0, sizeof(LodStatistics));
		}
		m_level--;
	}

	void DumpVisitor::ApplyStaticGeometry(StaticGeometry &g)
	{
		if (g.GetNodeMask() & NODE_TRANSPARENT)
			m_stats.transGeomCount++;
		else
			m_stats.opaqueGeomCount++;

		for (unsigned int i = 0; i < g.GetNumMeshes(); i++)
			m_stats.triangles += g.GetMeshAt(i).indexBuffer->GetSize() / 3;

		ApplyNode(static_cast<Node &>(g));
	}

	void DumpVisitor::PutIndent() const
	{
		for (unsigned int i = 0; i < m_level; i++)
			Output("  ");
	}

	void DumpVisitor::PutNodeName(const Node &g) const
	{
		if (g.GetName().empty())
			Output("%s\n", g.GetTypeName());
		else
			Output("%s - %s\n", g.GetTypeName(), g.GetName().c_str());
	}
} // namespace SceneGraph
