#pragma once

#include <HL/playable_client.h>
#include <HL/bspfile.h>

enum class NavDirection
{
	Forward,
	Back,
	Left,
	Right
};

struct NavArea
{
	glm::vec3 position = { 0.0f, 0.0f, 0.0f };
	std::map<NavDirection, std::optional<std::weak_ptr<NavArea>>> neighbours;
	bool isExplored() const;
	bool isBorder() const;
	bool isNeighbour(std::shared_ptr<NavArea> area) const;
};

struct NavMesh
{
	using AreaSet = std::unordered_set<std::shared_ptr<NavArea>>;

	AreaSet explored_areas;
	AreaSet unexplored_areas;

	static std::shared_ptr<NavArea> FindNearestArea(const AreaSet& areas, const glm::vec3& pos);
	static std::shared_ptr<NavArea> FindExactArea(const AreaSet& areas, const glm::vec3& pos, float tolerance);
};

using NavChain = std::list<std::shared_ptr<NavArea>>;

const std::vector<NavDirection> Directions = {
	NavDirection::Forward,
	NavDirection::Left,
	NavDirection::Right,
	NavDirection::Back,
};

const std::map<NavDirection, NavDirection> OppositeDirections = {
	{ NavDirection::Forward, NavDirection::Back },
	{ NavDirection::Back, NavDirection::Forward },
	{ NavDirection::Left, NavDirection::Right },
	{ NavDirection::Right, NavDirection::Left },
};

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

	const float NavStep = PlayerWidth * 1.0f;
	const float NavExploreDistance = 256.0f;

	const float TrivialMovementMinDistance = PlayerWidth * 0.75f;

public:
	AiClient();
	~AiClient();

public:
	void onFrame() override;

public:
	glm::vec3 getOrigin() const;
	glm::vec3 getAngles() const;

private:
	void initializeGameEngine() override;
	void initializeGame() override;
	void resetGameResources() override;
	void think(HL::Protocol::UserCmd& cmd);
	void synchronizeBspModel();
	void movement(HL::Protocol::UserCmd& cmd);
	glm::vec3 getFootOrigin() const;
	std::optional<glm::vec3> getGroundFromOrigin(const glm::vec3& origin) const;
	std::optional<glm::vec3> getRoofFromOrigin(const glm::vec3& origin) const;
	std::optional<HL::Protocol::Entity*> findNearestVisiblePlayerEntity();
	bool isVisible(const glm::vec3& eye, const glm::vec3& target) const;
	bool isVisible(const glm::vec3& target) const;
	bool isVisible(const HL::Protocol::Entity& entity) const;
	bool isAlive() const;
	bool isSpectator() const;
	bool isOnGround() const;
	bool isOnLadder() const;
	bool isDucking() const;
	bool isTired() const;
	float getHealth() const;
	float getCurrentHeight() const;
	float getCurrentEyeHeight() const;
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
	enum class MovementStatus
	{
		Finished,
		Processing
	};

	MovementStatus trivialMoveTo(HL::Protocol::UserCmd& cmd, const glm::vec3& target, bool allow_walk = true);
	MovementStatus trivialAvoidWallCorners(HL::Protocol::UserCmd& cmd, const glm::vec3& target);
	MovementStatus trivialAvoidVerticalObstacles(HL::Protocol::UserCmd& cmd, const glm::vec3& target);
	MovementStatus navMoveTo(HL::Protocol::UserCmd& cmd, const glm::vec3& target);
	MovementStatus avoidOtherPlayers(HL::Protocol::UserCmd& cmd);
	MovementStatus moveToCustomTarget(HL::Protocol::UserCmd& cmd);
	MovementStatus exploreNewAreas(HL::Protocol::UserCmd& cmd);
	
private:
	enum class BuildNavMeshStatus
	{
		Finished,
		Processing
	};

	BuildNavMeshStatus buildNavMesh();
	BuildNavMeshStatus buildNavMesh(const glm::vec3& start_ground_point);
	BuildNavMeshStatus buildNavMesh(std::shared_ptr<NavArea> base_area);
	NavChain buildNavChain(std::shared_ptr<NavArea> src_area, std::shared_ptr<NavArea> dst_area);

public:
	void setCustomMoveTarget(const glm::vec3& value) { mCustomMoveTarget = value; };
	const auto& getCustomMoveTarget() const { return mCustomMoveTarget; }
	const auto& getBsp() const { return mBspFile; }
	const auto& getNavMesh() const { return mNavMesh; }
	const auto& getNavChain() const { return mNavChain; }

	auto getUseNavMovement() const { return mUseNavMovement; }
	void setUseNavMovement(bool value) { mUseNavMovement = value; }

private:
	Clock::TimePoint mThinkTime = Clock::Now();
	BSPFile mBspFile;
	glm::vec3 mPrevViewAngles = { 0.0f, 0.0f, 0.0f };
	std::optional<glm::vec3> mCustomMoveTarget;
	bool mWantJump = false;
	bool mWantDuck = false;
	Clock::TimePoint mLastAirTime = Clock::Now();
	NavMesh mNavMesh;
	NavChain mNavChain;
	glm::vec3 mNavChainTarget;
	bool mUseNavMovement = true;
	float mNavExploreDistance = NavExploreDistance;
	float mNavStep = NavStep;
	std::set<int> mBspModelIndices;
};
