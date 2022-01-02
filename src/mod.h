#pragma once

#include <common/bitbuffer.h>
#include <string>
#include <map>
#include <functional>
#include <HL/base_client.h>

class Mod
{
public:
	void readMessage(const std::string& name, BitBuffer& msg);

public:
	using MessageCallback = std::function<void(BitBuffer& msg)>;

public:
	void setMessageCallback(const std::string& name, MessageCallback callback);

protected:
	void doWithClient(std::function<void(HL::BaseClient&)> func);

public:
	void setGetClientCallback(std::function<HL::BaseClient&()> value) { mGetClientCallback = value; }

private:
	std::map<std::string, MessageCallback> mMessageCallbacks;
	std::function<HL::BaseClient&()> mGetClientCallback;
};