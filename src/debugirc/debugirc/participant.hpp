/* participant.hpp
 * This file is a part of debugirc library
 * Copyright (c) debugirc authors (see file `COPYRIGHT` for the license)
 */

#pragma once

#include <boost/shared_ptr.hpp>
#include "types.hpp"

namespace debugirc
{
	class ChatParticipant
	{
	public:
		virtual ~ChatParticipant() {}
		virtual void Deliver(const ChatMessage& msg) = 0;
	};

	typedef boost::shared_ptr<ChatParticipant> ChatParticipantPtr;
} // namespace debugirc
