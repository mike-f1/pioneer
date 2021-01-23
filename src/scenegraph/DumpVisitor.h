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

	class DumpVisitor : public NodeVisitor {
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

		std::vector<std::string> GetModelStatistics();

		virtual void ApplyNode(Node &) override;
		virtual void ApplyGroup(Group &) override;
		virtual void ApplyLOD(LOD &) override;
		virtual void ApplyStaticGeometry(StaticGeometry &) override;

	private:
		void PutIndent() const;
		void PutNodeName(const Node &) const;

		unsigned int m_level;
		ModelStatistics m_modelStats;
		LodStatistics m_stats;
		std::vector<LodStatistics> m_lodStats;
	};

} // namespace SceneGraph
