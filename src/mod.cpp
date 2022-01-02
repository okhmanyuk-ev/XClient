#include "mod.h"
#include <console/device.h>

void Mod::readMessage(const std::string& name, BitBuffer& msg)
{
	if (!mMessageCallbacks.contains(name))
	{
		LOGF("unknown gmsg: {}", name);
		return;
	}

	mMessageCallbacks.at(name)(msg);
}

void Mod::setMessageCallback(const std::string& name, MessageCallback callback)
{
	mMessageCallbacks.insert({ name, callback });
}

void Mod::doWithClient(std::function<void(HL::BaseClient&)> func)
{
	func(mGetClientCallback());
}
