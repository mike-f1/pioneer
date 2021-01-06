// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _SCENEGRAPH_LOADER_H
#define _SCENEGRAPH_LOADER_H
/**
 * Model loader baseclass
 */
#include "text/DistanceFieldFont.h"

#include "Pattern.h"

#include <string>
#include "libs/RefCounted.h"

namespace Graphics {
	class Material;
}

namespace SceneGraph {

	class Model;
	class MaterialDefinition;

	class BaseLoader {
	public:
		BaseLoader();

		RefCountedPtr<Text::DistanceFieldFont> GetLabel3DFont() const { return m_labelFont; }

		//allocate material for dynamic decal, should be used in order 1..4
		RefCountedPtr<Graphics::Material> GetDecalMaterial(unsigned int index);

	protected:
		Model *m_model;
		std::string m_curPath; //path of current model file
		RefCountedPtr<Text::DistanceFieldFont> m_labelFont;

		//create a material from definition and add it to m_model
		void ConvertMaterialDefinition(const MaterialDefinition &);
		//find pattern texture files from the model directory
		void FindPatterns(PatternContainer &output);
		void SetUpPatterns();
	};

} // namespace SceneGraph
#endif
