#include "ai_client.h"
#include <HL/utils.h>

AiClient::AiClient()
{
	CONSOLE->execute("connect 127.0.0.1");

	setCertificate({ 1, 2, 3, 4 });

	setThinkCallback([this](HL::Protocol::UserCmd& usercmd) {
		if (!mFirstThink)
		{
			initializeGame();
			mFirstThink = true;
		}

		think(usercmd);
	});
}

void AiClient::initializeGame()
{
	CONSOLE->execute("delay 1 'cmd \"jointeam 2\"'");
	CONSOLE->execute("delay 2 'cmd \"joinclass 6\"'");
}

void AiClient::think(HL::Protocol::UserCmd& usercmd)
{
	auto now = Clock::Now();
	auto delta = now - mThinkTime;
	mThinkTime = now;

	usercmd.msec = Clock::ToMilliseconds(delta);
	usercmd.forwardmove = 0.0f;
	usercmd.sidemove = 0.0f;
	usercmd.upmove = 0.0f;
	usercmd.buttons = 0;	
}