/* chat.hpp
 * This file is a part of debugirc library
 * Copyright (c) debugirc authors (see file `COPYRIGHT` for the license)
 */

#pragma once

#include <set>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/locks.hpp>
#include "types.hpp"
#include "participant.hpp"
#include "channel.hpp"
#include "authmanager.hpp"

namespace debugirc
{
	class Chat
	{
	public:
		typedef boost::unordered_map<std::string, ChannelPtr>  ChannelMap;

		class IChannelVisitor
		{
		public:
			virtual void Visit(const ChannelMap::value_type &) = 0;
		};

		Chat()
		  : server_name_("debugirc"),
			  motd_start_("DebugIRC"),
				motd_("This is debug irc interface for logging and similar tasks"),
				auth_manager_(new AuthManager())
		{
		}

		const std::string & GetServerName() const { return server_name_; }
		void SetServerName(const std::string & value) { server_name_ = value; }

		const std::string & GetMOTDStart() const { return motd_start_; }
		void SetMOTDStart(const std::string & value) { motd_start_ = value; }

		const std::string & GetMOTD() const { return motd_; }
		void SetMOTD(const std::string & value) { motd_ = value; }

		const std::string & GetAutoJoin() const { return auto_join_; }
		void SetAutoJoin(const std::string & value) { auto_join_ = value; }

		void AddChannel(const std::string & name, const std::string & title)
		{
			boost::unique_lock<boost::shared_mutex> lock(channel_sync_);
			channels_.insert(std::make_pair(name, ChannelPtr(new Channel(name, title))));
		}

		void RemoveChannel(const std::string & name)
		{
			boost::unique_lock<boost::shared_mutex> lock(channel_sync_);
			channels_.erase(name);
		}

		void VisitChannels(IChannelVisitor * reviever) const
		{
			if(!reviever)
				return;
			boost::shared_lock<boost::shared_mutex> lock(channel_sync_);
			std::for_each(channels_.begin(), channels_.end(),
							boost::bind(&IChannelVisitor::Visit, reviever, _1));
		}

		void Join(const ChatParticipantPtr & participant)
		{
			boost::unique_lock<boost::shared_mutex> lock(participant_sync_);
			participants_.insert(participant);
		}

		void Leave(const ChatParticipantPtr & participant)
		{
			boost::unique_lock<boost::shared_mutex> lock(participant_sync_);
			participants_.erase(participant);
		}

		bool JoinChannel(const std::string & name, const ChatParticipantPtr & participant)
		{
			boost::shared_lock<boost::shared_mutex> lock(channel_sync_);
			ChannelMap::iterator it = channels_.find(name);
			if(it == channels_.end())
				return false;
			return it->second->Join(participant);
		}

		void LeaveChannel(const std::string & name, const ChatParticipantPtr & participant)
		{
			boost::shared_lock<boost::shared_mutex> lock(channel_sync_);
			ChannelMap::iterator it = channels_.find(name);
			if(it == channels_.end())
				return;
			it->second->Leave(participant);
		}

		void DeliverAll(const std::string & msg)
		{
			boost::shared_lock<boost::shared_mutex> lock(participant_sync_);
			ChatMessage info(new std::string(msg));
			std::for_each(participants_.begin(), participants_.end(),
					boost::bind(&ChatParticipant::Deliver, _1, boost::ref(info)));
		}

		void DeliverChannel(const std::string & name, const std::string & msg)
		{
			boost::shared_lock<boost::shared_mutex> lock(channel_sync_);
			ChannelMap::iterator it = channels_.find(name);
			if(it != channels_.end())
			{
				std::stringstream strstr;
				strstr<<":"<<GetServerName()<<" PRIVMSG "<<name<<" :"<<msg<<"\n";
				it->second->Deliver(strstr.str());
			}
		}

		bool Authorize(const std::string & username, const std::string & password)
		{
			return auth_manager_ && auth_manager_->Authorize(username, password);
		}

	private:
		// should not be changed after server started up
		std::string server_name_;
		std::string motd_start_;
		std::string motd_;
		std::string auto_join_;
		AuthManagerPtr auth_manager_;
		// can be changed after server startup
		ChannelMap channels_;
		mutable boost::shared_mutex channel_sync_;
		std::set<ChatParticipantPtr> participants_;
		boost::shared_mutex participant_sync_;
	};
} // namespace debugirc
