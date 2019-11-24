#include "GameLocator.h"

Game *GameLocator::s_game = nullptr;

void GameLocator::provideGame(Game *game)
{
	s_game = game;
}
