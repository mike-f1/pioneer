// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _MODELCACHE_H
#define _MODELCACHE_H

#include <map>
#include <stdexcept>
#include <string>

#include "ShipType.h"

namespace SceneGraph {
	class Model;
}

/*
 * This class is a quick thoughtless hack
 * Also it only deals in New Models
 * Made Singleton (...until there's need for something else...)
 */
class ModelCache {
public:
	ModelCache() = delete;
	~ModelCache() = delete;

	struct ModelNotFoundException : public std::runtime_error {
		ModelNotFoundException(const std::string &name) :
			std::runtime_error("Could not find model '" + name + "'\n") {}
	};

	static void Init(const ShipType::t_mapTypes &types);
	static SceneGraph::Model *FindModel(const std::string &name, bool allowPlaceholder = true);

private:
	static void Flush();

	static SceneGraph::Model *findmodel(const std::string &name);

	typedef std::map<std::string, SceneGraph::Model *> ModelMap;
	inline static ModelMap s_models;
};

#endif
