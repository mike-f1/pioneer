// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _SCENEGRAPH_ANIMATION_H
#define _SCENEGRAPH_ANIMATION_H
/*
 * A named animation, such as "GearDown".
 * An animation has a number of channels, each of which
 * animate the position/rotation of a single MatrixTransform node
 */

#include "AnimationChannel.h"

#include <string>
#include <vector>

namespace SceneGraph {

	class Node;

	class Animation {
		friend class Loader;
		friend class BinaryConverter;
	public:
		Animation(const std::string &name, double duration);
		Animation(const Animation &);
		// post-copy function which find the corresponding  transform after copy or creation
		void UpdateChannelTargets(Node *root);
		double GetDuration() const { return m_duration; }
		const std::string &GetName() const { return m_name; }
		double GetProgress() const;
		void SetProgress(double); //0.0 -- 1.0, overrides m_time; NOTE: not calling this lead to an early out in Interpolate()
		void Interpolate(); //update transforms according to m_time;

	private:
		double m_duration;
		double m_time;
		std::string m_name;
		std::vector<AnimationChannel> m_channels;
		bool m_noNeedOfUpdate;
	};

} // namespace SceneGraph

#endif
