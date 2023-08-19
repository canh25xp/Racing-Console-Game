#pragma once
#include <iostream>
#include "ConsoleGameEngine.h"

class Game : public cge::ConsoleGameEngine {
public:
	Game();
protected:
	bool OnUserCreate() override;
	bool OnUserUpdate(float fElapsedTime) override;
};