#pragma once

#include <shared/all.h>
#include "ai_client.h"
#include <HL/hud_views.h>

#define CLIENT ENGINE->getSystem<AiClient>()

namespace XClient
{
	class Application : public Shared::Application
	{
	public:
		Application();
		~Application();

	private:
		std::shared_ptr<HL::HudViews> mHudViews;
	};
}