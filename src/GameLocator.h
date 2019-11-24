#ifndef GAMELOCATOR_H
#define GAMELOCATOR_H

class Game;

class GameLocator
{
public:
	GameLocator() = delete;

	static Game *getGame() { return s_game; };
	static void provideGame(Game *game);

private:
	static Game *s_game;
};

#endif // GAMELOCATOR_H
