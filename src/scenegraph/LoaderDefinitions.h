// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _LOADERDEFINITIONS_H_
#define _LOADERDEFINITIONS_H_
/*
 * Data strcutrures used by Loader
 */

#include <string>
#include <vector>
#include "Color.h"

namespace SceneGraph {

	struct MaterialDefinition {
		explicit MaterialDefinition(const std::string &n) :
			name(n),
			tex_diff(""),
			tex_spec(""),
			tex_glow(""),
			tex_ambi(""),
			tex_norm(""),
			diffuse(Color::WHITE),
			specular(Color::WHITE),
			ambient(Color::BLANK),
			emissive(Color::BLANK),
			shininess(100),
			opacity(100),
			alpha_test(false),
			unlit(false),
			use_pattern(false)
		{}
		std::string name;
		std::string tex_diff;
		std::string tex_spec;
		std::string tex_glow;
		std::string tex_ambi;
		std::string tex_norm;
		Color diffuse;
		Color specular;
		Color ambient;
		Color emissive;
		unsigned int shininess; //specular power, 0-128
		unsigned int opacity; //0-100
		bool alpha_test;
		bool unlit;
		bool use_pattern;
	};

	struct LodDefinition {
		explicit LodDefinition(float size) :
			pixelSize(size)
		{}
		float pixelSize;
		std::vector<std::string> meshNames;
	};

	struct AnimDefinition {
		AnimDefinition(const std::string &name_, double start_, double end_, bool loop_) :
			name(name_),
			start(start_),
			end(end_),
			loop(loop_)
		{}
		std::string name;
		double start;
		double end;
		bool loop;
	};
	typedef std::vector<AnimDefinition> AnimList;

	struct ModelDefinition {
		std::string name;
		std::vector<LodDefinition> lodDefs;
		std::vector<MaterialDefinition> matDefs;
		std::vector<std::string> collisionDefs;
		AnimList animDefs;
	};

} // namespace SceneGraph

#endif
