/* channel.hpp
 * This file is a part of debugirc library
 * Copyright (c) debugirc authors (see file `COPYRIGHT` for the license)
 */

#pragma once

#include <string>
#include <set>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/locks.hpp>
#include "types.hpp"
#include "participant.hpp"

namespace debugirc
{
	class Channel
	{
	public:
		Channel(const std::string & name, const std::string & title)
			: name_(name),
				title_(title)
		{}

		const std::string & GetTitle() const { return title_; }
		const std::string & GetName() const { return name_; }

		bool Join(const ChatParticipantPtr & participant)
		{
			boost::unique_lock<boost::shared_mutex> lock(sync_);
			return participants_.insert(participant).second;
		}

		void Leave(const ChatParticipantPtr & participant)
		{
			boost::unique_lock<boost::shared_mutex> lock(sync_);
			participants_.erase(participant);
		}

		void Deliver(const ChatMessage & msg)
		{
			boost::shared_lock<boost::shared_mutex> lock(sync_);
			std::for_each(participants_.begin(), participants_.end(),
					boost::bind(&ChatParticipant::Deliver, _1, boost::ref(msg)));
		}

		void Deliver(const std::string & msg)
		{
			ChatMessage info(new std::string(msg));
			Deliver(info);
		}

	private:
		std::set<ChatParticipantPtr> participants_;
		std::string name_;
		std::string title_;
		boost::shared_mutex sync_;
	};

	typedef boost::shared_ptr<Channel> ChannelPtr;
} // namespace debugirc
