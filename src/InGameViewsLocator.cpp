#include "InGameViewsLocator.h"

#include "Json.h"
#include "InGameViews.h"

std::unique_ptr<InGameViews> InGameViewsLocator::s_inGameViews;

//static
InGameViews *InGameViewsLocator::getInGameViews()
{
	return s_inGameViews.get();
}

//static
void InGameViewsLocator::NewInGameViews(InGameViews *newInGameViews)
{
	s_inGameViews.reset(newInGameViews);
}

//static
void InGameViewsLocator::SaveInGameViews(Json &rootNode)
{
	s_inGameViews->SaveToJson(rootNode);
}
