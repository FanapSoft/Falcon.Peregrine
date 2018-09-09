/*
	Project			 : Wolf Engine. Copyright(c) Pooya Eimandar (http://WolfEngine.app) . All rights reserved.
	Source			 : Please direct any bug to https://github.com/PooyaEimandar/Wolf.Engine/issues
	Name			 : input.h
	Description		 : The input header
	Comment          :
*/

#if _MSC_VER > 1000
#pragma once
#endif

#ifndef __INPUT_H__
#define __INPUT_H__

#include <msgpack.hpp>

struct input
{
public:
	bool	 has_data = false;
	int		 mouse_pos_x = -1;
	int		 mouse_pos_y = -1;
	int		 key_scancode = -1;
	int		 key_action = -1;

	void reset()
	{
		this->mouse_pos_x = -1;
		this->mouse_pos_y = -1;
		this->key_scancode = -1;
		this->key_action = -1;//0 released, 1 pressed, 2 repeat 
	}

	MSGPACK_DEFINE(has_data, mouse_pos_x, mouse_pos_y, key_scancode, key_action);
};

#endif // __INPUT_H__
