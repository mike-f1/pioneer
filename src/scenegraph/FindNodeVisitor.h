// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _SCENEGRAPH_FINDNODEVISITOR_H
#define _SCENEGRAPH_FINDNODEVISITOR_H
/*
 * Returns a list of nodes according to search criteria
 * Note, does not remove duplicates
 */
#include "NodeVisitor.h"
#include <string>
#include <vector>

namespace SceneGraph {

	class FindNodeVisitor final : public NodeVisitor {
	public:
		enum class Criteria { //or criterion. whatever.
			MATCH_NAME_FULL,
			MATCH_NAME_STARTSWITH,
			MATCH_NAME_ENDSWITH,
			MATCH_TYPE
		};
		FindNodeVisitor(Criteria crit, const std::string &searchstring);
		void ApplyNode(Node &) override final;

		const std::vector<Node *> &GetResults() { return m_results; }

	private:
		std::vector<Node *> m_results;
		Criteria m_criteria;
		std::string m_string;
	};

} // namespace SceneGraph

#endif
