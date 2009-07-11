/* main.cpp
 * This file is a part of debugirc library
 * Copyright (c) debugirc authors (see file `COPYRIGHT` for the license)
 */

#include <iostream>
#include <boost/array.hpp>
#include <boost/thread.hpp>
#include <boost/thread/barrier.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <signal.h>
#include "core/common.hpp"
#include "core/shutdown_manager.hpp"
#include "debugirc/debugirc.hpp"

int main(int argc, char** argv)
{
	srand(time(0));
	try
	{
#if defined(__WIN32__) || defined(WIN32) || defined(_WIN32)
		SetConsoleCtrlHandler(console_ctrl_handler, TRUE);
#else
		signal(SIGINT, handle_signal);
#endif
		if (argc != 2)
		{
			std::cerr << "Usage: debugircd <port>\n";
			return 1;
		}

		boost::asio::io_service io_service;
		boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), std::atoi(argv[1]));
		debugirc::server s(io_service, endpoint);

		boost::thread t(boost::bind(&boost::asio::io_service::run, &io_service));
		main_shutdown_manager.wait();
		io_service.stop();
	}
	catch(std::exception & e)
	{
		std::cerr<<"unhandled std::exception "<<e.what()<<"\n";
		std::abort();
	}
	catch(...)
	{
		std::cerr<<"unhandled exception\n";
		std::abort();
	}
	return 0;
}

