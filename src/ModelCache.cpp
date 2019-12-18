// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "ModelCache.h"

#include "Shields.h"
#include "scenegraph/SceneGraph.h"
#include "scenegraph/Loader.h"
#include "utils.h"

ModelCache::ModelMap ModelCache::s_models;

void ModelCache::Init()
{
}

SceneGraph::Model *ModelCache::findmodel(const std::string &name)
{
	ModelCache::ModelMap::iterator it = s_models.find(name);

	if (it == s_models.end()) {
		try {
			SceneGraph::Loader loader;
			SceneGraph::Model *m = loader.LoadModel(name);
			Shields::ReparentShieldNodes(m);
			s_models[name] = m;
			return m;
		} catch (SceneGraph::LoadingError &) {
			throw ModelNotFoundException();
		}
	}
	return it->second;
}

SceneGraph::Model *ModelCache::FindModel(const std::string &name, bool allowPlaceholder)
{
	SceneGraph::Model *m = 0;
	try {
		m = findmodel(name);
	} catch (const ModelCache::ModelNotFoundException &) {
		Output("Could not find model: %s\n", name.c_str());
		if (allowPlaceholder) {
			try {
				m = findmodel("error");
			} catch (const ModelCache::ModelNotFoundException &) {
				Error("Could not find placeholder model");
			}
		}
	}

	return m;
}

void ModelCache::Flush()
{
	for (ModelMap::iterator it = s_models.begin(); it != s_models.end(); ++it) {
		delete it->second;
	}
	s_models.clear();
}
