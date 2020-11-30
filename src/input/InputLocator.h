#ifndef INPUTLOCATOR_H
#define INPUTLOCATOR_H

class Input;

class InputLocator
{
public:
	InputLocator() = delete;

	static Input *getInput() { return s_input; };
	static void provideInput(Input *input);

private:
	static Input *s_input;
};

#endif // INPUTLOCATOR_H
