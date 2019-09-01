#include <stdio.h>

extern "C" void Menu();
extern void SetupFont();

int main()
{
	SetupFont();
	Menu();
	return 0;
}