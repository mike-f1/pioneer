// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _MODELCACHE_H
#define _MODELCACHE_H
/*
 * This class is a quick thoughtless hack
 * Also it only deals in New Models
 * Made Singleton (...until there's need for something else...)
 */
#include <map>
#include <stdexcept>
#include <string>

namespace Graphics {
	class Renderer;
}

namespace SceneGraph {
	class Model;
}

class ModelCache {
public:
	ModelCache() = delete;
	~ModelCache() = delete;

	struct ModelNotFoundException : public std::runtime_error {
		ModelNotFoundException() :
			std::runtime_error("Could not find model") {}
	};
	static void Init(Graphics::Renderer *r);
	static SceneGraph::Model *FindModel(const std::string &name, bool allowPlaceholder = true);

private:
	static void Flush();

	static SceneGraph::Model *findmodel(const std::string &name);

	typedef std::map<std::string, SceneGraph::Model *> ModelMap;
	static ModelMap s_models;
	static Graphics::Renderer *s_renderer;
};

#endif
