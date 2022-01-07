#pragma once

#include <shared/all.h>
#include <HL/hltv_client.h>
#include <HL/gameplay_view_node.h>

namespace XClient
{
	class GameplayViewNode : public HL::GameplayViewNode
	{
	public:
		GameplayViewNode();

	public:
		void draw() override;
		void touch(Touch type, const glm::vec2& pos) override;

	private:
		void drawCustomMoveTarget();
	};

	class GameplayScreen : public Shared::SceneHelpers::StandardScreen
	{
	public:
		GameplayScreen();

		void draw() override;
	};
}