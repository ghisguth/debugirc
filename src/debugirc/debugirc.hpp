/* debugirc.hpp
 * This file is a part of debugirc library
 * Copyright (c) debugirc authors (see file `COPYRIGHT` for the license)
 */

#pragma once

#include "core/common.hpp"
#include <set>
#include <deque>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>

namespace debugirc
{
	using boost::asio::ip::tcp;

	typedef boost::shared_ptr<std::string> info_message;
	typedef std::deque<info_message> info_message_queue;

	class info_participant
	{
	public:
		virtual ~info_participant() {}
		virtual void deliver(const info_message& msg) = 0;
	};

	typedef boost::shared_ptr<info_participant> info_participant_ptr;

	//----------------------------------------------------------------------

	class info_bridge
	{
	public:
		void join(info_participant_ptr participant)
		{
			participants_.insert(participant);
		}

		void leave(info_participant_ptr participant)
		{
			participants_.erase(participant);
		}

		void deliver(const std::string & msg)
		{
			info_message info(new std::string(msg));
			std::for_each(participants_.begin(), participants_.end(),
					boost::bind(&info_participant::deliver, _1, boost::ref(info)));
		}

	private:
		std::set<info_participant_ptr> participants_;
	};

	//----------------------------------------------------------------------

	class session
		: public info_participant,
			public boost::enable_shared_from_this<session>
	{
	public:
		session(boost::asio::io_service& io_service, info_bridge& room)
			: socket_(io_service),
				bridge_(room)
		{
		}

		tcp::socket& socket()
		{
			return socket_;
		}

		void start()
		{
			bridge_.join(shared_from_this());
			boost::asio::async_read_until(socket_, buffer_, '\n',
				boost::bind(&session::handle_read, this,
								boost::asio::placeholders::error));
		}

		void deliver(const info_message& msg)
		{
			if(!msg || msg->empty())
				return;
			bool write_in_progress = !write_msgs_.empty();
			write_msgs_.push_back(msg);
			if (!write_in_progress)
			{
				boost::asio::async_write(socket_,
						boost::asio::buffer(write_msgs_.front()->c_str(),
							write_msgs_.front()->length()),
						boost::bind(&session::handle_write, shared_from_this(),
							boost::asio::placeholders::error));
			}
		}

		void handle_read(const boost::system::error_code& error)
		{
			if (!error)
			{
				std::istream is(&buffer_);
				std::string line;
				std::getline(is, line);
				std::cout<<"<<["<<line<<"]\n";
				boost::asio::async_read_until(socket_, buffer_, '\n',
					boost::bind(&session::handle_read, this,
								boost::asio::placeholders::error));
			}
			else
			{
				bridge_.leave(shared_from_this());
			}
		}

		void handle_write(const boost::system::error_code& error)
		{
			if (!error)
			{
				write_msgs_.pop_front();
				if (!write_msgs_.empty())
				{
					boost::asio::async_write(socket_,
							boost::asio::buffer(write_msgs_.front()->c_str(),
								write_msgs_.front()->length()),
							boost::bind(&session::handle_write, shared_from_this(),
								boost::asio::placeholders::error));
				}
			}
			else
			{
				bridge_.leave(shared_from_this());
			}
		}

	private:
		tcp::socket socket_;
		info_bridge& bridge_;
		boost::asio::streambuf buffer_;
		info_message_queue write_msgs_;
	};

	typedef boost::shared_ptr<session> session_ptr;

	//----------------------------------------------------------------------

	class server
	{
	public:
		server(boost::asio::io_service& io_service,
				const tcp::endpoint& endpoint)
			: io_service_(io_service),
				acceptor_(io_service, endpoint)
		{
			session_ptr new_session(new session(io_service_, bridge_));
			acceptor_.async_accept(new_session->socket(),
					boost::bind(&server::handle_accept, this, new_session,
						boost::asio::placeholders::error));
		}

		void handle_accept(session_ptr current_session,
				const boost::system::error_code& error)
		{
			if (!error)
			{
				current_session->start();
				session_ptr new_session(new session(io_service_, bridge_));
				acceptor_.async_accept(new_session->socket(),
						boost::bind(&server::handle_accept, this, new_session,
							boost::asio::placeholders::error));
			}
		}

	private:
		boost::asio::io_service& io_service_;
		tcp::acceptor acceptor_;
		info_bridge bridge_;
	};

} // namespace debugirc
