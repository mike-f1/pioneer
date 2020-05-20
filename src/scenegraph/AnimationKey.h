// Copyright © 2008-2019 Pioneer Developers. See AUTHORS.txt for details
// Licensed under the terms of the GPL v3. See licenses/GPL-3.txt

#ifndef _SCENEGRAPH_ANIMATIONKEY_H
#define _SCENEGRAPH_ANIMATIONKEY_H

#include "libs/quaternion.h"
#include "libs/vector3.h"

namespace SceneGraph {

	struct AnimationKey {
		double time;

		AnimationKey(double t) :
			time(t) {}
	};

	struct PositionKey : public AnimationKey {
		vector3f position;

		PositionKey(double t, const vector3f &pos) :
			AnimationKey(t),
			position(pos) {}
	};

	struct RotationKey : public AnimationKey {
		quaternionf rotation;

		RotationKey(double t, const quaternionf &q) :
			AnimationKey(t),
			rotation(q) {}
	};

	struct ScaleKey : public AnimationKey {
		vector3f scale;

		ScaleKey(double t, const vector3f &s) :
			AnimationKey(t),
			scale(s) {}
	};

} // namespace SceneGraph

#endif
