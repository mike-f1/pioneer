#ifndef COLLISIONCALLBACKFWD_H_INCLUDED
#define COLLISIONCALLBACKFWD_H_INCLUDED

#include <functional>
#include <vector>

struct CollisionContact;

typedef std::vector<CollisionContact> CollisionContactVector;

typedef std::function<void(const CollisionContactVector &)> CollCallback;

#endif // COLLISIONCALLBACKFWD_H_INCLUDED
