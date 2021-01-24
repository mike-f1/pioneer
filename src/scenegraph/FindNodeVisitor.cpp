// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "FindNodeVisitor.h"

#include "Node.h"
#include "libs/stringUtils.h"

namespace SceneGraph {

	FindNodeVisitor::FindNodeVisitor(Criteria c, const std::string &s) :
		m_criteria(c),
		m_string(s)
	{
	}

	void FindNodeVisitor::ApplyNode(Node &n)
	{
		switch (m_criteria) {
			case Criteria::MATCH_NAME_STARTSWITH:
				if (!n.GetName().empty() && stringUtils::starts_with(n.GetName(), m_string.c_str()))
					m_results.push_back(&n);
			break;
			case Criteria::MATCH_NAME_ENDSWITH:
				if (!n.GetName().empty() && stringUtils::ends_with(n.GetName(), m_string.c_str()))
					m_results.push_back(&n);
			break;
			case Criteria::MATCH_NAME_FULL:
				if (!n.GetName().empty() && n.GetName() == m_string)
					m_results.push_back(&n);
			break;
			case Criteria::MATCH_TYPE:
				if (n.GetTypeName() == m_string)
					m_results.push_back(&n);
			break;
		}
		n.Traverse(*this);
	}

} // namespace SceneGraph
