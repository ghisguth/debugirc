/* authmanager.hpp
 * This file is a part of debugirc library
 * Copyright (c) debugirc authors (see file `COPYRIGHT` for the license)
 */

#pragma once

#include <string>
#include <boost/shared_ptr.hpp>

namespace debugirc
{
	class AuthManager
	{
	public:
		virtual ~AuthManager() {}
		virtual bool Authorize(const std::string & username, const std::string & password)
		{
			return username.length() >= 1 && username.length() <= 16;
		}
	};

	typedef boost::shared_ptr<AuthManager> AuthManagerPtr;
} // namespace debugirc
