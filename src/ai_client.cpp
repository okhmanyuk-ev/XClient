#include "ai_client.h"
#include <HL/utils.h>
#include "mod_cs.h"
#include <common/helpers.h>

AiClient::AiClient()
{
	CONSOLE->execute("connect 127.0.0.1");

	setCertificate({ 1, 2, 3, 4 });

	setThinkCallback([this](HL::Protocol::UserCmd& usercmd) {
		think(usercmd);
	});

	setReadGameMessageCallback([this](const std::string& name, void* memory, size_t size) {
		auto msg = BitBuffer();
		msg.write(memory, size);
		msg.toStart();
		mMod->readMessage(name, msg);
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

	mMod = std::make_shared<ModCS>();
	mMod->setGetClientCallback([this]()->HL::BaseClient& {
		return *this;
	});
}

void AiClient::initializeGame()
{
	PlayableClient::initializeGame();

	const auto& info = getServerInfo().value();
	mBspFile.loadFromFile(info.game_dir + "/" + info.map, false);

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
	usercmd.viewangles = mPrevViewAngles;

	testMove(usercmd);

	mPrevViewAngles = usercmd.viewangles;
}

void AiClient::testMove(HL::Protocol::UserCmd& usercmd)
{
	HL::Protocol::Entity* nearest_ent = nullptr;
	float min_distance = 9999.0f;
	
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
			nearest_ent = entity;
		}
	}

	if (nearest_ent == nullptr)
		return;

	lookAt(usercmd, *nearest_ent);

	auto distance = getDistance(*nearest_ent);

	if (distance > 200.0f)
		moveTo(usercmd, *nearest_ent);
	else if (distance < 150.0f)
		moveFrom(usercmd, *nearest_ent);
}

bool AiClient::isVisible(const glm::vec3& origin) const
{
	auto my_origin = getClientData().origin;
	auto result = mBspFile.traceLine(my_origin, origin);
	return result.fraction >= 1.0f;
}

bool AiClient::isVisible(const HL::Protocol::Entity& entity) const
{
	return isVisible(entity.origin);
}

float AiClient::getDistance(const glm::vec3& origin) const
{
	auto my_origin = getClientData().origin;
	return glm::distance(my_origin, origin);
}

float AiClient::getDistance(const HL::Protocol::Entity& entity) const
{
	return getDistance(entity.origin);
}

void AiClient::moveTo(HL::Protocol::UserCmd& usercmd, const glm::vec3& origin) const
{
	const float Speed = 250.0f;

	auto my_origin = getClientData().origin;
	auto v = origin - my_origin;
	auto angle = usercmd.viewangles.y - glm::degrees(glm::atan(v.y, v.x));

	usercmd.forwardmove = glm::cos(glm::radians(angle)) * Speed;
	usercmd.sidemove = glm::sin(glm::radians(angle)) * Speed;
}

void AiClient::moveTo(HL::Protocol::UserCmd& usercmd, const HL::Protocol::Entity& entity) const
{
	moveTo(usercmd, entity.origin);
}

void AiClient::moveFrom(HL::Protocol::UserCmd& usercmd, const glm::vec3& origin) const
{
	moveTo(usercmd, origin);
	usercmd.forwardmove = -usercmd.forwardmove;
	usercmd.sidemove = -usercmd.sidemove;
}

void AiClient::moveFrom(HL::Protocol::UserCmd& usercmd, const HL::Protocol::Entity& entity) const
{
	moveFrom(usercmd, entity.origin);
}

void AiClient::lookAt(HL::Protocol::UserCmd& usercmd, const glm::vec3& origin) const
{
	auto v = origin - getClientData().origin;
	glm::vec3 viewangles;
	viewangles.x = (float)-glm::degrees(glm::atan(v.z, glm::sqrt(v.x * v.x + v.y * v.y))); // TODO: glm dot?
	viewangles.y = (float)glm::degrees(glm::atan(v.y, v.x));
	viewangles.z = 0.0f;

	usercmd.viewangles.x = Common::Helpers::SmoothValueAssign(usercmd.viewangles.x, viewangles.x, Clock::FromMilliseconds(usercmd.msec));
	usercmd.viewangles.y = glm::degrees(Common::Helpers::SmoothRotationAssign(glm::radians(usercmd.viewangles.y), glm::radians(viewangles.y), Clock::FromMilliseconds(usercmd.msec)));
	usercmd.viewangles.z = 0.0f;
}

void AiClient::lookAt(HL::Protocol::UserCmd& usercmd, const HL::Protocol::Entity& entity) const
{
	lookAt(usercmd, entity.origin);
}
