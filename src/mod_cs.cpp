#include "mod_cs.h"

ModCS::ModCS()
{
	setMessageCallback("ReqState", [this](auto& msg) {
		doWithClient([](HL::BaseClient& client) {
			client.sendCommand("VModEnable 1");
		});
	});
}