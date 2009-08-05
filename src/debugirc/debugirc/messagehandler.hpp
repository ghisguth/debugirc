/* messagehandler.hpp
 * This file is a part of debugirc library
 * Copyright (c) debugirc authors (see file `COPYRIGHT` for the license)
 */

#pragma once

#include <string>
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>

namespace debugirc
{
	class MessageHandler
	{
	public:
		typedef boost::function<void (const std::string &)> SendCallback;
		virtual ~MessageHandler() {}
		virtual void Handle(const std::string & username, const std::string & channel, const std::string & data, SendCallback send_callback)
		{
		}
	};

	typedef boost::shared_ptr<MessageHandler> MessageHandlerPtr;
} // namespace debugirc
