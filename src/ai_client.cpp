#include "ai_client.h"
#include <HL/utils.h>
#include <common/helpers.h>

std::shared_ptr<NavArea> NavMesh::findNearestArea(const glm::vec3& pos) const
{
	float min_distance = 8192.0f;
	std::shared_ptr<NavArea> result = nullptr;
	for (auto area : areas)
	{
		auto distance = glm::distance(pos, area->position);
		if (distance < min_distance)
		{
			result = area;
			min_distance = distance;
		}
	}
	return result;
}

std::shared_ptr<NavArea> NavMesh::findExactArea(const glm::vec3& pos, float tolerance) const
{
	for (auto area : areas)
	{
		auto distance = glm::distance(pos, area->position);
		if (distance <= tolerance)
			return area;
	}
	return nullptr;
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
	mNavChain.clear();
	mNavMesh.areas.clear();
	mCustomMoveTarget.reset();
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

glm::vec3 AiClient::getAngles() const
{
	return mPrevViewAngles;
}

glm::vec3 AiClient::getFootOrigin() const
{
	auto origin = getOrigin();
	origin.z -= getCurrentEyeHeight();
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

float AiClient::getCurrentHeight() const
{
	if (isDucking())
		return PlayerHeightDuck;
	else
		return PlayerHeightStand;
}

float AiClient::getCurrentEyeHeight() const
{
	if (isDucking())
		return PlayerOriginZDuck;
	else
		return PlayerOriginZStand;
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

AiClient::MovementStatus AiClient::trivialMoveTo(HL::Protocol::UserCmd& cmd, const glm::vec3& target, bool allow_walk)
{
	const glm::vec3 eye_target = { target.x, target.y, getOrigin().z };

	auto distance = getDistance(eye_target);

	if (distance <= TrivialMovementMinDistance)
		return MovementStatus::Finished;

	lookAt(cmd, eye_target);

	if (trivialAvoidVerticalObstacles(cmd, target) == MovementStatus::Processing)
		return MovementStatus::Processing;

	if (trivialAvoidWallCorners(cmd, target) == MovementStatus::Processing)
		return MovementStatus::Processing;

	bool walk = distance <= PlayerWidth * 2.0f && allow_walk;

	moveTo(cmd, target, walk);

	return MovementStatus::Processing;
}

AiClient::MovementStatus AiClient::trivialAvoidWallCorners(HL::Protocol::UserCmd& cmd, const glm::vec3& target)
{
	const auto origin = getOrigin();
	const auto direction = glm::normalize(target - origin);

	auto left_direction = glm::cross(direction, { 0.0f, 0.0f, -1.0f });
	auto right_direction = glm::cross(direction, { 0.0f, 0.0f, 1.0f });

	const auto origin_left = origin + (left_direction * PlayerWidth * 0.5f);
	const auto origin_left_forward = origin_left + (direction * PlayerWidth * 1.5f);

	const auto origin_right = origin + (right_direction * PlayerWidth * 0.5f);
	const auto origin_right_forward = origin_right + (direction * PlayerWidth * 1.5f);

	auto result_left = traceLine(origin_left, origin_left_forward);
	auto result_right = traceLine(origin_right, origin_right_forward);

	if (result_left.fraction < 1.0f && result_right.fraction >= 1.0f)
	{
		moveTo(cmd, origin_right);
		return MovementStatus::Processing;
	}
	else if (result_left.fraction >= 1.0f && result_right.fraction < 1.0f)
	{
		moveTo(cmd, origin_left);
		return MovementStatus::Processing;
	}
	
	return MovementStatus::Finished;
}

AiClient::MovementStatus AiClient::trivialAvoidVerticalObstacles(HL::Protocol::UserCmd& cmd, const glm::vec3& target)
{
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
		return MovementStatus::Finished;

	auto ground_next_pos = window.value().ground;
	auto roof_next_pos = window.value().roof;

	auto step_height = ground_next_pos.z - foot_next_pos.z;

	if (step_height > StepHeight)
	{
		moveTo(cmd, target);
		jump(true);
		return MovementStatus::Processing;
	}
	else if (glm::distance(ground_next_pos, roof_next_pos) < PlayerHeightStand)
	{
		moveTo(cmd, target);
		duck();
		return MovementStatus::Processing;
	}

	return MovementStatus::Finished;
}

AiClient::MovementStatus AiClient::navMoveTo(HL::Protocol::UserCmd& cmd, const glm::vec3& target)
{
	auto src_area = mNavMesh.findNearestArea(getFootOrigin());
	auto dst_area = mNavMesh.findNearestArea(target);

	mNavChain = buildNavChain(dst_area, src_area);
	mNavChain.reverse();

	/*for (auto area : mNavChain)
	{
		auto pos = area.lock()->position;
		auto corrected_pos = pos + glm::vec3{ 0.0f, 0.0f, getCurrentOriginZ() };

		auto result = trivialMoveTo(cmd, corrected_pos);

		if (result == MovementStatus::Finished)
			continue;
		else
			return result;
	}

	return MovementStatus::Finished;*/

	/*auto trivial_target = target;
	trivial_target.z += getCurrentOriginZ();

	for (auto area : mNavChain)
	{
		auto pos = area.lock()->position;
		auto corrected_pos = pos + glm::vec3{ 0.0f, 0.0f, getCurrentOriginZ() };

		if (getDistance(corrected_pos) <= TrivialMovementMinDistance)
			continue;

		if (!isVisible(corrected_pos))
			break;

		mNavChain.pop_front();
		trivial_target = corrected_pos;
		break;
	}

	return trivialMoveTo(cmd, trivial_target);*/

	if (mNavChain.size() > 1)
	{
		mNavChain.pop_front();
		mNavChain.pop_front();
	}
	else if (mNavChain.size() > 0)
	{
		mNavChain.pop_front();
	}

	if (!mNavChain.empty())
		return trivialMoveTo(cmd, mNavChain.front()->position, false);
	else
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
	auto ground = getGroundFromOrigin(getOrigin());

	if (!ground.has_value())
		return MovementStatus::Processing;

	buildNavMesh(ground.value());

	if (mNavMesh.areas.empty())
		return MovementStatus::Processing;

	if (!mCustomMoveTarget.has_value())
		return MovementStatus::Finished;
	
	auto target = mCustomMoveTarget.value();
	
	auto target_ground = getGroundFromOrigin(mCustomMoveTarget.value());

	if (target_ground.has_value())
		target = target_ground.value();

	MovementStatus result;

	if (mUseNavMovement)
		result = navMoveTo(cmd, target);
	else
		result = trivialMoveTo(cmd, target);

	if (result == MovementStatus::Finished)
	{
		if (getSpeed() == 0.0f)
		{
			mCustomMoveTarget.reset();
			return MovementStatus::Finished;
		}
	}

	return MovementStatus::Processing;
}

AiClient::BuildNavMeshStatus AiClient::buildNavMesh(const glm::vec3& start_ground_point)
{
	if (mNavMesh.areas.empty())
	{
		auto area = std::make_shared<NavArea>();
		area->position = start_ground_point;
		mNavMesh.areas.insert(area);
		return BuildNavMeshStatus::Processing;
	}

	auto base_area = mNavMesh.findExactArea(start_ground_point, NavStep * 1.25f);

	if (base_area == nullptr)
	{
		mNavMesh.areas.clear();
		return BuildNavMeshStatus::Processing;
	}

	std::list<std::shared_ptr<NavArea>> open_list;
	std::unordered_set<std::shared_ptr<NavArea>> ignore;
	open_list.push_back(base_area);
	bool skip = false;

	while (!open_list.empty())
	{
		auto area = open_list.back();
		open_list.pop_back();

		if (ignore.contains(area))
			continue;

		ignore.insert(area);

		while (true)
		{
			if (buildNavMesh(area) != BuildNavMeshStatus::Finished)
				skip = true;
			else
				break;
		}

		if (skip)
			continue;

		for (auto dir : Directions)
		{
			auto neighbour = area->neighbours.at(dir);

			if (!neighbour.has_value())
				continue;

			auto neighbour_nn = neighbour.value().lock();

			if (getDistance(neighbour_nn->position) > NavFieldDistance)
				continue;

			open_list.push_front(neighbour_nn);
		}
	}

	removeFarNavAreas();

	GAME_STATS("areas", mNavMesh.areas.size());

	return skip ? BuildNavMeshStatus::Processing : BuildNavMeshStatus::Finished;
}

AiClient::BuildNavMeshStatus AiClient::buildNavMesh(std::shared_ptr<NavArea> base_area)
{
	auto stepPosition = [NavStep = NavStep](const glm::vec3& pos, NavDirection dir) -> glm::vec3 {
		auto dst_pos = pos;
		if (dir == NavDirection::Back)
			dst_pos.y -= NavStep;
		else if (dir == NavDirection::Forward)
			dst_pos.y += NavStep;
		else if (dir == NavDirection::Left)
			dst_pos.x += NavStep;
		else if (dir == NavDirection::Right)
			dst_pos.x -= NavStep;
		return dst_pos;
	};

	for (auto dir : Directions)
	{
		if (base_area->neighbours.contains(dir))
			continue;

		auto src_pos = base_area->position;
		src_pos.z += StepHeight;

		auto dst_pos = stepPosition(src_pos, dir);
		
		auto result = traceLine(src_pos, dst_pos);

		if (result.fraction < 1.0f)
		{
			base_area->neighbours.insert({ dir, std::nullopt });
			return BuildNavMeshStatus::Processing;
		}

		auto dst_ground = getGroundFromOrigin(dst_pos).value();

		auto neighbour = mNavMesh.findExactArea(dst_ground, 4.0f);

		auto opposite_dir = OppositeDirections.at(dir);

		if (neighbour != nullptr)
		{
			base_area->neighbours.insert({ dir, neighbour });
			neighbour->neighbours.insert({ opposite_dir, base_area });
			return BuildNavMeshStatus::Processing;
		}

		auto area = std::make_shared<NavArea>();
		area->position = dst_ground;
		mNavMesh.areas.insert(area);

		base_area->neighbours.insert({ dir, area });
		area->neighbours.insert({ opposite_dir, base_area });

		return BuildNavMeshStatus::Processing;
	}

	return BuildNavMeshStatus::Finished;
}

void AiClient::removeNavArea(std::shared_ptr<NavArea> area)
{
	for (auto _area : mNavMesh.areas)
	{
		for (auto dir : Directions)
		{
			if (!_area->neighbours.contains(dir))
				continue;

			auto neighbour = _area->neighbours.at(dir);

			if (!neighbour.has_value())
				continue;

			auto neighbour_nn = neighbour.value().lock();

			if (neighbour_nn != area)
				continue;

			_area->neighbours.erase(dir);
		}
	}

	mNavMesh.areas.erase(area);
}

void AiClient::removeFarNavAreas()
{
	std::list<std::shared_ptr<NavArea>> far_areas;

	for (auto area : mNavMesh.areas)
	{
		auto distance = glm::distance(getFootOrigin(), area->position);
		
		if (distance <= NavFieldDistance * 1.25f)
			continue;

		far_areas.push_back(area);
	}

	for (auto far_area : far_areas)
	{
		removeNavArea(far_area);
	}
}

NavChain AiClient::buildNavChain(std::shared_ptr<NavArea> src_area, std::shared_ptr<NavArea> dst_area)
{
	assert(src_area);
	assert(dst_area);

	struct Info
	{
		std::shared_ptr<NavArea> parent;
		float cost_to_start = 0.0f; // g
		float cost_to_finish = 0.0f; // h
		auto get_cost_total() const { return cost_to_start + cost_to_finish; } // f
		//auto get_cost_total() const { return cost_to_start; } // f, dijkstra
	};

	std::unordered_map<std::shared_ptr<NavArea>, Info> infos;
	std::unordered_set<std::shared_ptr<NavArea>> open_list;
	std::unordered_set<std::shared_ptr<NavArea>> closed_list;

	auto find_best_from_open_list = [&] {
		float min_cost = std::numeric_limits<float>::max();
		std::shared_ptr<NavArea> result = nullptr;
		for (auto area : open_list)
		{
			const auto& info = infos.at(area);
			auto cost_total = info.get_cost_total();
			if (cost_total < min_cost)
			{
				result = area;
				min_cost = cost_total;
			}
		}
		assert(result);
		return result;
	};

	auto assemble_chain = [&](std::shared_ptr<NavArea> a) {
		auto result = NavChain{};
		while (a != nullptr)
		{
			result.push_front(a);
			a = infos.at(a).parent;
		}
		return result;
	};

	auto get_cost_multiplier = [](std::shared_ptr<NavArea> a) {
		const float total_penalty = 16.0f;
		float result = total_penalty;
		for (auto dir : Directions)
		{
			if (!a->neighbours.contains(dir))
				continue;//return -1.0f; // promote researching

			auto neighbour = a->neighbours.at(dir);

			if (!neighbour.has_value())
				continue;

			result -= total_penalty / 4.0f;
		}
		return result + 1.0f;
	};

	auto& src_area_info = infos[src_area];
	src_area_info.cost_to_finish = glm::distance(src_area->position, dst_area->position);
	open_list.insert(src_area);

	while (!open_list.empty())
	{
		auto area = find_best_from_open_list();

		if (area == dst_area)
			return assemble_chain(area);

		open_list.erase(area);
		closed_list.insert(area);

		for (auto [dir, neighbour] : area->neighbours)
		{
			if (!neighbour.has_value())
				continue;

			auto neighbour_nn = neighbour->lock();

			if (!neighbour_nn->neighbours.at(OppositeDirections.at(dir)).has_value())
				continue; // do not allow one-way connections, because we swap src and dst areas

			if (closed_list.contains(neighbour_nn))
				continue;

			auto cost_multiplier = get_cost_multiplier(neighbour_nn);
			auto cost_to_start = infos.at(area).cost_to_start + glm::distance(area->position, neighbour_nn->position) * cost_multiplier;

			bool neighbour_is_better = !infos.contains(neighbour_nn) || infos.at(neighbour_nn).cost_to_start > cost_to_start;
			
			if (!neighbour_is_better)
				continue;

			open_list.insert(neighbour_nn);

			auto& info = infos[neighbour_nn];
			info.parent = area;
			info.cost_to_finish = glm::distance(neighbour_nn->position, dst_area->position);
			info.cost_to_start = cost_to_start;
		}
	}

	return { };
}
