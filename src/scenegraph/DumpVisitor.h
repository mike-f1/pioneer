// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "NodeVisitor.h"

#include <string>
#include <vector>

/*
 * Print the graph structure to console
 * Collect statistics
 */
namespace SceneGraph {

	class Model;

	class DumpVisitor final: public NodeVisitor {
	public:
		struct LodStatistics {
			unsigned int nodeCount;
			unsigned int opaqueGeomCount;
			unsigned int transGeomCount;

			unsigned int triangles;
		};

		struct ModelStatistics {
			unsigned int materialCount;
			unsigned int collTriCount;
		};

		explicit DumpVisitor(const Model *m);

		std::vector<std::string> GetModelStatistics(bool with_tree = false);

		void ApplyNode(Node &) override;
		void ApplyGroup(Group &) override;
		void ApplyLOD(LOD &) override;
		void ApplyStaticGeometry(StaticGeometry &) override;

	private:
		void StoreNodeName(const Node &);

		unsigned int m_level;
		ModelStatistics m_modelStats;
		LodStatistics m_stats;
		std::vector<LodStatistics> m_lodStats;
		std::vector<std::string> m_treeStructure;
	};

} // namespace SceneGraph
