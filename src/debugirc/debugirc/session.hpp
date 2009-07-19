/* session.hpp
 * This file is a part of debugirc library
 * Copyright (c) debugirc authors (see file `COPYRIGHT` for the license)
 */

#pragma once

#include <set>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>
#include <boost/unordered_map.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/locks.hpp>
#include "types.hpp"
#include "participant.hpp"
#include "chat.hpp"

namespace debugirc
{
	using boost::asio::ip::tcp;

	class Session
		: public ChatParticipant,
			public boost::enable_shared_from_this<Session>
	{
	public:
		static const int PingInterval = 300; // 5 miniutes

		Session(boost::asio::io_service& io_service, Chat& room)
			: socket_(io_service),
				bridge_(room),
				initialized_(false),
				authorized_(false),
				register_timeout_(io_service),
				connection_timeout_(io_service),
				closing_connection_(false),
				ping_sent_(false)
		{
			registration_handlers_["NICK"] = &Session::MessageNick;
			registration_handlers_["PASS"] = &Session::MessagePass;
			registration_handlers_["USER"] = &Session::MessageUser;
			message_handlers_["MODE"] = &Session::MessageIgnore;
			message_handlers_["QUIT"] = &Session::MessageQuit;
			message_handlers_["PING"] = &Session::MessagePing;
			message_handlers_["JOIN"] = &Session::MessageJoin;
			message_handlers_["PART"] = &Session::MessagePart;
			message_handlers_["LIST"] = &Session::MessageList;
			message_handlers_["WHO"] = &Session::MessageWho;
			message_handlers_["PONG"] = &Session::MessagePong;
			message_handlers_["PRIVMSG"] = &Session::MessagePrivMsg;
			message_handlers_["NOTICE"] = &Session::MessageIgnore;
		}

		tcp::socket & GetSocket()
		{
			return socket_;
		}

		void Start()
		{
			initialized_ = true;
			register_timeout_.expires_from_now(boost::posix_time::seconds(5));
			register_timeout_.async_wait(boost::bind(&Session::HandleRegisterTimeout, shared_from_this(),
								boost::asio::placeholders::error));
			bridge_.Join(shared_from_this());
			boost::asio::async_read_until(socket_, buffer_, '\n',
				boost::bind(&Session::HandleRead, this,
								boost::asio::placeholders::error));
		}

		void Deliver(const std::string & msg)
		{
			if(msg.empty())
				return;
			ChatMessage info(new std::string(msg));
			Deliver(info);
		}

		void Deliver(const ChatMessage& msg)
		{
			if(!msg || msg->empty())
				return;
			boost::unique_lock<boost::mutex> lock(sync_);
			bool write_in_progress = !write_msgs_.empty();
			write_msgs_.push_back(msg);
			if (!write_in_progress)
			{
				WriteNextMessage();
			}
		}

	private:

		std::ostream & WriteServerHeader(std::ostream & ostr, const std::string & command_id)
		{
			ostr<<":"<<bridge_.GetServerName()<<" "<<command_id<<" "<<nick_<<" ";
			return ostr;
		}

		std::ostream & WriteServerHeaderNoNick(std::ostream & ostr, const std::string & command_id)
		{
			ostr<<":"<<bridge_.GetServerName()<<" "<<command_id<<" ";
			return ostr;
		}

		std::ostream & WriteUserHeaderNoNick(std::ostream & ostr, const std::string & command_id)
		{
			ostr<<":"<<nick_<<"!"<<nick_<<" "<<command_id<<" ";
			return ostr;
		}

		void Authorize()
		{
			if(bridge_.Authorize(nick_, password_))
			{
				//std::cerr<<"AUTH "<<nick_<<" success\n";
				authorized_ = true;
				register_timeout_.cancel();
				connection_timeout_.expires_from_now(boost::posix_time::seconds(PingInterval));
				connection_timeout_.async_wait(boost::bind(&Session::HandleConnectionTimeout, shared_from_this(),
								boost::asio::placeholders::error));
				std::stringstream strstr;
				WriteServerHeader(strstr, "001")<<":Hi "<<nick_<<"\n";
				WriteServerHeader(strstr, "002")<<":Your host is "<<bridge_.GetServerName()<<", running version 0.0.0\n";
				WriteServerHeader(strstr, "003")<<":This server was created 0\n";
				WriteServerHeader(strstr, "004")<<":"<<bridge_.GetServerName()<<" 0.0.0 - n\n";
				WriteServerHeader(strstr, "375")<<":- "<<bridge_.GetServerName()<<" "<<bridge_.GetMOTDStart()<<" -\n";
				WriteServerHeader(strstr, "372")<<":- "<<bridge_.GetMOTD()<<"\n";
				const std::string &  auto_join = bridge_.GetAutoJoin();
				if(!auto_join.empty())
				{
					if(bridge_.JoinChannel(auto_join, shared_from_this()))
					{
						active_channels_.insert(auto_join);
						WriteUserHeaderNoNick(strstr, "JOIN")<<auto_join<<" :"<<auto_join<<"\n";
					}
				}
				Deliver(strstr.str());
			}
			else
			{
				//std::cerr<<"!!!: auth failed\n";
				Cleanup();
			}
		}

		void MessageIgnore(const std::string & command_id, const std::string & data, std::string & answer)
		{}

		void MessageUnknown(const std::string & command_id, const std::string & data, std::string & answer)
		{
			std::stringstream strstr;
			WriteServerHeader(strstr, "421")<<command_id<<" :Command "<<command_id<<" is unknown or unsupported"<<"\n";
			answer = strstr.str();
		}

		void MessageNick(const std::string & command_id, const std::string & data, std::string & answer)
		{
			nick_ = data;
		}

		void MessagePass(const std::string & command_id, const std::string & data, std::string & answer)
		{
			password_ = data;
		}

		void MessageUser(const std::string & command_id, const std::string & data, std::string & answer)
		{
			Authorize();
		}

		void MessageQuit(const std::string & command_id, const std::string & data, std::string & answer)
		{
			//std::cerr<<"!!!: quit\n";
			Cleanup();
		}

		void MessagePing(const std::string & command_id, const std::string & data, std::string & answer)
		{
			std::stringstream strstr;
			WriteServerHeaderNoNick(strstr, "PONG")<<bridge_.GetServerName()<<" :"<<data<<"\n";
			answer = strstr.str();
			if(!ping_sent_)
			{
				connection_timeout_.expires_from_now(boost::posix_time::seconds(PingInterval));
				connection_timeout_.async_wait(boost::bind(&Session::HandleConnectionTimeout, shared_from_this(),
							boost::asio::placeholders::error));
			}
		}

		void MessageJoin(const std::string & command_id, const std::string & data, std::string & answer)
		{
			std::stringstream strstr;
			if(data.length() > 1 && data[0]=='#' && bridge_.JoinChannel(data, shared_from_this()))
			{
				WriteUserHeaderNoNick(strstr, "JOIN")<<data<<" :"<<data<<"\n";
				active_channels_.insert(data);
			}
			else
			{
				if(active_channels_.find(data) != active_channels_.end())
				{
					WriteUserHeaderNoNick(strstr, "JOIN")<<data<<" :"<<data<<"\n";
				}
				else
				{
					strstr<<":"<<nick_<<" 403 "<<data<<" :No such channel\n";
				}
			}
			answer = strstr.str();
		}

		void MessagePart(const std::string & command_id, const std::string & data, std::string & answer)
		{
			std::stringstream strstr;
			std::string channel;
			std::string message;
			SplitChannelMessage(data, channel, message);
			if(!channel.empty())
			{
				WriteUserHeaderNoNick(strstr, "PART")<<data;
				if(!message.empty())
					strstr<<" :"<<data;
				strstr<<"\n";
				bridge_.LeaveChannel(channel, shared_from_this());
				active_channels_.erase(channel);
			}
			else
			{
				strstr<<":"<<nick_<<" 403 "<<data<<" :No such channel\n";
			}
			answer = strstr.str();
		}

		void MessageList(const std::string & command_id, const std::string & data, std::string & answer)
		{
			std::stringstream strstr;
			WriteServerHeader(strstr, "321")<<"Channel :Users  Name\n";
			ChannelListBuilder builder(strstr, bridge_.GetServerName(), nick_);
			bridge_.VisitChannels(&builder);
			WriteServerHeader(strstr, "323")<<":End of /LIST\n";
			answer = strstr.str();
		}

		void MessageWho(const std::string & command_id, const std::string & data, std::string & answer)
		{
			std::stringstream strstr;
			WriteServerHeader(strstr, "315")<<data<<" :End of /WHO list.\n";
			answer = strstr.str();
		}

		void MessagePong(const std::string & command_id, const std::string & data, std::string & answer)
		{
			std::stringstream strstr;
			if(ping_sent_)
			{
				ping_sent_ = false;
				connection_timeout_.expires_from_now(boost::posix_time::seconds(PingInterval));
				connection_timeout_.async_wait(boost::bind(&Session::HandleConnectionTimeout, shared_from_this(),
							boost::asio::placeholders::error));
			}
			answer = strstr.str();
		}

		void MessagePrivMsg(const std::string & command_id, const std::string & data, std::string & answer)
		{
			if(data.empty())
				return;
			MessageHandlerPtr handler = bridge_.GetMessageHandler();
			if(handler)
			{
				std::string channel;
				std::string message;
				SplitChannelMessage(data, channel, message);
				if(!channel.empty() && !message.empty())
				{
					std::string this_answer;
					handler->Handle(nick_, channel, message, this_answer);
					if(!this_answer.empty())
					{
						std::stringstream strstr;
						WriteServerHeaderNoNick(strstr, "PRIVMSG")<<channel<<" :"<<this_answer<<"\n";
						answer = strstr.str();
					}
				}
			}
		}

		void HandleCommand(const std::string & command_data)
		{
			if(command_data.empty())
				return;
			size_t pos = command_data.find(' ');
			std::string command = command_data.substr(0, pos);
			std::string data = pos == std::string::npos ? "" : command_data.substr(pos + 1);
			if(!authorized_)
			{
				//std::cout<<command<<" "<<data<<"\n";
				std::map<std::string, MessageHandler>::iterator it = registration_handlers_.find(command);
				std::string answer;
				if(it != registration_handlers_.end())
				{
					(this->*it->second)(command, data, answer);
				}
				else
				{
					MessageUnknown(command, data, answer);
				}
				Deliver(answer);
			}
			else
			{
				//std::cout<<":"<<nick_<<"!"<<nick_<<" "<<command<<" "<<data<<"\n";
				std::map<std::string, MessageHandler>::iterator it = message_handlers_.find(command);
				std::string answer;
				if(it != message_handlers_.end())
				{
					(this->*it->second)(command, data, answer);
				}
				else
				{
					MessageUnknown(command, data, answer);
				}
				Deliver(answer);
			}
		}

		void HandleRead(const boost::system::error_code& error)
		{
			if (!error)
			{
				std::istream is(&buffer_);
				std::string line;
				std::getline(is, line);
				if(line.length() > 0 && line[line.length()-1] == '\r')
					line.resize(line.length()-1);
				HandleCommand(line);
				if(socket_.is_open())
				{
					boost::asio::async_read_until(socket_, buffer_, '\n',
						boost::bind(&Session::HandleRead, this,
									boost::asio::placeholders::error));
				}
			}
			else
			{
				//std::cerr<<"!!!: read failed\n";
				Cleanup();
			}
		}

		void HandleWrite(const boost::system::error_code& error)
		{
			if (!error)
			{
				boost::unique_lock<boost::mutex> lock(sync_);
				write_msgs_.pop_front();
				WriteNextMessage();
			}
			else
			{
				//std::cerr<<"!!!: write failed\n";
				Cleanup();
			}
		}

		void HandleRegisterTimeout(const boost::system::error_code& error)
		{
			if (!error)
			{
				closing_connection_ = true;
				Deliver("ERROR: registration timeout\n");
			}
		}

		void HandleConnectionTimeout(const boost::system::error_code& error)
		{
			if (!error)
			{
				if(ping_sent_)
				{
					closing_connection_ = true;
					Deliver("ERROR: connection timeout\n");
				}
				else
				{
					ping_sent_ = true;
					connection_timeout_.expires_from_now(boost::posix_time::seconds(30));
					connection_timeout_.async_wait(boost::bind(&Session::HandleConnectionTimeout, shared_from_this(),
								boost::asio::placeholders::error));
					std::stringstream strstr;
					strstr<<"PING :"<<bridge_.GetServerName()<<"\n";
					Deliver(strstr.str());
				}
			}
		}

		void WriteNextMessage()
		{
			if (!write_msgs_.empty())
			{
				//std::cout<<(*write_msgs_.front());
				boost::asio::async_write(socket_,
						boost::asio::buffer(write_msgs_.front()->c_str(),
							write_msgs_.front()->length()),
						boost::bind(&Session::HandleWrite, shared_from_this(),
							boost::asio::placeholders::error));
			}
			else
			{
				if(closing_connection_)
				{
					//std::cerr<<"!!!: closing connection\n";
					Cleanup();
				}
			}
		}

		void SplitChannelMessage(const std::string & data, std::string & channel, std::string & message)
		{
			if(data.empty())
				return;
			size_t pos = data.find(' ');
			if(data[0] == '#')
			{
				channel = data.substr(0, pos);
				if(pos != std::string::npos)
				{
					pos = data.find(':', pos+1);
					if(pos != std::string::npos)
					{
						message = data.substr(pos+1);
					}
				}
			}
		}

		void Cleanup()
		{
			if(initialized_)
			{
				if(!active_channels_.empty())
				{
					std::for_each(active_channels_.begin(), active_channels_.end(),
							boost::bind(&Chat::LeaveChannel, &bridge_, _1, shared_from_this()));
					active_channels_.clear();
				}
				bridge_.Leave(shared_from_this());
				if(socket_.is_open())
					socket_.close();
				initialized_ = false;
			}
		}

		class ChannelListBuilder
			: public Chat::IChannelVisitor
		{
		public:
			ChannelListBuilder(std::ostream &  ostr, const std::string & servername, const std::string & nick)
				: ostr_(ostr),
					servername_(servername),
					nick_(nick)
			{}
			virtual ~ChannelListBuilder() {}
			virtual void Visit(const Chat::ChannelMap::value_type & channel)
			{
				ostr_<<":"<<servername_<<" 322 "<<nick_<<" "
						 <<channel.first<<" 999 :"<<channel.second->GetTitle()<<"\n";
			}
		private:
			std::ostream &  ostr_;
			const std::string & servername_;
			const std::string & nick_;
		};

	private:
		typedef  void (Session::*MessageHandler)(const std::string & command_id, const std::string & data, std::string & answer);

		tcp::socket socket_;
		Chat& bridge_;
		boost::asio::streambuf buffer_;
		ChatMessageQueue write_msgs_;
		bool initialized_;
		bool authorized_;
		boost::asio::deadline_timer register_timeout_;
		boost::asio::deadline_timer connection_timeout_;
		std::string nick_;
		std::string password_;
		std::set<std::string> active_channels_;
		bool closing_connection_;
		bool ping_sent_;
		boost::mutex sync_;
		std::map<std::string, MessageHandler> registration_handlers_;
		std::map<std::string, MessageHandler> message_handlers_;
	};

	typedef boost::shared_ptr<Session> SessionPtr;
} // namespace debugirc
