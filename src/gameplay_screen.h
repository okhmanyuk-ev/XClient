#pragma once

#include <HL/hltv_client.h>
#include <shared/all.h>

namespace XClient
{
	class GameplayScreen : public Scene::Tappable<Shared::SceneHelpers::StandardScreen>
	{
	public:
		GameplayScreen();
	};
}