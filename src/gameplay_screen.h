#pragma once

#include <shared/all.h>
#include <HL/hltv_client.h>
#include <HL/gameplay_view_node.h>
#include <HL/bsp_draw.h>

namespace XClient
{
	class GameplayViewNode : public HL::GameplayViewNode
	{
	public:
		GameplayViewNode();
		~GameplayViewNode();

	public:
		void draw() override;
		void touch(Touch type, const glm::vec2& pos) override;

	protected:
		void drawOnBackground(Scene::Node& holder) override;

	private:
		void drawTrivialMovement(Scene::Node& holder);
		void drawNavMovement(Scene::Node& holder);
		void draw3dView();
		void draw3dNavMesh(std::shared_ptr<skygfx::RenderTarget> target, const glm::vec3& pos, const glm::vec3& angles);
		void draw2dNavMesh(Scene::Node& holder);

	private:
		std::optional<std::pair<std::string, std::shared_ptr<HL::BspDraw>>> mBspDraw;
		bool mDraw3dBsp = false;
		bool mDraw2dNavmesh = true;
	};

	class GameplayScreen : public Shared::SceneHelpers::StandardScreen
	{
	public:
		GameplayScreen();

		void draw() override;
	};
}
