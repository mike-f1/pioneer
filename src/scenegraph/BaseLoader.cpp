// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#include "BaseLoader.h"

#include "Model.h"
#include "LoaderDefinitions.h"

#include "FileSystem.h"
#include "libs/utils.h"
#include "graphics/Material.h"
#include "graphics/Renderer.h"
#include "graphics/RendererLocator.h"
#include "graphics/TextureBuilder.h"
#include "libs/stringUtils.h"

using namespace SceneGraph;

BaseLoader::BaseLoader() :
	m_model(nullptr)
{
	Graphics::Texture *sdfTex = Graphics::TextureBuilder("fonts/label3d.dds",
		Graphics::TextureSampleMode::LINEAR_CLAMP, true, true, true)
									.GetOrCreateTexture(RendererLocator::getRenderer(), "model");
	m_labelFont.Reset(new Text::DistanceFieldFont("fonts/sdf_definition.txt", sdfTex));
}

void BaseLoader::ConvertMaterialDefinition(const MaterialDefinition &mdef)
{
	//Build material descriptor
	const std::string &diffTex = mdef.tex_diff;
	const std::string &specTex = mdef.tex_spec;
	const std::string &glowTex = mdef.tex_glow;
	const std::string &ambiTex = mdef.tex_ambi;
	const std::string &normTex = mdef.tex_norm;

	Graphics::MaterialDescriptor matDesc;
	matDesc.lighting = !mdef.unlit;
	matDesc.alphaTest = mdef.alpha_test;

	matDesc.usePatterns = mdef.use_pattern;

	//diffuse texture is a must. Will create a white dummy texture if one is not supplied
	matDesc.textures = 1;
	matDesc.specularMap = !specTex.empty();
	matDesc.glowMap = !glowTex.empty();
	matDesc.ambientMap = !ambiTex.empty();
	matDesc.normalMap = !normTex.empty();
	matDesc.quality = Graphics::HAS_HEAT_GRADIENT;

	//Create material and set parameters
	RefCountedPtr<Graphics::Material> mat(RendererLocator::getRenderer()->CreateMaterial(matDesc));
	mat->diffuse = mdef.diffuse;
	mat->specular = mdef.specular;
	mat->emissive = mdef.emissive;
	mat->shininess = mdef.shininess;

	//semitransparent material
	//the node must be marked transparent when using this material
	//and should not be mixed with opaque materials
	if (mdef.opacity < 100)
		mat->diffuse.a = (float(mdef.opacity) / 100.f) * 255;

	if (!diffTex.empty())
		mat->texture0 = Graphics::TextureBuilder::Model(diffTex).GetOrCreateTexture(RendererLocator::getRenderer(), "model");
	else
		mat->texture0 = Graphics::TextureBuilder::GetWhiteTexture(RendererLocator::getRenderer());
	if (!specTex.empty())
		mat->texture1 = Graphics::TextureBuilder::Model(specTex).GetOrCreateTexture(RendererLocator::getRenderer(), "model");
	if (!glowTex.empty())
		mat->texture2 = Graphics::TextureBuilder::Model(glowTex).GetOrCreateTexture(RendererLocator::getRenderer(), "model");
	if (!ambiTex.empty())
		mat->texture3 = Graphics::TextureBuilder::Model(ambiTex).GetOrCreateTexture(RendererLocator::getRenderer(), "model");
	//texture4 is reserved for pattern
	//texture5 is reserved for color gradient
	if (!normTex.empty())
		mat->texture6 = Graphics::TextureBuilder::Normal(normTex).GetOrCreateTexture(RendererLocator::getRenderer(), "model");

	m_model->m_materials.push_back(std::make_pair(mdef.name, mat));
}

RefCountedPtr<Graphics::Material> BaseLoader::GetDecalMaterial(unsigned int index)
{
	assert(index <= Model::MAX_DECAL_MATERIALS);
	RefCountedPtr<Graphics::Material> &decMat = m_model->m_decalMaterials[index - 1];
	if (!decMat.Valid()) {
		Graphics::MaterialDescriptor matDesc;
		matDesc.textures = 1;
		matDesc.lighting = true;
		decMat.Reset(RendererLocator::getRenderer()->CreateMaterial(matDesc));
		decMat->texture0 = Graphics::TextureBuilder::GetTransparentTexture(RendererLocator::getRenderer());
		decMat->specular = Color::BLACK;
		decMat->diffuse = Color::WHITE;
	}
	return decMat;
}

void BaseLoader::FindPatterns(PatternContainer &output)
{
	for (FileSystem::FileEnumerator files(FileSystem::gameDataFiles, m_curPath); !files.Finished(); files.Next()) {
		const FileSystem::FileInfo &info = files.Current();
		if (info.IsFile()) {
			const std::string &name = info.GetName();
			if (stringUtils::starts_with(name, "pattern")) {
				if (stringUtils::ends_with_ci(name, ".png") || stringUtils::ends_with_ci(name, ".dds"))
					output.push_back(Pattern(name, m_curPath, RendererLocator::getRenderer()));
			}
		}
	}
}

void BaseLoader::SetUpPatterns()
{
	FindPatterns(m_model->m_patterns);

	if (m_model->m_patterns.empty()) {
		m_model->m_patterns.push_back(Pattern());
		Pattern &dumpat = m_model->m_patterns.back();
		dumpat.name = "Dummy";
		dumpat.texture = RefCountedPtr<Graphics::Texture>(Graphics::TextureBuilder::GetWhiteTexture(RendererLocator::getRenderer()));
	}

	//set up some noticeable default colors
	std::vector<Color> colors;
	colors.push_back(Color::RED);
	colors.push_back(Color::GREEN);
	colors.push_back(Color::BLUE);
	m_model->SetColors(colors);
	m_model->SetPattern(0);
}
