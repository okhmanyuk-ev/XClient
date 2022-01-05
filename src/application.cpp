#include "application.h"
#include "gameplay_screen.h"

using namespace XClient;

Application::Application() : Shared::Application(PROJECT_NAME, { Flag::Network, Flag::Scene, Flag::Audio })
{
	PLATFORM->resize(640, 360);
	PLATFORM->rescale(1.5f);

	ImGui::User::SetupStyleFromColor(0.5, 1.0, 0.5);

	ENGINE->addSystem<AiClient>(std::make_shared<AiClient>());
	mHudViews = std::make_shared<HL::HudViews>(*CLIENT);

	PRECACHE_FONT_ALIAS("fonts/sansation.ttf", "default");

	Scene::Label::DefaultFont = FONT("default");

	CONSOLE->execute("hud_show_fps 1");
	CONSOLE->execute("r_vsync 1");
	//CONSOLE->execute("hud_show_ents 1"); // feel free to uncomment

	SCENE_MANAGER->switchScreen(std::make_shared<GameplayScreen>());

	STATS->setAlignment(Shared::StatsSystem::Align::BottomRight);
}

Application::~Application()
{
	ENGINE->removeSystem<AiClient>();
}
