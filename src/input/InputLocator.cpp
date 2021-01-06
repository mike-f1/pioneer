#include "InputLocator.h"

Input *InputLocator::s_input = nullptr;

void InputLocator::provideInput(Input *input)
{
	s_input = input;
}
