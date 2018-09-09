#include "pch.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define SERVER_IP		"172.16.105.75"
#define SERVER_PORT		8080

using namespace wolf::system;
using namespace wolf::framework;

static w_timer_callback TimerCallBack;
static void glfw_error_callback(int error, const char* description)
{
	wolf::logger.error("Error {}:{}", error, description);
}

static glm::vec4 Clear_Color_From = glm::vec4(0.211f, 0.223f, 0.251f, 1.0f);
static glm::vec4 Clear_Color_To = glm::vec4(1.0f, 0.537f, 0.0117f, 1.0f);
static tbb::concurrent_queue<input> inputs;
static bool is_connected = false;
static bool fp_exit = false;

static void wait_for_connection_colors(
	_Inout_ glm::vec4& _clear_color, 
	_Inout_ float& pSign, 
	_Inout_ bool& pRReached, 
	_Inout_ bool& pGReached, 
	_Inout_ bool& pBReached)
{
	_clear_color.r += pSign * 0.004f;
	_clear_color.g += pSign * 0.004f;
	_clear_color.b += pSign * 0.004f;
	if (pSign > 0)
	{
		if (_clear_color.r >= Clear_Color_To.r) { _clear_color.r = Clear_Color_To.r; pRReached = true; }
		if (_clear_color.g >= Clear_Color_To.g) { _clear_color.g = Clear_Color_To.g; pGReached = true; }
		if (_clear_color.b >= Clear_Color_To.b) { _clear_color.b = Clear_Color_To.b; pBReached = true; }
	}
	else
	{
		if (_clear_color.r <= Clear_Color_From.r) { _clear_color.r = Clear_Color_From.r; pRReached = true; }
		if (_clear_color.g <= Clear_Color_From.g) { _clear_color.g = Clear_Color_From.g; pGReached = true; }
		if (_clear_color.b <= Clear_Color_From.b) { _clear_color.b = Clear_Color_From.b; pBReached = true; }
	}

	if (pRReached && pGReached && pBReached)
	{
		//change after 0.5 sec
		TimerCallBack.do_sync(500, [&]()
		{
			pSign *= -1;
			pRReached = false;
			pGReached = false;
			pBReached = false;
		});
	}
}

static void keyboard_callback(_In_ GLFWwindow* pWindow, _In_ int pKey, _In_ int pScanCode, _In_ int pAction, _In_ int pMods)
{
	if (!is_connected || fp_exit) return;
	
	input _input;
	_input.key_scancode = pScanCode;
	_input.key_action = pAction;
	inputs.push(_input);

	//wolf::logger.write("{} : {}" , pScanCode, pAction);
}

//Entry point of program 
WOLF_MAIN()
{
	std::ostringstream _obuffer;

	_obuffer <<
#ifdef __WIN32
		"falcon.peregrine.client.Win32 v."
#elif defined(__APPLE__)
		"falcon.peregrine.client.osx v."
#else
		"falcon.peregrine.client.? v."
#endif
		<< FALCON_PEREGRINE_MAJOR_VERSION << "." << FALCON_PEREGRINE_MINOR_VERSION << "." << FALCON_PEREGRINE_PATCH_VERSION << "." << FALCON_PEREGRINE_DEBUG_VERSION;

	auto _current_dir = wolf::system::io::get_current_directoryW();

	w_logger_config _logger_config;
	_logger_config.app_name = wolf::system::convert::string_to_wstring(_obuffer.str());
	_logger_config.log_path = wolf::system::io::get_current_directoryW();
	_logger_config.flush_level = false;
#ifdef __WIN32
	_logger_config.log_to_std_out = false;
#else
	_logger_config.log_to_std_out = true;
#endif

	wolf::logger.initialize(_logger_config);

	// Setup window
	glfwSetErrorCallback(glfw_error_callback);
	if (!glfwInit())
	{
		//clear buffer
		_obuffer.str("");
		_obuffer.clear();
		wolf::logger.error("could not initialize window.");

		return EXIT_FAILURE;
	}

	uint32_t _width = 1280, _height = 720;
	GLFWwindow* _window = glfwCreateWindow(_width, _height, _obuffer.str().c_str(), NULL, NULL);

	if (!_window)
	{
		wolf::logger.error("could not create window.");
		return EXIT_FAILURE;
	}

	//integrate gl and window
	glfwMakeContextCurrent(_window);
	//Enable vsync
	glfwSwapInterval(1);

	//set input callbacks
	glfwSetKeyCallback(_window, keyboard_callback);

	//set clear colors
	glm::vec4 _clear_color = Clear_Color_From;
	float _sign = 1;
	bool _r_reached = false, _g_reached = false, _b_reached = false;

	//we will allocate memory for pixels when stream created 
	GLuint _texture[1] = { 0 };
	size_t _size = 0;
	std::atomic<bool> _need_to_recreate_texture;
	_need_to_recreate_texture.store(false);
	std::atomic<uint8_t*> _pixels;
	_pixels.store(nullptr);

	std::condition_variable _cv;
	std::mutex _mutex;
	message _message;

	//run two threads, one for input on port 8555 and the other one for streaming on port 8554
	w_thread_pool _thread_pool;
	//allocate 3 threads
	_thread_pool.allocate(3);
	//first thread used for sending io
	_thread_pool.add_job_for_thread(0, [&]()
	{
		//wait for connection server
		{
			//TEST comment following
			std::unique_lock<std::mutex> _lock(_mutex);
			_cv.wait(_lock);
		}

		//TEST
		//_message.streamer_ip = "192.168.9.129";
		if (_message.streamer_ip.empty())
		{
			wolf::logger.error("missing streamer ip information from server");
			return;
		}

		//a thread for sending inputs on port 8555
		w_network _input_puller;
		w_signal<void(const int&)> _on_input_bind_established;
		_on_input_bind_established += [&](const int& pSocketID)
		{
			wolf::logger.write("input pusher launched with socket ID: {}", pSocketID);

			size_t _len = 0;
			std::stringstream _sbuffer;
			while (!fp_exit)
			{
				_sbuffer.str("");
				_sbuffer.clear();

				if (inputs.unsafe_size())
				{
					input _input;
					if (inputs.try_pop(_input))
					{
						_input.has_data = true;
					}
					msgpack::pack(_sbuffer, _input);
					_len = _sbuffer.str().size() + 1;//1 for "\0"
					if (w_network::send(pSocketID, _sbuffer.str().c_str(), _len) != _len)
					{
						wolf::logger.error("count of sent bytes not equal to count of message bytes");
					}
				}
				w_thread::sleep_current_thread(50);
			}
		};
		//connect to streamer ip
		_input_puller.setup_one_way_pusher(("tcp://" + _message.streamer_ip + ":8555").c_str(), _on_input_bind_established);
		_input_puller.release();
	});
	//the second thread used for streaming
	_thread_pool.add_job_for_thread(1, [&]()
	{
		//wait for connection server
		{
			//TEST comment following
			std::unique_lock<std::mutex> _lock(_mutex);
			_cv.wait(_lock);
		}
		//TEST
		//_message.client_ip = "172.16.105.75";
		if (_message.client_ip.empty())
		{
			wolf::logger.error("missing client ip information from server");
			return;
		}

		//a thread for streaming on port 8554
		w_media_core::register_all(true);

		w_media_core _media_core;
		w_signal<void(const w_media_core::w_stream_connection_info&)> _on_connection_established;
		w_signal<void(const w_media_core::w_stream_frame_info&)> _on_getting_stream_frame_buffer;
		w_signal<void(const char*)> _on_connection_lost;
		w_signal<void(const char*)> _on_connection_closed;

		_on_connection_established += [&](const w_media_core::w_stream_connection_info& pConnectionInfo)->void
		{
			is_connected = true;
			wolf::logger.write("connection {} established", pConnectionInfo.url);
		};
		_on_connection_lost += [&](const char* pURL)->void
		{
			is_connected = false;
			wolf::logger.write("connection {} lost", pURL);
		};
		_on_connection_closed += [&](const char* pURL)->void
		{
			is_connected = false;
			fp_exit = true;
			wolf::logger.write("connection {} closed", pURL);
			glfwDestroyWindow(_window);
		};

		_on_getting_stream_frame_buffer += [&](const w_media_core::w_stream_frame_info& pFrameInfo)->void
		{
			if (!fp_exit && pFrameInfo.pixels)
			{
				_width = pFrameInfo.width;
				_height = pFrameInfo.height;
				auto _stride = pFrameInfo.stride;

				auto _stream_size = _width * _height * _stride * sizeof(uint8_t);
				if (_size != _stream_size)
				{
					_size = _stream_size;
					if (_pixels)
					{
						free(_pixels);
					}

					_pixels = (uint8_t*)malloc(_stream_size);
					if (!_pixels)
					{
						fp_exit = true;
						return;
					}
					_need_to_recreate_texture = true;

					wolf::logger.write("buffer need to be resize");
				}
				if (_pixels)
				{
					std::memcpy(&_pixels[0], &pFrameInfo.pixels[0], _stream_size);
				}
			}
		};

		//get current ip of this machine from server
		_media_core.open_stream_client(
			("rtsp://" + _message.client_ip + ":8554/live.sdp").c_str(),
			"udp",
			"rtsp",
			AV_CODEC_ID_H264,
			AV_PIX_FMT_YUV420P,
			_width,
			_height,
			15000,//15 sec for timeout
			_on_connection_established,
			_on_getting_stream_frame_buffer,
			_on_connection_lost,
			_on_connection_closed);
		//release media core
		_media_core.release();
		w_media_core::shut_down();
	});
	//the third thread used for communicating to server
	_thread_pool.add_job_for_thread(2, [&]()
	{
		//TEST
		//return;

		//sleep for 1 sec, cause we need to make sure the other threads started
		w_thread::sleep_current_thread(1000);

		//first connect to server and get the necessary information
		char _buf[128];
		using asio::ip::tcp;
		asio::io_service _io_service;
		tcp::socket _socket(_io_service);
		bool _socket_is_available = true;
		try
		{
			_socket.connect(tcp::endpoint(asio::ip::address::from_string(SERVER_IP), SERVER_PORT));
		}
		catch (...)
		{
			_socket_is_available = false;
			wolf::logger.error("falcon peregrine could not fly for you now, try again later! error : socket not connected");
		}

		if (_socket_is_available)
		{
			for (;;)
			{
				asio::error_code _error;
				size_t _len = _socket.read_some(asio::buffer(_buf), _error);
				if (_error)
				{
					if (_error == asio::error::eof)
					{
						break; // connection closed cleanly by peer.
					}
					else
					{
						wolf::logger.write("falcon peregrine could not fly for you now, try again later! error : {}", _error.message());
					}
				}
				else if (_len > 0)
				{
					//first read current ip and streamer ip
					auto _msg = msgpack::unpack(_buf, _len);
					_msg.get().convert(_message);

					//notify other threads
					_cv.notify_all();
					break;
				}
			}
		}
	});

	// Main loop
	int _frames = 0;
	double _last_time = 0, _timeout = 0;
	while (!glfwWindowShouldClose(_window))
	{
		double _current_time = glfwGetTime();
		auto _delta_time = _current_time - _last_time;
		_frames++;
		if (_delta_time >= 1.0)
		{
			_obuffer.str("");
			_obuffer.clear();
			_obuffer << "falcon.peregrine.client.Win32 v." << FALCON_PEREGRINE_MAJOR_VERSION << "." << FALCON_PEREGRINE_MINOR_VERSION << "." << FALCON_PEREGRINE_PATCH_VERSION << "." << FALCON_PEREGRINE_DEBUG_VERSION << " fps:" << _frames;
			glfwSetWindowTitle(_window, _obuffer.str().c_str());
			_frames = 0;
			_last_time += 1.0f;

			if (is_connected)
			{
				_timeout = 0;
			}
			else
			{
				_timeout += 1.0f;
				if (_timeout > 60)//15
				{
					wolf::logger.error("streammer timeout");
					fp_exit = true;
				}
			}
		}		

		if (fp_exit) break;

		if (_need_to_recreate_texture)
		{
			if (_texture[0])
			{
				glDeleteTextures(1, &_texture[0]);
			}
			glGenTextures(1, &_texture[0]);
			glBindTexture(GL_TEXTURE_2D, _texture[0]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, _width, _height, 0, GL_RGB, GL_UNSIGNED_BYTE, _pixels);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // Linear Filtering
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // Linear Filtering
			_need_to_recreate_texture = false;
		}

		// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
		// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
		// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
		// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
		glfwPollEvents();

		if (!is_connected)
		{
			wait_for_connection_colors(_clear_color, _sign, _r_reached, _g_reached, _b_reached);
		}

		// Rendering
		int display_w, display_h;
		glfwGetFramebufferSize(_window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		glLoadIdentity();
		glClearColor(_clear_color.x, _clear_color.y, _clear_color.z, _clear_color.w);
		glClear(GL_COLOR_BUFFER_BIT);

		if (is_connected && _texture[0])
		{
			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, _texture[0]);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, _width, _height, GL_RGB, GL_UNSIGNED_BYTE, &_pixels[0]);

			//begin drawing
			glBegin(GL_QUADS);

			//top left
			glVertex3f(-1.0f, 1.0f, 0.0f);
			glTexCoord2f(1.0f, 0.0f);

			//top right
			glVertex3f(1.0f, 1.0f, 0.0f);
			glTexCoord2f(1.0f, 1.0f);

			//bottom right
			glVertex3f(1.0f, -1.0f, 0.0f);
			glTexCoord2f(0.0f, 1.0f);

			//bottom left
			glVertex3f(-1.0f, -1.0f, 0.0f);
			glTexCoord2f(0.0f, 0.0f);

			glEnd();
		}

		glfwSwapBuffers(_window);
	}

	//release buffer
	_obuffer.str("");
	_obuffer.clear();
	//release threads
	_thread_pool.release();
	inputs.clear();

	if (_pixels)
	{
		free(_pixels);
		_pixels = nullptr;
	}
	if (_texture[0])
	{
		glDeleteTextures(1, &_texture[0]);
	}
	//glfwDestroyWindow(_window);
	glfwTerminate();

	wolf::release_heap_data();

	//wait 1 sec
	w_thread::sleep_current_thread(1000);

	return EXIT_SUCCESS;
}
