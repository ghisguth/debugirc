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
#include <boost/lexical_cast.hpp>
#include <signal.h>
#include "shutdown_manager.hpp"
#include "debugirc/debugirc.hpp"

void DebugThread(debugirc::Server &  srv)
{
	try
	{
		while(true)
		{
			boost::this_thread::interruption_point();
			boost::this_thread::sleep(boost::posix_time::milliseconds(rand()%500 + 500));
			if((rand()%1000) < 300)
				srv.GetChat().DeliverChannel("#system", boost::lexical_cast<std::string>(rand()));
			else
				srv.GetChat().DeliverChannel("#debug", boost::lexical_cast<std::string>(rand()));
		}
	}
	catch(boost::thread_interrupted const&)
  {}
}

class TestMessageHandler
	: public debugirc::MessageHandler
{
public:
	TestMessageHandler(debugirc::Server &  server)
		: server_(server)
	{}
	virtual void Handle(const std::string & username, const std::string & channel, const std::string & data, SendCallback send_callback)
	{
		if(channel == "#system")
		{
			std::stringstream strstr;
			strstr<<"system command "<<data;
			send_callback(strstr.str());
		}
		else if(channel.length() > 0 &&  channel[0] == '#' && data.find('\n') == std::string::npos)
		{
			std::stringstream strstr;
			strstr<<username<<" says "<<data<<" on channel "<<channel;
			server_.GetChat().DeliverChannel(channel, strstr.str());
		}
	}
private:
	debugirc::Server & server_;
};

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
		debugirc::Server s(io_service, endpoint);

		s.GetChat().AddChannel("#system", "System channel");
		s.GetChat().SetAutoJoin("#system");
		s.GetChat().AddChannel("#debug", "DEBUG");
		s.GetChat().AddChannel("#test", "Test  CHANNEL");
		s.GetChat().AddChannel("#test2", "TEST2");
		s.GetChat().SetMessageHandler(debugirc::MessageHandlerPtr(new TestMessageHandler(s)));

		boost::thread t(boost::bind(&boost::asio::io_service::run, &io_service));
		boost::thread_group t2;
		for(int i = 0; i < 32; ++i)
			t2.create_thread(boost::bind(&DebugThread, boost::ref(s)));
		main_shutdown_manager.wait();
		t2.interrupt_all();
		t2.join_all();
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

