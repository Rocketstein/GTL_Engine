#pragma once
#include "InputSystem.h"
#include "iostream"

class InputTest
{
public:
	// Update 루프에서 이 함수를 호출해서 키 입력 테스트
	static void Update()
	{
		if (InputSystem::GetKeyDown('Z'))
			std::cout << "Z down\n";
		if (InputSystem::GetKeyUp('Z'))
			std::cout << "Z up\n";
		if (InputSystem::GetKey('Z'))
			std::cout << "Z stay\n";
	}
};