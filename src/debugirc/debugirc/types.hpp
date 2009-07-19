/* types.hpp
 * This file is a part of debugirc library
 * Copyright (c) debugirc authors (see file `COPYRIGHT` for the license)
 */

#pragma once

#include <string>
#include <deque>
#include <boost/shared_ptr.hpp>

namespace debugirc
{
	typedef boost::shared_ptr<std::string> ChatMessage;
	typedef std::deque<ChatMessage> ChatMessageQueue;
} // namespace debugirc
