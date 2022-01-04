#pragma once

#include <HL/playable_client.h>
#include <HL/bspfile.h>

class AiClient : public HL::PlayableClient
{
public:
	AiClient();

protected:
	void initializeGameEngine() override;
	void initializeGame() override;
	void think(HL::Protocol::UserCmd& usercmd);
	void testMove(HL::Protocol::UserCmd& usercmd);
	bool isVisible(const glm::vec3& origin) const;
	bool isVisible(const HL::Protocol::Entity& entity) const;
	float getDistance(const glm::vec3& origin) const;
	float getDistance(const HL::Protocol::Entity& entity) const;
	void moveTo(HL::Protocol::UserCmd& usercmd, const glm::vec3& origin) const;
	void moveTo(HL::Protocol::UserCmd& usercmd, const HL::Protocol::Entity& entity) const;
	void moveFrom(HL::Protocol::UserCmd& usercmd, const glm::vec3& origin) const;
	void moveFrom(HL::Protocol::UserCmd& usercmd, const HL::Protocol::Entity& entity) const;
	void lookAt(HL::Protocol::UserCmd& usercmd, const glm::vec3& origin) const;
	void lookAt(HL::Protocol::UserCmd& usercmd, const HL::Protocol::Entity& entity) const;

public:
	void setMoveTarget(std::optional<glm::vec3> value) { mMoveTarget = value; }

private:
	Clock::TimePoint mThinkTime = Clock::Now();
	BSPFile mBspFile;
	glm::vec3 mPrevViewAngles = { 0.0f, 0.0f, 0.0f };
	std::optional<glm::vec3> mMoveTarget;
};