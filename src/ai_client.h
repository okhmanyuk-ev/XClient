#pragma once

#include <HL/playable_client.h>

class AiClient : public HL::PlayableClient
{
public:
	AiClient();

private:
	void initializeGame();
	void think(HL::Protocol::UserCmd& usercmd);
	
private:
	bool mFirstThink = false;
	Clock::TimePoint mThinkTime = Clock::Now();
};