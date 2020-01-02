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
#include <Common/size_converter.h>
#include <sol/sol.hpp>
#include <HL/bspfile.h>

class Scripting
{	
public:
	void handleError(sol::protected_function_result pfr)
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

	Common::FrameSystem frameSystem;
	engine.setFrame(&frameSystem);

	Common::EventSystem eventSystem;
	engine.setEvent(&eventSystem);

	Common::NativeConsoleDevice consoleDevice;
	engine.setConsoleDevice(&consoleDevice);

	Console::System consoleSystem;
	engine.setConsole(&consoleSystem);

	Network::System networkSystem;
	engine.setNetwork(&networkSystem);
	
	Common::ConsoleCommands consoleCommands;
	HL::PlayableClient client;
	Common::FramerateCounter framerateCounter;
	Scripting scripting(client);

	Common::Timer timer;
	timer.setInterval(Clock::FromSeconds(1.0f));
	timer.setCallback([&] {
		consoleDevice.setTitle("XClient - " + std::to_string(framerateCounter.getFramerate()) + " fps, " + 
			Common::SizeConverter::ToString(scripting.getMemoryUsed()) + " mem");
	});

	bool shutdown = false;
	consoleCommands.setQuitCallback([&shutdown] { shutdown = true; });

	while (!shutdown)
	{
		frameSystem.frame();
	}
}