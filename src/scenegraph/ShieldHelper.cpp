#include "ShieldHelper.h"

#include "FindNodeVisitor.h"
#include "Group.h"
#include "MatrixTransform.h"
#include "Model.h"
#include "NodeVisitor.h"
#include "StaticGeometry.h"
#include "graphics/RenderState.h"
#include "graphics/Renderer.h"
#include "graphics/RendererLocator.h"
#include "libs/matrix4x4.h"

#include <sstream>

namespace ShieldHelper {

	//used to find the accumulated transform of a MatrixTransform
	class MatrixAccumVisitor : public SceneGraph::NodeVisitor {
	public:
		MatrixAccumVisitor(const std::string &name_) :
			outMat(matrix4x4f::Identity()),
			m_accumMat(matrix4x4f::Identity()),
			m_name(name_)
		{
		}

		virtual void ApplyMatrixTransform(SceneGraph::MatrixTransform &mt) override
		{
			if (mt.GetName() == m_name) {
				outMat = m_accumMat * mt.GetTransform();
			} else {
				const matrix4x4f prevAcc = m_accumMat;
				m_accumMat = m_accumMat * mt.GetTransform();
				mt.Traverse(*this);
				m_accumMat = prevAcc;
			}
		}

		matrix4x4f outMat;

	private:
		matrix4x4f m_accumMat;
		std::string m_name;
	};

	void ReparentShieldNodes(SceneGraph::Model *model)
	{
		using SceneGraph::MatrixTransform;
		using SceneGraph::Node;
		using SceneGraph::StaticGeometry;
		using SceneGraph::FindNodeVisitor;
		using SceneGraph::Group;

		//This will find all matrix transforms meant for navlights.
		FindNodeVisitor shieldFinder(FindNodeVisitor::Criteria::MATCH_NAME_ENDSWITH, "_shield");
		model->GetRoot()->Accept(shieldFinder);
		const std::vector<Node *> &results = shieldFinder.GetResults();

		//Move shield geometry to same level as the LODs
		for (unsigned int i = 0; i < results.size(); i++) {
			MatrixTransform *mt = dynamic_cast<MatrixTransform *>(results.at(i));
			assert(mt);

			const uint32_t NumChildren = mt->GetNumChildren();
			if (NumChildren > 0) {
				// Group to contain all of the shields we might find
				Group *shieldGroup = new Group();
				shieldGroup->SetName(s_shieldGroupName);

				// go through all of this MatrixTransforms children to extract all of the shield meshes
				for (uint32_t iChild = 0; iChild < NumChildren; ++iChild) {
					Node *node = mt->GetChildAt(iChild);
					assert(node);
					if (node) {
						RefCountedPtr<StaticGeometry> sg(dynamic_cast<StaticGeometry *>(node));
						assert(sg.Valid());
						sg->SetNodeMask(SceneGraph::NODE_TRANSPARENT);

						// We can early-out if we've already processed this models scenegraph.
						if (Graphics::BLEND_ALPHA == sg->m_blendMode) {
							assert(false);
						}

						// force the blend mode
						sg->m_blendMode = Graphics::BLEND_ALPHA;

						Graphics::RenderStateDesc rsd;
						rsd.blendMode = Graphics::BLEND_ALPHA;
						rsd.depthWrite = false;
						sg->SetRenderState(RendererLocator::getRenderer()->CreateRenderState(rsd));

						// find the accumulated transform from the root to our node
						MatrixAccumVisitor mav(mt->GetName());
						model->GetRoot()->Accept(mav);

						// set our nodes transformation to be the accumulated transform
						MatrixTransform *sg_transform_parent = new MatrixTransform(mav.outMat);
						std::stringstream nodeStream;
						nodeStream << iChild << s_matrixTransformName;
						sg_transform_parent->SetName(nodeStream.str());
						sg_transform_parent->AddChild(sg.Get());

						// detach node from current location in the scenegraph...
						mt->RemoveChild(node);

						// attach new transform node which parents the our shields mesh to the shield group.
						shieldGroup->AddChild(sg_transform_parent);
					}
				}

				model->GetRoot()->AddChild(shieldGroup);
			}
		}
	}

} // namespace ShieldHelper
