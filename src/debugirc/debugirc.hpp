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

	typedef boost::shared_ptr<std::string> InfoMessage;
	typedef std::deque<InfoMessage> InfoMessageQueue;

	class InfoParticipant
	{
	public:
		virtual ~InfoParticipant() {}
		virtual void Deliver(const InfoMessage& msg) = 0;
	};

	typedef boost::shared_ptr<InfoParticipant> InfoParticipantPtr;

	//----------------------------------------------------------------------

	class InfoBridge
	{
	public:
		void Join(InfoParticipantPtr participant)
		{
			participants_.insert(participant);
		}

		void Leave(InfoParticipantPtr participant)
		{
			participants_.erase(participant);
		}

		void Deliver(const std::string & msg)
		{
			InfoMessage info(new std::string(msg));
			std::for_each(participants_.begin(), participants_.end(),
					boost::bind(&InfoParticipant::Deliver, _1, boost::ref(info)));
		}

	private:
		std::set<InfoParticipantPtr> participants_;
	};

	//----------------------------------------------------------------------

	class Session
		: public InfoParticipant,
			public boost::enable_shared_from_this<Session>
	{
	public:
		Session(boost::asio::io_service& io_service, InfoBridge& room)
			: socket_(io_service),
				bridge_(room)
		{
		}

		tcp::socket & GetSocket()
		{
			return socket_;
		}

		void Start()
		{
			// TODO: move Join after reg
			bridge_.Join(shared_from_this());
			boost::asio::async_read_until(socket_, buffer_, '\n',
				boost::bind(&Session::HandleRead, this,
								boost::asio::placeholders::error));
		}

		void Deliver(const InfoMessage& msg)
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
						boost::bind(&Session::HandleWrite, shared_from_this(),
							boost::asio::placeholders::error));
			}
		}

		void HandleRead(const boost::system::error_code& error)
		{
			if (!error)
			{
				std::istream is(&buffer_);
				std::string line;
				std::getline(is, line);
				std::cout<<"<<["<<line<<"]\n";
				boost::asio::async_read_until(socket_, buffer_, '\n',
					boost::bind(&Session::HandleRead, this,
								boost::asio::placeholders::error));
			}
			else
			{
				bridge_.Leave(shared_from_this());
			}
		}

		void HandleWrite(const boost::system::error_code& error)
		{
			if (!error)
			{
				write_msgs_.pop_front();
				if (!write_msgs_.empty())
				{
					boost::asio::async_write(socket_,
							boost::asio::buffer(write_msgs_.front()->c_str(),
								write_msgs_.front()->length()),
							boost::bind(&Session::HandleWrite, shared_from_this(),
								boost::asio::placeholders::error));
				}
			}
			else
			{
				bridge_.Leave(shared_from_this());
			}
		}

	private:
		tcp::socket socket_;
		InfoBridge& bridge_;
		boost::asio::streambuf buffer_;
		InfoMessageQueue write_msgs_;
	};

	typedef boost::shared_ptr<Session> SessionPtr;

	//----------------------------------------------------------------------

	class Server
	{
	public:
		Server(boost::asio::io_service& io_service,
				const tcp::endpoint& endpoint)
			: io_service_(io_service),
				acceptor_(io_service, endpoint)
		{
			SessionPtr new_session(new Session(io_service_, bridge_));
			acceptor_.async_accept(new_session->GetSocket(),
					boost::bind(&Server::HandleAccept, this, new_session,
						boost::asio::placeholders::error));
		}

		void HandleAccept(SessionPtr current_Session,
				const boost::system::error_code& error)
		{
			if (!error)
			{
				current_Session->Start();
				SessionPtr new_session(new Session(io_service_, bridge_));
				acceptor_.async_accept(new_session->GetSocket(),
						boost::bind(&Server::HandleAccept, this, new_session,
							boost::asio::placeholders::error));
			}
		}

	private:
		boost::asio::io_service& io_service_;
		tcp::acceptor acceptor_;
		InfoBridge bridge_;
	};

} // namespace debugirc
