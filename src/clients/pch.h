/*
	Project			 : Falcon.Peregrine. Copyright(c) Pooya Eimandar (http://PooyaEimandar.com) . All rights reserved.
	Source			 : Please direct any bug to https://github.com/PooyaEimandar/Falcon.Peregrine/issues
	Website			 : http://WolfSource.io
	Name			 : pch.h
	Description		 : The pre-compiled header
	Comment          :
*/

#include <GLFW/glfw3.h>
#include <wolf.h>
#include <vector>
#include <string>
#include <mutex>
#include <condition_variable>
#include <w_window.h>
#include <glm_extension.h>
#include <w_timer_callback.h>
#include <w_media_core.h>
#include <w_network.h>
#include <asio.hpp>
#include <w_thread_pool.h>
#include <tbb/concurrent_queue.h>
#include <msgpack.hpp>
#include "../common/input.h"
#include "../common/version.h"
#include "../common/message.h"