#include "gameplay_screen.h"
#include "application.h"

using namespace XClient;

GameplayViewNode::GameplayViewNode() : HL::GameplayViewNode(CLIENT)
{
	setTouchable(true);
}

void GameplayViewNode::draw()
{
	HL::GameplayViewNode::draw();

	if (CLIENT->getState() != HL::BaseClient::State::GameStarted)
		return;

	draw3dView();

	const auto& clientdata = CLIENT->getClientData();
	STATS_INDICATE_GROUP("clientdata", "origin", fmt::format("{:.0f} {:.0f} {:.0f}", clientdata.origin.x, clientdata.origin.y, clientdata.origin.z));
	STATS_INDICATE_GROUP("clientdata", "flags", clientdata.flags);
	STATS_INDICATE_GROUP("clientdata", "maxspeed", fmt::format("{:.0f}", clientdata.maxspeed));
}

void GameplayViewNode::drawOnBackground(Scene::Node& holder)
{
	if (CLIENT->getUseNavMovement())
	{
		draw2dNavMesh(holder);
		drawNavMovement(holder);
	}
	else
	{
		drawTrivialMovement(holder);
	}
	
	HL::GameplayViewNode::drawOnBackground(holder);
}

void GameplayViewNode::drawTrivialMovement(Scene::Node& holder)
{
	auto move_target = CLIENT->getCustomMoveTarget();

	static std::optional<glm::vec3> end_pos;
	
	if (!move_target.has_value())
	{
		end_pos.reset();
		return;
	}

	const auto dTime = FRAME->getTimeDelta();

	auto start_pos = CLIENT->getClientData().origin;
	
	if (!end_pos.has_value())
		end_pos = start_pos;

	end_pos = Common::Helpers::SmoothValueAssign(end_pos.value(), move_target.value(), dTime);

	end_pos.value().z = start_pos.z;

	auto trace_result = CLIENT->traceLine(start_pos, end_pos.value());

	auto mid_pos = trace_result.endpos;

	auto start_scr = worldToScreen(start_pos);
	auto mid_scr = worldToScreen(mid_pos);
	auto end_scr = worldToScreen(end_pos.value());

	auto node = IMSCENE->attachTemporaryNode<HL::GenericDrawNode>(holder);
	node->setStretch(1.0f);
	node->setDrawCallback([node, start_scr, mid_scr, end_scr] {
		GRAPHICS->draw(Renderer::Topology::LineList, {
			{ { start_scr, 0.0f }, { Graphics::Color::Lime, 1.0f } },
			{ { mid_scr, 0.0f }, { Graphics::Color::Lime, 1.0f } },
			{ { mid_scr, 0.0f }, { Graphics::Color::Red, 1.0f } },
			{ { end_scr, 0.0f }, { Graphics::Color::Red, 1.0f } }
		});
	});

	auto circle = IMSCENE->attachTemporaryNode<Scene::Circle>(holder);
	circle->setPosition(end_scr);
	circle->setPivot(0.5f);
	circle->setColor(trace_result.fraction < 1.0f ? Graphics::Color::Red : Graphics::Color::Lime);
	circle->setRadius(4.0f);
}

void GameplayViewNode::drawNavMovement(Scene::Node& holder)
{
	auto move_target = CLIENT->getCustomMoveTarget();

	static std::optional<glm::vec3> end_pos;

	if (!move_target.has_value())
	{
		end_pos.reset();
		return;
	}

	const auto dTime = FRAME->getTimeDelta();

	auto start_pos = CLIENT->getClientData().origin;

	if (!end_pos.has_value())
		end_pos = start_pos;

	end_pos = Common::Helpers::SmoothValueAssign(end_pos.value(), move_target.value(), dTime);

	auto end_scr = worldToScreen(end_pos.value());

	auto node = IMSCENE->attachTemporaryNode<HL::GenericDrawNode>(holder);
	node->setStretch(1.0f);
	node->setDrawCallback([this, start_pos, end_scr, dTime] {
		if (CLIENT->getState() != HL::BaseClient::State::GameStarted)
			return;

		static auto builder = Graphics::MeshBuilder();
		builder.begin();

		static std::vector<glm::vec3> g_chain;

		const auto& chain = CLIENT->getNavChain();

		while (g_chain.size() < chain.size())
			if (g_chain.empty())
				g_chain.push_back(start_pos);
			else
				g_chain.push_back(g_chain.back());

		while (g_chain.size() > chain.size())
			g_chain.pop_back();

		for (int i = 0; i < g_chain.size(); i++)
		{
			auto chain_area = *std::next(chain.begin(), i);
			g_chain[i] = Common::Helpers::SmoothValueAssign(g_chain.at(i), chain_area->position, dTime);
			//g_chain[i] = std::next(chain.begin(), i)->lock()->position;
		}

		std::optional<glm::vec3> prev_v;

		for (auto pos : g_chain)
		{
			if (prev_v.has_value())
			{
				builder.color(Graphics::Color::Yellow);
				builder.vertex(worldToScreen(prev_v.value()));
				builder.vertex(worldToScreen(pos));
			}
			prev_v = pos;
		}

		if (!prev_v.has_value())
			prev_v = start_pos;

		builder.color(Graphics::Color::Yellow);
		builder.vertex(worldToScreen(prev_v.value()));
		builder.vertex(end_scr);

		auto [vertices, count] = builder.end();

		GRAPHICS->draw(Renderer::Topology::LineList, vertices, count);
	});

	auto circle = IMSCENE->attachTemporaryNode<Scene::Circle>(holder);
	circle->setPosition(end_scr);
	circle->setPivot(0.5f);
	circle->setColor(Graphics::Color::Yellow);
	circle->setRadius(4.0f);
}

void GameplayViewNode::draw3dView() 
{
	if (!mBspDraw.has_value() || mBspDraw.value().first != getShortMapName())
		mBspDraw = { getShortMapName(), std::make_shared<HL::BspDraw>(CLIENT->getBsp()) };

	auto target = GRAPHICS->getRenderTarget("bsp_3d_view", 800, 600);
	static auto pos = glm::vec3{};
	pos = Common::Helpers::SmoothValueAssign(pos, CLIENT->getOrigin(), FRAME->getTimeDelta());
	auto angles = CLIENT->getAngles();
	mBspDraw.value().second->draw(target, pos, angles);
	draw3dNavMesh(target, pos, angles);

	ImGui::Begin("3D View");
	auto width = ImGui::GetContentRegionAvailWidth();
	Shared::SceneEditor::drawImage(target, std::nullopt, width);
	ImGui::End();
}

void GameplayViewNode::draw3dNavMesh(std::shared_ptr<Renderer::RenderTarget> target, const glm::vec3& pos, const glm::vec3& angles)
{
	static auto camera = std::make_shared<Graphics::Camera3D>();
	camera->setWorldUp({ 0.0f, 0.0f, 1.0f });
	camera->setPosition(pos);
	camera->setYaw(glm::radians(angles.x));
	camera->setPitch(glm::radians(angles.y));
	camera->onFrame();

	auto view = camera->getViewMatrix();
	auto projection = camera->getProjectionMatrix();
	auto prev_batching = GRAPHICS->isBatching();

	GRAPHICS->setBatching(false);
	GRAPHICS->pushCleanState();
	GRAPHICS->pushViewMatrix(view);
	GRAPHICS->pushProjectionMatrix(projection);
	GRAPHICS->pushRenderTarget(target);
	GRAPHICS->pushViewport(target);
	GRAPHICS->pushDepthMode(Renderer::ComparisonFunc::Less);
	
	const auto& nav = CLIENT->getNavMesh();

	static auto builder = Graphics::MeshBuilder();
	builder.begin();

	for (auto area : nav.areas)
	{
		auto v1 = area->position;
		auto v2 = v1 + glm::vec3{ 0.0f, 0.0f, 8.0f };

		builder.color(Graphics::Color::Blue);
		builder.vertex(v1);
		builder.vertex(v2);

		for (auto dir : { NavDirection::Forward, NavDirection::Right })
		{
			if (!area->neighbours.contains(dir))
				continue;

			auto neighbour = area->neighbours.at(dir);

			if (!neighbour.has_value())
				continue;

			auto v3 = neighbour.value().lock()->position + glm::vec3{ 0.0f, 0.0f, 8.0f };

			builder.color(Graphics::Color::Yellow);
			builder.vertex(v2);
			builder.vertex(v3);
		}
	}

	auto [vertices, count] = builder.end();

	GRAPHICS->draw(Renderer::Topology::LineList, vertices, count);
	GRAPHICS->pop(6);
	GRAPHICS->setBatching(prev_batching);
}		

void GameplayViewNode::draw2dNavMesh(Scene::Node& holder)
{
	auto node = IMSCENE->attachTemporaryNode<HL::GenericDrawNode>(holder);
	node->setStretch(1.0f);
	node->setDrawCallback([this] {
		if (CLIENT->getState() != HL::BaseClient::State::GameStarted)
			return;

		const auto& nav = CLIENT->getNavMesh();

		static auto builder = Graphics::MeshBuilder();
		builder.begin();

		for (auto area : nav.areas)
		{
			auto v1 = area->position;

			for (auto dir : { NavDirection::Forward, NavDirection::Right })
			{
				if (!area->neighbours.contains(dir))
					continue;

				auto neighbour = area->neighbours.at(dir);

				if (!neighbour.has_value())
					continue;

				auto neighbour_nn = neighbour.value().lock();

				auto opposite_dir = OppositeDirections.at(dir);
				if (!neighbour_nn->neighbours.contains(opposite_dir))
					builder.color({ Graphics::Color::Red, 0.5f });
				else if (!neighbour_nn->neighbours.at(opposite_dir).has_value())
					builder.color({ Graphics::Color::Blue, 0.5f });
				else
					builder.color({ Graphics::Color::White, 0.5f });

				auto v2 = neighbour_nn->position;

				builder.vertex(worldToScreen(v1));
				builder.vertex(worldToScreen(v2));
			}
		}

		auto [vertices, count] = builder.end();

		GRAPHICS->draw(Renderer::Topology::LineList, vertices, count);
	});
}

void GameplayViewNode::touch(Touch type, const glm::vec2& pos)
{
	HL::GameplayViewNode::touch(type, pos);

	if (CLIENT->getState() < HL::BaseClient::State::GameStarted)
		return;

	auto target = screenToWorld(unproject(pos));
	CLIENT->setCustomMoveTarget(target);

	if (type == Touch::Begin)
	{
		auto circle = std::make_shared<Scene::Circle>();
		circle->setPivot(0.5f);
		circle->setAnchor(pos / getAbsoluteSize() / PLATFORM->getScale());
		circle->setRadius(0.0f);
		attach(circle);

		const float AnimDuration = 1.0f;
		const auto Easing = Easing::CubicOut;

		circle->runAction(Actions::Collection::MakeSequence(
			Actions::Collection::MakeParallel(
				Actions::Collection::ChangeCircleRadius(circle, 32.0f, AnimDuration, Easing),
				Actions::Collection::ChangeAlpha(circle, 0.0f, AnimDuration, Easing),
				Actions::Collection::ChangeCircleFill(circle, 0.0f, AnimDuration, Easing)
			),
			Actions::Collection::Kill(circle)
		));
	}
}

GameplayScreen::GameplayScreen()
{
	//
}

void GameplayScreen::draw()
{
	Shared::SceneHelpers::StandardScreen::draw();

	auto gameplay_holder = IMSCENE->attachTemporaryNode<Shared::SceneHelpers::SafeArea>(*getContent());
	auto gui_holder = IMSCENE->attachTemporaryNode<Shared::SceneHelpers::SafeArea>(*getContent());

	auto state = CLIENT->getState();
	auto dTime = FRAME->getTimeDelta();

	if (state == HL::BaseClient::State::GameStarted)
	{
		auto gameplay_view = IMSCENE->attachTemporaryNode<GameplayViewNode>(*gameplay_holder);
		gameplay_view->setStretch(1.0f);
	}
	else if (state != HL::BaseClient::State::Disconnected)
	{
		auto label = IMSCENE->attachTemporaryNode<Scene::Label>(*gameplay_holder);
		label->setText("LOADING...");
		label->setFontSize(32.0f);
		label->setAnchor(0.5f);
		label->setPivot(0.5f);

		const auto& channel = CLIENT->getChannel();
		
		if (channel.has_value())
		{
			auto draw_progress_bar = [&](std::shared_ptr<HL::Channel::FragBuffer> fragbuf, float y) {
				int completed_count = 1;
				int total_count = fragbuf->frags.size();
				for (const auto& frag : fragbuf->frags)
				{
					if (!frag.completed)
						continue;

					completed_count += 1;
				}

				auto progress = static_cast<float>(completed_count) / static_cast<float>(total_count);
				
				auto progressbar = IMSCENE->attachTemporaryNode<Scene::ClippableStencil<Shared::SceneHelpers::Progressbar>>(*gameplay_holder);
				progressbar->setWidth(512.0f);
				progressbar->setHeight(8.0f);
				progressbar->setAnchor({ 0.5f, 0.5f });
				progressbar->setPivot(0.5f);
				progressbar->setY(y + 32.0f);
				progressbar->setRounding(1.0f);
				progressbar->setSlicedSpriteOptimizationEnabled(false); // this enables nice clipping
				progressbar->setProgress(Common::Helpers::SmoothValueAssign(progressbar->getProgress(), progress, dTime));
			};

			float y = 0.0f;
			const float y_step = 16.0f;
			for (auto [index, fragbuf] : channel->getNormalFragBuffers())
			{
				draw_progress_bar(fragbuf, y);
				y += y_step;
			}
			for (auto [index, fragbuf] : channel->getFileFragBuffers())
			{
				draw_progress_bar(fragbuf, y);
				y += y_step;
			}
		}
	}

	if (state == HL::BaseClient::State::Disconnected)
	{
		auto button = IMSCENE->attachTemporaryNode<Shared::SceneHelpers::Smoother<Shared::SceneHelpers::BouncingButtonBehavior<Shared::SceneHelpers::RectangleButton>>>(*gui_holder);
		button->getLabel()->setText("CONNECT");
		button->getLabel()->setFontSize(Common::Helpers::SmoothValueAssign(button->getLabel()->getFontSize(), 18.0f, dTime));
		button->setSize({ 192.0f, 48.0f });
		button->setAnchor({ 0.5f, 0.5f });
		button->setPivot({ 0.5f, 0.5f });
		button->setRounding(0.5f);
		button->setClickCallback([] {
			CONSOLE->execute("connect 192.168.0.106:27015");
		});
	}
	else
	{
		{
			auto button = IMSCENE->attachTemporaryNode< Shared::SceneHelpers::Smoother<Shared::SceneHelpers::BouncingButtonBehavior<Shared::SceneHelpers::RectangleButton>>>(*gui_holder);
			button->getLabel()->setText("DISCONNECT");
			button->getLabel()->setFontSize(Common::Helpers::SmoothValueAssign(button->getLabel()->getFontSize(), 10.0f, dTime));
			button->setSize({ 96.0f, 24.0f });
			button->setAnchor({ 1.0f, 0.0f });
			button->setPivot({ 1.0f, 0.0f });
			button->setPosition({ -8.0f, 8.0f });
			button->setRounding(0.5f);
			button->setClickCallback([] {
				CONSOLE->execute("disconnect");
			});
		}

		{
			auto button = IMSCENE->attachTemporaryNode< Shared::SceneHelpers::Smoother<Shared::SceneHelpers::BouncingButtonBehavior<Shared::SceneHelpers::RectangleButton>>>(*gui_holder);
			if (CLIENT->getUseNavMovement())
				button->getLabel()->setText("NAV");
			else
				button->getLabel()->setText("TRIVIAL");

			button->getLabel()->setFontSize(Common::Helpers::SmoothValueAssign(button->getLabel()->getFontSize(), 10.0f, dTime));
			button->setSize({ 96.0f, 24.0f });
			button->setAnchor({ 1.0f, 0.0f });
			button->setPivot({ 1.0f, 0.0f });
			button->setPosition({ -8.0f, 36.0f });
			button->setRounding(0.5f);
			button->setClickCallback([] {
				CLIENT->setUseNavMovement(!CLIENT->getUseNavMovement());
			});
		}
	}
}
