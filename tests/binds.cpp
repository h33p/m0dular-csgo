#include <stdio.h>
#include "../core/settings.h"
#include "../core/binds.h"

BindKey toggleKeyBhop(BindManager::sharedInstance->bindList[0]->AllocKeyBind(), BindMode::TOGGLE);
BindKey holdKeyBhop(BindManager::sharedInstance->bindList[0]->AllocKeyBind());
BindKey holdKeyBhop2(BindManager::sharedInstance->bindList[0]->AllocKeyBind());

int main()
{
	toggleKeyBhop.InitializePointer(true);
	holdKeyBhop.InitializePointer(false);
	holdKeyBhop2.InitializePointer(true);

	//Test the basic sequence of different (hold) keys getting pressed up and down
	holdKeyBhop.HandleKeyPress(true);
	assert(!Settings::bunnyhopping);
	holdKeyBhop2.HandleKeyPress(true);
	assert(Settings::bunnyhopping);
	holdKeyBhop2.HandleKeyPress(false);
	assert(!Settings::bunnyhopping);
	holdKeyBhop2.HandleKeyPress(true);
	assert(Settings::bunnyhopping);

	//Test for a key getting released from the middle of the stack
	toggleKeyBhop.HandleKeyPress(true);
	toggleKeyBhop.HandleKeyPress(false);
	assert(Settings::bunnyhopping);
	holdKeyBhop2.HandleKeyPress(false);
	assert(Settings::bunnyhopping);
	toggleKeyBhop.HandleKeyPress(true);
	toggleKeyBhop.HandleKeyPress(false);
	assert(!Settings::bunnyhopping);

	//Test again for the last pressed key changing the value of the keybind
	holdKeyBhop2.HandleKeyPress(true);
	assert(Settings::bunnyhopping);
	toggleKeyBhop.HandleKeyPress(true);
	toggleKeyBhop.HandleKeyPress(false);
	assert(Settings::bunnyhopping);
	holdKeyBhop.HandleKeyPress(false);
	assert(Settings::bunnyhopping);
	holdKeyBhop.HandleKeyPress(true);
	assert(!Settings::bunnyhopping);

	return 0;
}
