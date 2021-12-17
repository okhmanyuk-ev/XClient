#include <Shared/all.h>
#include <Console/system.h>
#include <Common/embedded_console_device.h>
#include <Common/console_commands.h>
#include <Common/frame_system.h>
#include <Common/framerate_counter.h>
#include <Network/system.h>
#include <HL/playable_client.h>
#include <Common/native_console_device.h>
#include <Platform/asset.h>
#include <HL/bspfile.h>

#ifndef XCLIENT_DLL
#include <sol/sol.hpp>

class Scripting
{	
public:
	void handleError(const sol::protected_function_result& pfr)
	{
		CONSOLE_DEVICE->writeLine("");
		CONSOLE_DEVICE->writeLine(sol::to_string(pfr.status()), Console::Color::Red);
		CONSOLE_DEVICE->writeLine(pfr.get<sol::error>().what(), Console::Color::Red);
	}

	static int HandlePanic(lua_State* L)
	{
		CONSOLE_DEVICE->writeLine("");
		CONSOLE_DEVICE->writeLine(lua_tostring(L, -1), Console::Color::Red);

		auto lua = sol::state_view(L);
		CONSOLE_DEVICE->writeLine(lua["debug"]["traceback"](), Console::Color::Red);

		return 0;
	}

	void initializeGame()
	{
		mBspFile.loadFromFile(mClient.getGameDir() + "/" + mClient.getMap(), false);

		lua["InitializeGame"]();
	}

	Scripting(HL::PlayableClient& client) : mClient(client)
	{	
		lua.open_libraries();
		lua.set_panic(HandlePanic);

		/*lua["print"] = [this](sol::variadic_args va) {
			for (auto v : va)
				mConsoleDevice->write(v.as<std::string>() + " ");
			
			mConsoleDevice->writeLine("");
		};*/

		// console

		auto nsConsole = lua.create_named_table("Console");
		nsConsole["Execute"] = [this](const std::string & s) {
			CONSOLE->execute(s);
		};
		nsConsole["Write"] = [this](const std::string & s, std::optional<int> color) {
			auto col = (Console::Color)color.value_or((int)Console::Color::Default);
			CONSOLE_DEVICE->write(s, col);
		};
		nsConsole["WriteLine"] = [this](const std::string & s, std::optional<int> color) {
			auto col = (Console::Color)color.value_or((int)Console::Color::Default);
			CONSOLE_DEVICE->writeLine(s, col);
		};
		nsConsole["RegisterCommand"] = [this](const std::string& name, sol::variadic_args args) {
			std::string description = {};
			std::vector<std::string> cmd_args = {};
			sol::function callback = {};

			int argc = 0;

			if (args[argc].is<std::string>())
			{
				description = args[argc].as<std::string>();
				argc++;
			}

			if (args[argc].is<std::vector<std::string>>())
			{
				cmd_args = args[argc].as<std::vector<std::string>>();
				argc++;
			}

			callback = args[argc];

			CONSOLE->registerCommand(name, description, cmd_args, [callback](CON_ARGS) {
				callback.call(args);
			});
		};

		nsConsole["RegisterCVar"] = [this](const std::string& name, sol::variadic_args args) {
			std::string description = {};
			std::vector<std::string> cvar_args = {};
			std::vector<std::string> cvar_optional_args = {};
			sol::function getter = {};
			sol::function setter = {};

			int argc = 0;

			if (args[argc].is<std::string>())
			{
				description = args[argc].as<std::string>();
				argc++;
			}

			if (args[argc].is<std::vector<std::string>>())
			{
				cvar_args = args[argc].as<std::vector<std::string>>();
				argc++;
			}

			if (args[argc].is<std::vector<std::string>>())
			{
				cvar_optional_args = args[argc].as<std::vector<std::string>>();
				argc++;
			}

			getter = args[argc];
			setter = args[argc + 1];

			CONSOLE->registerCVar(name, description, cvar_args, cvar_optional_args, 
				[getter] { return getter.call().get<std::vector<std::string>>(); }, 
				setter.valid() ? ([setter](CON_ARGS) { setter.call(args); }) : Console::CVar::Setter(nullptr));
		};

		// client

		auto nsClient = lua.create_named_table("Client");
		nsClient["SendCommand"] = [this](const std::string& s) {
			mClient.sendCommand(s);
		};
		nsClient["GetIndex"] = [this] {
			return mClient.getIndex();
		};
		nsClient["GetMap"] = [this] {
			return mClient.getMap();
		};
		nsClient["GetGameDir"] = [this] {
			return mClient.getGameDir();
		};
		nsClient["GetOrigin"] = [this] {
			auto origin = mClient.getClientData().origin;
			auto result = lua.create_table();
			result[1] = origin[0];
			result[2] = origin[1];
			result[3] = origin[2];
			return result;
		};
		nsClient["GetEntitiesCount"] = [this] {
			return mClient.getEntities().size();
		};
		nsClient["IsEntityActive"] = [this](int index) {
			return mClient.getEntities().count(index) > 0;
		};
		nsClient["IsPlayerIndex"] = [this](int index) {
			return mClient.isPlayerIndex(index);
		};
		nsClient["GetEntity"] = [this](int index) {
			const auto& ent = *mClient.getEntities().at(index);
			auto result = lua.create_table();
			
			result["origin"] = lua.create_table();
			result["origin"][1] = ent.origin[0];
			result["origin"][2] = ent.origin[1];
			result["origin"][3] = ent.origin[2];
			
			//result["angles"] = lua.create_table_with(ent.angles[0], ent.angles[1], ent.angles[2]);
			result["angles"] = lua.create_table();
			result["angles"][1] = ent.angles[0];
			result["angles"][2] = ent.angles[1];
			result["angles"][3] = ent.angles[2];

			return result;
		};
		nsClient["TraceLine"] = [this](sol::table v1, sol::table v2) {
			glm::vec3 start = { v1["X"], v1["Y"], v1["Z"] };
			glm::vec3 dest = { v2["X"], v2["Y"], v2["Z"] };

			auto trace = mBspFile.traceLine(start, dest);
			
			auto result = lua.create_table();

			result["AllSolid"] = trace.allsolid;
			result["StartSolid"] = trace.startsolid;
			result["InOpen"] = trace.inopen;
			result["InWater"] = trace.inwater;
			result["Fraction"] = trace.fraction;
			result["EndPos"] = lua.create_table();
			result["EndPos"][1] = trace.endpos.x;
			result["EndPos"][2] = trace.endpos.y;
			result["EndPos"][3] = trace.endpos.z;
			// plane
			// edict
			result["HitGroup"] = trace.hitgroup;

			return result;
		};
		nsClient["SetCertificate"] = [this](const std::string& value) {
			mClient.setCertificate({ value.begin(), value.end() });
		};

		// start

		lua.do_string("package.path = package.path .. ';./assets/scripting/?.lua'");
		
		auto main_script = Platform::Asset("scripting/main.lua");
		auto main_script_str = std::string((char*)main_script.getMemory(), main_script.getSize());
		if (auto res = lua.do_string(main_script_str); !res.valid())
		{
			handleError(res);
			return;
		}
	
		lua["Initialize"]();
		
		// read game messages

		mClient.setReadGameMessageCallback([this](const std::string& name, void* memory, size_t size) {
			lua["ReadGameMessage"](name, std::string((char*)memory, size));
		});
		mClient.setResourceRequiredCallback([this](const HL::Protocol::Resource& resource) -> bool {
			if (resource.name == mClient.getMap())
				return true;

			return lua["IsResourceRequired"](resource.name);
		});

		// think

		mClient.setThinkCallback([this](HL::Protocol::UserCmd& usercmd) {
			if (!mFirstThink)
			{
				initializeGame();
				mFirstThink = true;
			}

			auto cmd = lua["Think"]().get<sol::table>();
			usercmd.msec = cmd["MSec"];
			usercmd.buttons = cmd["Buttons"];
			usercmd.forwardmove = cmd["ForwardMove"];
			usercmd.sidemove = cmd["SideMove"];
			usercmd.upmove = cmd["UpMove"];

			auto viewAngles = cmd["ViewAngles"];
			if (viewAngles.valid())
			{
				usercmd.viewangles[0] = viewAngles[1];
				usercmd.viewangles[1] = viewAngles[2];
				usercmd.viewangles[2] = viewAngles[3];
			}
		});

		mClient.setDisconnectCallback([this](const auto& reason) {
			mFirstThink = false;
		});

		// frame

		mFramer.setCallback([this] {
			lua["Frame"](Clock::ToSeconds(FRAME->getTimeDelta()));
		});
	}

	auto getMemoryUsed() const { return lua.memory_used(); }

private:
	bool mFirstThink = false;
	HL::PlayableClient& mClient;
	Common::FrameSystem::Framer mFramer;
	sol::state lua;
	BSPFile mBspFile;
};

void main(int argc, char* argv[])
{
	Core::Engine engine;

	ENGINE->addSystem<Common::FrameSystem>(std::make_shared<Common::FrameSystem>());
	ENGINE->addSystem<Common::Event::System>(std::make_shared<Common::Event::System>());
	ENGINE->addSystem<Console::Device>(std::make_shared<Common::NativeConsoleDevice>());
	ENGINE->addSystem<Console::System>(std::make_shared<Console::System>());
	ENGINE->addSystem<Network::System>(std::make_shared<Network::System>());

	Common::ConsoleCommands consoleCommands;
	HL::PlayableClient client;
	Common::FramerateCounter framerateCounter;
	Scripting scripting(client);

	Common::Timer timer;
	timer.setInterval(Clock::FromSeconds(1.0f));
	timer.setCallback([&] {
		NATIVE_CONSOLE_DEVICE->setTitle("XClient - " + std::to_string(framerateCounter.getFramerate()) + " fps, " +
			Common::Helpers::BytesToNiceString(scripting.getMemoryUsed()) + " mem");
	});

	bool shutdown = false;
	consoleCommands.setQuitCallback([&shutdown] { shutdown = true; });

	while (!shutdown)
	{
		FRAME->frame();
	}
}
#else
class XClient
{
public:
	using ConsoleWriteCallback = void(*)(const char*, int color);
	using ConsoleClearCallback = void(*)();
	using ThinkCallback = void(*)(HL::Protocol::UserCmd*);
	using ReadGameMessageCallback = void(*)(const char*, void* data, size_t size);

public:
	XClient()
	{
		ENGINE->addSystem<Common::FrameSystem>(std::make_shared<Common::FrameSystem>());
		ENGINE->addSystem<Common::EventSystem>(std::make_shared<Common::EventSystem>());
		ENGINE->addSystem<Console::Device>(std::make_shared<Common::EmbeddedConsoleDevice>());
		ENGINE->addSystem<Console::System>(std::make_shared<Console::System>());
		ENGINE->addSystem<Network::System>(std::make_shared<Network::System>());

		mConsoleCommands = std::make_shared<Common::ConsoleCommands>();
		mClient = std::make_shared<HL::PlayableClient>();

		mConsoleCommands->setQuitCallback([this] {
			mRunning = false;
		});

		EMBEDDED_CONSOLE_DEVICE->setWriteCallback([this](const auto& s, auto col) {
			if (mConsoleWriteCallback)
			{
				mConsoleWriteCallback(s.c_str(), (int)col);
			}
		});

		EMBEDDED_CONSOLE_DEVICE->setWriteLineCallback([this](const auto& s, auto col) {
			if (mConsoleWriteLineCallback)
			{
				mConsoleWriteLineCallback(s.c_str(), (int)col);
			}
		});

		EMBEDDED_CONSOLE_DEVICE->setClearCallback([this] {
			if (mConsoleClearCallback)
			{
				mConsoleClearCallback();
			}
		});

		mClient->setThinkCallback([this](auto& cmd) {
			if (!mFirstThink)
			{
				initializeGame();
				mFirstThink = true;
			}
			if (mThinkCallback)
			{
				mThinkCallback(&cmd);
			}
		});

		mClient->setReadGameMessageCallback([this](const std::string& name, void* memory, size_t size) {
			if (mReadGameMessageCallback)
			{
				mReadGameMessageCallback(name.c_str(), memory, size);
			}
		});

		mClient->setResourceRequiredCallback([this](const HL::Protocol::Resource& resource) -> bool {
			if (resource.name == mClient->getMap())
				return true;

			return false;
		});
	}

	void ensureContext()
	{
		ENGINE = &mEngine;
	}

private:
	void initializeGame()
	{
		mBspFile.loadFromFile(mClient->getGameDir() + "/" + mClient->getMap(), false);
	}

public:
	Core::Engine mEngine;
	std::shared_ptr<Common::ConsoleCommands> mConsoleCommands;
	std::shared_ptr<HL::PlayableClient> mClient;
	bool mRunning = true;

	ConsoleWriteCallback mConsoleWriteCallback = nullptr;
	ConsoleWriteCallback mConsoleWriteLineCallback = nullptr;
	ConsoleClearCallback mConsoleClearCallback = nullptr;
	ThinkCallback mThinkCallback = nullptr;
	ReadGameMessageCallback mReadGameMessageCallback = nullptr;

private:
	bool mFirstThink = false;
	BSPFile mBspFile;
};

#define EXPORT extern "C" __declspec(dllexport) 

EXPORT void* xcCreate()
{
	return new XClient();
}

EXPORT void xcDestroy(XClient* client)
{
	client->ensureContext();
	delete client;
}

EXPORT void xcFrame(XClient* client)
{
	client->ensureContext();
	FRAME->frame();
}

EXPORT bool xcRunning(XClient* client)
{
	return client->mRunning;
}

EXPORT void xcSetConsoleWriteCallback(XClient* client, XClient::ConsoleWriteCallback value)
{
	client->mConsoleWriteCallback = value;
}

EXPORT void xcSetConsoleWriteLineCallback(XClient* client, XClient::ConsoleWriteCallback value)
{
	client->mConsoleWriteLineCallback = value;
}

EXPORT void xcSetConsoleClearCallback(XClient* client, XClient::ConsoleClearCallback value)
{
	client->mConsoleClearCallback = value;
}

EXPORT void xcSetThinkCallback(XClient* client, XClient::ThinkCallback value)
{
	client->mThinkCallback = value;
}

EXPORT void xcSetReadGameMessageCallback(XClient* client, XClient::ReadGameMessageCallback value)
{
	client->mReadGameMessageCallback = value;
}

EXPORT void xcSetCertificate(XClient* client, void* data, size_t size)
{
	auto value = std::string((const char*)data, size);
	client->mClient->setCertificate({ value.begin(), value.end() });
}

EXPORT void xcExecuteCommand(XClient* client, const char* str)
{
	client->ensureContext();
	CONSOLE->execute(str);
}

EXPORT void xcSendCommand(XClient* client, const char* str)
{
	client->ensureContext();
	client->mClient->sendCommand(str);
}

EXPORT int xcGetEntityCount(XClient* client)
{
	client->ensureContext();
	auto& entities = client->mClient->getEntities();
	int high = 0;

	for (auto& [index, entity] : entities)
	{
		if (high >= index)
			continue;

		high = index;
	}

	return high;
}

EXPORT HL::Protocol::Entity* xcGetEntity(XClient* client, int index)
{
	client->ensureContext();
	auto& entities = client->mClient->getEntities();

	if (entities.count(index) > 0)
		return entities.at(index);
	else
		return nullptr;
}

EXPORT HL::Protocol::ClientData* xcGetClientData(XClient* client)
{
	client->ensureContext();
	return const_cast<HL::Protocol::ClientData*>(&client->mClient->getClientData());
}

#endif