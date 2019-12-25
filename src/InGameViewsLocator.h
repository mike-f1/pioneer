#ifndef INGAMEVIEWSLOCATOR_H
#define INGAMEVIEWSLOCATOR_H

#include <memory>
#include "JsonFwd.h"

class InGameViews;

class InGameViewsLocator
{
public:
	InGameViewsLocator() = delete;

	static InGameViews *getInGameViews();

	// These are needed by Game(Mono)State, which will use below methods
	// to set s_InGameViews
	static void NewInGameViews(InGameViews *newInGameViews);
	static void SaveInGameViews(Json &rootNode);


protected:

private:
	static std::unique_ptr<InGameViews> s_inGameViews;
};

#endif // INGAMEVIEWSLOCATOR_H
