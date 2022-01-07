#pragma once

#include <HL/playable_client.h>
#include <HL/bspfile.h>

class AiClient : public HL::PlayableClient
{
private:
	const float PlayerHeightStand = 72.0f;
	const float PlayerHeightDuck = 36.0f;
	const float PlayerOriginZStand = 36.0f;
	const float PlayerOriginZDuck = 18.0f;
	const float PlayerWidth = 32.0f;
	const float StepHeight = 18.0f; // if delta Z is greater than this, we have to jump to get up (MoveVars.stepsize)
	const float JumpHeight = 41.8f; // if delta Z is less than this, we can jump up on it
	const float JumpCrouchHeight = 58.0f; // (48) if delta Z is less than or equal to this, we can jumpcrouch up on it
	const float MaxDistance = 8192.0f;
	const float WalkSpeedMultiplier = 0.4f;
	const float UseRadius = 64.0f;
	const float JumpCooldownSeconds = 1.0f; // we should not bunnyhopping, because next jumps are not high

public:
	AiClient();

private:
	void initializeGameEngine() override;
	void initializeGame() override;
	void think(HL::Protocol::UserCmd& cmd);
	void movement(HL::Protocol::UserCmd& cmd);
	glm::vec3 getOrigin() const;
	glm::vec3 getFootOrigin() const;
	std::optional<HL::Protocol::Entity*> findNearestVisiblePlayerEntity();
	bool isVisible(const glm::vec3& target) const;
	bool isVisible(const HL::Protocol::Entity& entity) const;
	bool isOnGround() const;
	bool isOnLadder() const;
	bool isDucking() const;
	bool isTired() const;
	float getSpeed() const;
	float getDistance(const glm::vec3& target) const;
	float getDistance(const HL::Protocol::Entity& entity) const;
	void moveTo(HL::Protocol::UserCmd& cmd, const glm::vec3& target, bool walk = false) const;
	void moveTo(HL::Protocol::UserCmd& cmd, const HL::Protocol::Entity& entity, bool walk = false) const;
	void moveFrom(HL::Protocol::UserCmd& cmd, const glm::vec3& target, bool walk = false) const;
	void moveFrom(HL::Protocol::UserCmd& cmd, const HL::Protocol::Entity& entity, bool walk = false) const;
	void lookAt(HL::Protocol::UserCmd& cmd, const glm::vec3& target) const;
	void lookAt(HL::Protocol::UserCmd& cmd, const HL::Protocol::Entity& entity) const;
	void jump(bool duck = false);
	void duck();

public:
	struct TraceResult
	{
		glm::vec3 endpos = { 0.0f, 0.0f, 0.0f };
		float fraction = 0.0f;
		bool start_solid = false;
	};

	TraceResult traceLine(const glm::vec3& begin, const glm::vec3& end) const;

private:
	enum class TrivialMoveStatus
	{
		Finished,
		Processing
	};

	TrivialMoveStatus trivialMoveTo(HL::Protocol::UserCmd& cmd, const glm::vec3& target);

	enum class AvoidOtherPlayersStatus
	{
		Finished,
		Processing
	};

	AvoidOtherPlayersStatus avoidOtherPlayers(HL::Protocol::UserCmd& cmd);

	TrivialMoveStatus trivialMoveToCustomTarget(HL::Protocol::UserCmd& cmd);

public:
	void setCustomMoveTarget(const glm::vec3& value) { mCustomMoveTarget = value; };
	const auto& getCustomMoveTarget() const { return mCustomMoveTarget; }
	const auto& getBsp() const { return mBspFile; }

private:
	Clock::TimePoint mThinkTime = Clock::Now();
	BSPFile mBspFile;
	glm::vec3 mPrevViewAngles = { 0.0f, 0.0f, 0.0f };
	std::optional<glm::vec3> mCustomMoveTarget;
	bool mWantJump = false;
	bool mWantDuck = false;
	Clock::TimePoint mLastAirTime = Clock::Now();
};