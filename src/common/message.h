/*
	Project			 : Wolf Engine. Copyright(c) Pooya Eimandar (http://WolfEngine.app) . All rights reserved.
	Source			 : Please direct any bug to https://github.com/PooyaEimandar/Wolf.Engine/issues
	Name			 : message.h
	Description		 : the structure of message shared between server and clients
	Comment          :
*/

#if _MSC_VER > 1000
#pragma once
#endif

#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#include <string>
#include <msgpack.hpp>

struct message
{
	bool error = false;
	std::string client_ip = "";
	std::string streamer_ip = "";
	
	MSGPACK_DEFINE(error, client_ip, streamer_ip);
};

#endif // __MESSAGE_H__
