/* server.hpp
 * This file is a part of debugirc library
 * Copyright (c) debugirc authors (see file `COPYRIGHT` for the license)
 */

#pragma once

#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/asio.hpp>
#include "types.hpp"
#include "participant.hpp"
#include "chat.hpp"
#include "session.hpp"

namespace debugirc
{
	using boost::asio::ip::tcp;
	class Server
	{
	public:
		Server(boost::asio::io_service& io_service,
				const tcp::endpoint& endpoint)
			: io_service_(io_service),
				acceptor_(io_service, endpoint)
		{
			SessionPtr new_session(new Session(io_service_, chat_));
			acceptor_.async_accept(new_session->GetSocket(),
					boost::bind(&Server::HandleAccept, this, new_session,
						boost::asio::placeholders::error));
		}

		void HandleAccept(SessionPtr current_session,
				const boost::system::error_code& error)
		{
			if (!error)
			{
				current_session->Start();
				SessionPtr new_session(new Session(io_service_, chat_));
				acceptor_.async_accept(new_session->GetSocket(),
						boost::bind(&Server::HandleAccept, this, new_session,
							boost::asio::placeholders::error));
			}
		}

		Chat & GetChat() { return chat_; }

	private:
		boost::asio::io_service& io_service_;
		tcp::acceptor acceptor_;
		Chat chat_;
	};

} // namespace debugirc
