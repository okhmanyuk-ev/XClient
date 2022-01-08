#include "ai_client.h"
#include <HL/utils.h>
#include <common/helpers.h>

std::optional<std::shared_ptr<NavMesh::Area>> NavMesh::getArea(const glm::vec3& pos) const
{
	return std::nullopt;
}

NavMesh::Chain NavMesh::buildChain(const glm::vec3& start, const glm::vec3& end) const
{
	return {};
}

void NavMesh::addArea(Area area)
{
	mAreas.insert({ mAreaIndex, area });
	mAreaIndex += 1;
}

bool NavMesh::Area::isContainPoint(const glm::vec3& value) const
{
	return false;
}

AiClient::AiClient()
{
	setCertificate({ 1, 2, 3, 4 });

	setThinkCallback([this](HL::Protocol::UserCmd& cmd) {
		think(cmd);
	});

	setResourceRequiredCallback([this](const HL::Protocol::Resource& resource) -> bool {
		const auto& info = getServerInfo().value();

		if (resource.name == info.map)
			return true;

		return false;
	});
}

void AiClient::initializeGameEngine()
{
	PlayableClient::initializeGameEngine();

	mThinkTime = Clock::Now();
}

void AiClient::initializeGame()
{
	PlayableClient::initializeGame();

	const auto& info = getServerInfo().value();
    mBspFile.loadFromFile(info.game_dir + "/" + info.map, false);

	CONSOLE->execute("delay 1 'cmd \"jointeam 2\"'");
	CONSOLE->execute("delay 2 'cmd \"joinclass 6\"'");
}

void AiClient::think(HL::Protocol::UserCmd& cmd)
{
	auto now = Clock::Now();
	auto delta = now - mThinkTime;
	mThinkTime = now;

	cmd.msec = Clock::ToMilliseconds(delta);
	cmd.forwardmove = 0.0f;
	cmd.sidemove = 0.0f;
	cmd.upmove = 0.0f;
	cmd.buttons = 0;
	cmd.viewangles = mPrevViewAngles;

	movement(cmd);

	if (mWantJump && (isOnGround() || isOnLadder()))
	{
		cmd.buttons |= IN_JUMP;
	}

	if (mWantDuck)
	{
		cmd.buttons |= IN_DUCK;
	}

	if (isOnGround() || isOnLadder())
	{
		mWantDuck = false;
	}

	mWantJump = false;

	mPrevViewAngles = cmd.viewangles;

	if (!isOnGround())
	{
		mLastAirTime = Clock::Now();
	}
}

void AiClient::movement(HL::Protocol::UserCmd& cmd)
{
	if (avoidOtherPlayers(cmd) == MovementStatus::Processing)
		return;

	moveToCustomTarget(cmd);
}

glm::vec3 AiClient::getOrigin() const
{
	return getClientData().origin;
}

glm::vec3 AiClient::getFootOrigin() const
{
	auto origin = getOrigin();

	if (isDucking())
		origin.z -= PlayerOriginZDuck;
	else
		origin.z -= PlayerOriginZStand;

	return origin;
}

std::optional<glm::vec3> AiClient::getGroundFromOrigin(const glm::vec3& origin) const
{
	auto start_pos = origin;
	auto end_pos = start_pos - glm::vec3{ 0.0f, 0.0f, MaxDistance };
	auto trace = traceLine(start_pos, end_pos);

	if (trace.start_solid)
		return std::nullopt;

	return trace.endpos;
}

std::optional<glm::vec3> AiClient::getRoofFromOrigin(const glm::vec3& origin) const
{
	auto start_pos = origin;
	auto end_pos = start_pos + glm::vec3{ 0.0f, 0.0f, MaxDistance };
	auto trace = traceLine(start_pos, end_pos);

	if (trace.start_solid)
		return std::nullopt;

	return trace.endpos;
}

std::optional<HL::Protocol::Entity*> AiClient::findNearestVisiblePlayerEntity()
{
	std::optional<HL::Protocol::Entity*> result;
	auto min_distance = MaxDistance;

	for (const auto& [index, entity] : getEntities())
	{
		if (!isPlayerIndex(index))
			continue;

		if (!isVisible(*entity))
			continue;

		auto distance = getDistance(*entity);

		if (distance < min_distance)
		{
			min_distance = distance;
			result = entity;
		}
	}

	return result;
}

bool AiClient::isVisible(const glm::vec3& target) const
{
	auto result = traceLine(getOrigin(), target);
	return result.fraction >= 1.0f;
}

bool AiClient::isVisible(const HL::Protocol::Entity& entity) const
{
	return isVisible(entity.origin);
}

bool AiClient::isOnGround() const
{
	auto flags = getClientData().flags;
	return flags & FL_ONGROUND;
}

bool AiClient::isOnLadder() const
{
	return false; // TODO
}

bool AiClient::isDucking() const
{
	auto flags = getClientData().flags;
	return flags & FL_DUCKING;
}

bool AiClient::isTired() const
{
	return Clock::Now() - mLastAirTime <= Clock::FromSeconds(JumpCooldownSeconds);
}

float AiClient::getSpeed() const
{
	return glm::length(getClientData().velocity);
}

float AiClient::getDistance(const glm::vec3& target) const
{
	return glm::distance(getOrigin(), target);
}

float AiClient::getDistance(const HL::Protocol::Entity& entity) const
{
	return getDistance(entity.origin);
}

void AiClient::moveTo(HL::Protocol::UserCmd& cmd, const glm::vec3& target, bool walk) const
{
	float speed = getClientData().maxspeed;

	if (speed <= 0.0f)
		speed = getMoveVars().value().max_speed;

	if (walk && !isDucking())
		speed *= WalkSpeedMultiplier;

	auto origin = getOrigin();
	auto v = target - origin;
	auto angle = cmd.viewangles.y - glm::degrees(glm::atan(v.y, v.x));

	cmd.forwardmove = glm::cos(glm::radians(angle)) * speed;
	cmd.sidemove = glm::sin(glm::radians(angle)) * speed;
	cmd.buttons |= IN_FORWARD; // TODO: this is for ladders, it seems we should refactor moveTo approach
}

void AiClient::moveTo(HL::Protocol::UserCmd& cmd, const HL::Protocol::Entity& entity, bool walk) const
{
	moveTo(cmd, entity.origin, walk);
}

void AiClient::moveFrom(HL::Protocol::UserCmd& cmd, const glm::vec3& target, bool walk) const
{
	moveTo(cmd, target, walk);
	cmd.forwardmove = -cmd.forwardmove;
	cmd.sidemove = -cmd.sidemove;
}

void AiClient::moveFrom(HL::Protocol::UserCmd& cmd, const HL::Protocol::Entity& entity, bool walk) const
{
	moveFrom(cmd, entity.origin, walk);
}

void AiClient::lookAt(HL::Protocol::UserCmd& cmd, const glm::vec3& target) const
{
	auto origin = getOrigin();
	auto v = target - origin;
	glm::vec3 viewangles;
	viewangles.x = (float)-glm::degrees(glm::atan(v.z, glm::sqrt(v.x * v.x + v.y * v.y))); // TODO: glm dot?
	viewangles.y = (float)glm::degrees(glm::atan(v.y, v.x));
	viewangles.z = 0.0f;

	cmd.viewangles.x = Common::Helpers::SmoothValueAssign(cmd.viewangles.x, viewangles.x, Clock::FromMilliseconds(cmd.msec));
	cmd.viewangles.y = glm::degrees(Common::Helpers::SmoothRotationAssign(glm::radians(cmd.viewangles.y), glm::radians(viewangles.y), Clock::FromMilliseconds(cmd.msec)));
	cmd.viewangles.z = 0.0f;
}

void AiClient::lookAt(HL::Protocol::UserCmd& cmd, const HL::Protocol::Entity& entity) const
{
	lookAt(cmd, entity.origin);
}

void AiClient::jump(bool duck)
{
	if (isTired())
		return;

	mWantJump = true;

	if (duck)
		mWantDuck = true;
}

void AiClient::duck()
{
	mWantDuck = true;
}

AiClient::TraceResult AiClient::traceLine(const glm::vec3& begin, const glm::vec3& end) const
{
	TraceResult result;
	auto r = mBspFile.traceLine(begin, end, false);
	result.endpos = r.endpos;
	result.fraction = r.fraction;
	result.start_solid = r.startsolid;
	return result;
}

AiClient::MovementStatus AiClient::trivialMoveTo(HL::Protocol::UserCmd& cmd, const glm::vec3& target)
{
	auto distance = getDistance(target);

	if (distance <= PlayerWidth / 2.0f)
		return MovementStatus::Finished;

	bool walk = distance <= PlayerWidth * 2.0f;

	lookAt(cmd, target);
	moveTo(cmd, target, walk);

	const auto foot_origin = getFootOrigin();
	const glm::vec3 foot_target = { target.x, target.y, foot_origin.z };

	const auto direction = glm::normalize(foot_target - foot_origin);
	const auto foot_next_pos = foot_origin + (direction * PlayerWidth);

	struct Window
	{
		glm::vec3 ground;
		glm::vec3 roof;
	};

	std::optional<Window> window;

	float search_z_offset = 0.0f;

	while (true)
	{
		auto search_origin = foot_next_pos + glm::vec3{ 0.0f, 0.0f, search_z_offset };
		auto ground = getGroundFromOrigin(search_origin);

		if (ground.has_value())
		{
			auto step_height = ground.value().z - foot_next_pos.z;

			if (step_height > JumpCrouchHeight)
				break;

			auto roof = getRoofFromOrigin(search_origin);
			auto window_height = glm::distance(ground.value(), roof.value());
			if (window_height >= PlayerHeightDuck)
			{
				window = Window{ ground.value(), roof.value() };
				break;
			}
		}

		if (search_z_offset > JumpCrouchHeight)
			break;

		search_z_offset += PlayerHeightDuck;
	}

	if (!window.has_value())
		return MovementStatus::Processing;

	auto ground_next_pos = window.value().ground;
	auto roof_next_pos = window.value().roof;
	
	auto step_height = ground_next_pos.z - foot_next_pos.z;

	if (step_height > StepHeight)
	{
		jump(true);
	}
	else if (glm::distance(ground_next_pos, roof_next_pos) < PlayerHeightStand)
	{
		duck();
	}

	return MovementStatus::Processing;
}

AiClient::MovementStatus AiClient::navmeshMoveTo(HL::Protocol::UserCmd& cmd, const glm::vec3& target)
{
	/*auto ground = getGroundFromOrigin(getOrigin());

	if (!ground.has_value())
		return MovementStatus::Processing;

	auto my_area = mNavMesh.getArea(ground.value());

	if (!my_area.has_value())
	{
		buildNavMesh(ground.value());
		return MovementStatus::Processing;
	}*/

	return trivialMoveTo(cmd, target);
}

AiClient::MovementStatus AiClient::avoidOtherPlayers(HL::Protocol::UserCmd& cmd)
{
	auto nearest_ent = findNearestVisiblePlayerEntity();

	if (!nearest_ent.has_value())
		return MovementStatus::Finished;

	auto distance = getDistance(*nearest_ent.value());

	if (distance >= PlayerWidth * 1.5f)
		return MovementStatus::Finished;

	lookAt(cmd, *nearest_ent.value());
	moveFrom(cmd, *nearest_ent.value());
	return MovementStatus::Processing;
}

AiClient::MovementStatus AiClient::moveToCustomTarget(HL::Protocol::UserCmd& cmd)
{
	if (!mCustomMoveTarget.has_value())
		return MovementStatus::Finished;
	
	auto target = mCustomMoveTarget.value();
	target.z = getOrigin().z;

	if (navmeshMoveTo(cmd, target) == MovementStatus::Finished)
	{
		if (getSpeed() == 0.0f)
		{
			mCustomMoveTarget.reset();
			return MovementStatus::Finished;
		}
	}

	return MovementStatus::Processing;
}

void AiClient::buildNavMesh(const glm::vec3& start_ground_point)
{
	auto area = NavMesh::Area{};
	area.position = start_ground_point;

	//mNavMesh.addArea(area);
}
