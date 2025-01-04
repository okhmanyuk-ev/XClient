#include "gameplay_screen.h"
#include "application.h"

using namespace XClient;

GameplayViewNode::GameplayViewNode() : HL::GameplayViewNode(CLIENT)
{
	setTouchable(true);

	CONSOLE->registerCVar("r_draw_3d_bsp", { "bool" }, CVAR_GETTER_BOOL(mDraw3dBsp), CVAR_SETTER_BOOL(mDraw3dBsp));
	CONSOLE->registerCVar("r_draw_2d_navmesh", { "int" }, CVAR_GETTER_INT(mDraw2dNavmesh), CVAR_SETTER_INT(mDraw2dNavmesh));
}

GameplayViewNode::~GameplayViewNode()
{
	CONSOLE->removeCVar("r_draw_3d_bsp");
	CONSOLE->removeCVar("r_draw_2d_navmesh");
}

void GameplayViewNode::draw()
{
	HL::GameplayViewNode::draw();

	if (CLIENT->getState() != HL::BaseClient::State::GameStarted)
		return;

	draw3dView();
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

	end_pos = sky::ease_towards(end_pos.value(), move_target.value(), dTime);

	end_pos.value().z = start_pos.z;

	auto trace_result = CLIENT->traceLine(start_pos, end_pos.value());

	auto mid_pos = trace_result.endpos;

	auto start_scr = worldToScreen(start_pos);
	auto mid_scr = worldToScreen(mid_pos);
	auto end_scr = worldToScreen(end_pos.value());

	auto node = IMSCENE->spawn<HL::GenericDrawNode>(holder);
	node->setStretch(1.0f);
	node->setDrawCallback([node, start_scr, mid_scr, end_scr] {
		GRAPHICS->draw(nullptr, nullptr, skygfx::Topology::LineList, std::vector<skygfx::utils::Mesh::Vertex>{
			{ .pos = { start_scr, 0.0f }, .color = { Graphics::Color::Lime, 1.0f } },
			{ .pos = { mid_scr, 0.0f }, .color = { Graphics::Color::Lime, 1.0f } },
			{ .pos = { mid_scr, 0.0f }, .color = { Graphics::Color::Red, 1.0f } },
			{ .pos = { end_scr, 0.0f }, .color = { Graphics::Color::Red, 1.0f } }
		});
	});

	auto circle = IMSCENE->spawn<Scene::Circle>(holder);
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

	end_pos = sky::ease_towards(end_pos.value(), move_target.value(), dTime);

	auto end_scr = worldToScreen(end_pos.value());

	auto node = IMSCENE->spawn<HL::GenericDrawNode>(holder);
	node->setStretch(1.0f);
	node->setDrawCallback([this, start_pos, end_scr, dTime] {
		if (CLIENT->getState() != HL::BaseClient::State::GameStarted)
			return;

		GRAPHICS->draw(nullptr, nullptr, skygfx::utils::MeshBuilder::Mode::Lines, [&](auto vertex) {
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
				g_chain[i] = sky::ease_towards(g_chain.at(i), chain_area->position, dTime);
				//g_chain[i] = std::next(chain.begin(), i)->lock()->position;
			}

			std::optional<glm::vec3> prev_v;

			for (auto pos : g_chain)
			{
				if (prev_v.has_value())
				{
					vertex(skygfx::utils::Mesh::Vertex{
						.pos = { worldToScreen(prev_v.value()), 0.0f },
						.color = { Graphics::Color::Yellow, 1.0f }
					});
					vertex(skygfx::utils::Mesh::Vertex{
						.pos = { worldToScreen(pos), 0.0f },
						.color = { Graphics::Color::Yellow, 1.0f }
					});
				}
				prev_v = pos;
			}

			if (!prev_v.has_value())
				prev_v = start_pos;

			vertex(skygfx::utils::Mesh::Vertex{
				.pos = { worldToScreen(prev_v.value()), 0.0f },
				.color = { Graphics::Color::Yellow, 1.0f }
			});
			vertex(skygfx::utils::Mesh::Vertex{
				.pos = { end_scr, 0.0f },
				.color = { Graphics::Color::Yellow, 1.0f }
			});
		});
	});

	auto circle = IMSCENE->spawn<Scene::Circle>(holder);
	circle->setPosition(end_scr);
	circle->setPivot(0.5f);
	circle->setColor(Graphics::Color::Yellow);
	circle->setRadius(4.0f);
}

void GameplayViewNode::draw3dView()
{
//	if (!mDraw3dBsp)
//		return;
//
//	if (!mBspDraw.has_value() || mBspDraw.value().first != getShortMapName())
//		mBspDraw = { getShortMapName(), std::make_shared<HL::BspDraw>(CLIENT->getBsp()) };
//
//	auto target = GRAPHICS->getRenderTarget("bsp_3d_view", 800, 600);
//	static auto pos = glm::vec3{};
//	pos = Common::Helpers::SmoothValueAssign(pos, CLIENT->getOrigin(), FRAME->getTimeDelta());
//	auto angles = CLIENT->getAngles();
//	mBspDraw.value().second->draw(target, pos, glm::radians(angles.x), glm::radians(angles.y));
//	draw3dNavMesh(target, pos, angles);
//
//	ImGui::Begin("3D View");
//	auto width = ImGui::GetContentRegionAvail().x;
//	Shared::SceneEditor::drawImage(target, std::nullopt, width);
//	ImGui::End();
}

void GameplayViewNode::draw3dNavMesh(std::shared_ptr<skygfx::RenderTarget> target, const glm::vec3& pos, const glm::vec3& angles)
{
//	static auto camera = std::make_shared<Graphics::Camera3D>();
//	camera->setWorldUp({ 0.0f, 0.0f, 1.0f });
//	camera->setPosition(pos);
//	camera->setYaw(glm::radians(angles.x));
//	camera->setPitch(glm::radians(angles.y));
//	camera->onFrame();
//
//	auto view = camera->getViewMatrix();
//	auto projection = camera->getProjectionMatrix();
//	auto prev_batching = GRAPHICS->isBatching();
//
//	GRAPHICS->setBatching(false);
//	GRAPHICS->pushCleanState();
//	GRAPHICS->pushViewMatrix(view);
//	GRAPHICS->pushProjectionMatrix(projection);
//	GRAPHICS->pushRenderTarget(target);
//	GRAPHICS->pushDepthMode(skygfx::ComparisonFunc::Less);
//	
//	const auto& nav = CLIENT->getNavMesh();
//	
//	if (!nav.explored_areas.empty())
//	{
//		static auto builder = Graphics::MeshBuilder();
//		builder.begin();
//
//		for (auto area : nav.explored_areas)
//		{
//			auto v1 = area->position;
//			auto v2 = v1 + glm::vec3{ 0.0f, 0.0f, 8.0f };
//
//			builder.color(Graphics::Color::Blue);
//			builder.vertex(v1);
//			builder.vertex(v2);
//
//			for (auto dir : { NavDirection::Forward, NavDirection::Right })
//			{
//				if (!area->neighbours.contains(dir))
//					continue;
//
//				auto neighbour = area->neighbours.at(dir);
//
//				if (!neighbour.has_value())
//					continue;
//
//				auto v3 = neighbour.value().lock()->position + glm::vec3{ 0.0f, 0.0f, 8.0f };
//
//				builder.color(Graphics::Color::Yellow);
//				builder.vertex(v2);
//				builder.vertex(v3);
//			}
//		}
//
//		auto [vertices, count] = builder.end();
//
//		GRAPHICS->draw(skygfx::Topology::LineList, vertices, count);
//	}
//	
//	GRAPHICS->pop(5);
//	GRAPHICS->setBatching(prev_batching);
}

void GameplayViewNode::draw2dNavMesh(Scene::Node& holder)
{
	if (mDraw2dNavmesh == 0)
		return;

	auto node = IMSCENE->spawn<HL::GenericDrawNode>(holder);
	node->setStretch(1.0f);
	node->setDrawCallback([this] {
		if (CLIENT->getState() != HL::BaseClient::State::GameStarted)
			return;

		const auto& nav = CLIENT->getNavMesh();

		if (mDraw2dNavmesh == 1)
		{
			NavMesh::AreaSet border_areas;

			for (auto area : nav.explored_areas)
			{
				if (!area->isBorder())
					continue;

				border_areas.insert(area);
			}

			/*for (auto area : border_areas)
			{
				auto scr_pos = worldToScreen(area->position);

				auto model = getTransform();
				model = glm::translate(model, { scr_pos, 0.0f });
				model = glm::scale(model, { 3.0f, 3.0f, 0.5f });

				GRAPHICS->pushModelMatrix(model);
				GRAPHICS->drawCircleTexture({ Graphics::Color::Lime, 1.0f });
				GRAPHICS->pop();
			}*/

			if (border_areas.empty())
				return;

			GRAPHICS->draw(nullptr, nullptr, skygfx::utils::MeshBuilder::Mode::Lines, [&](auto vertex) {
				std::function<void(std::shared_ptr<NavArea>)> recursiveBorderDraw = [&](std::shared_ptr<NavArea> area) {
					border_areas.erase(area);

					NavMesh::AreaSet targets;

					for (auto [dir, neighbour] : area->neighbours)
					{
						if (!neighbour.has_value())
							continue;

						auto neighbour_nn = neighbour.value().lock();

						targets.insert(neighbour_nn);
					}

					for (auto horz_dir : { NavDirection::Left, NavDirection::Right })
					{
						if (!area->neighbours.contains(horz_dir))
							continue;

						if (!area->neighbours.at(horz_dir).has_value())
							continue;

						auto horz_neighbour_nn = area->neighbours.at(horz_dir).value().lock();

						for (auto vert_dir : { NavDirection::Forward, NavDirection::Back })
						{
							if (!horz_neighbour_nn->neighbours.contains(vert_dir))
								continue;

							if (!horz_neighbour_nn->neighbours.at(vert_dir).has_value())
								continue;

							auto diagonal_neighbour_nn = horz_neighbour_nn->neighbours.at(vert_dir).value().lock();

							targets.insert(diagonal_neighbour_nn);
						}
					}

					for (auto target : targets)
					{
						if (!border_areas.contains(target))
							continue;

						vertex(skygfx::utils::Mesh::Vertex{
							.pos = { worldToScreen(area->position), 0.0f },
							.color = { Graphics::Color::Lime, 1.0f }
						});
						vertex(skygfx::utils::Mesh::Vertex{
							.pos = { worldToScreen(target->position), 0.0f },
							.color = { Graphics::Color::Lime, 1.0f }
						});
						recursiveBorderDraw(target);
					}
				};

				while (!border_areas.empty())
				{
					recursiveBorderDraw(*border_areas.begin());
				}
			});
		}
		else
		{
			const auto& areas = mDraw2dNavmesh == 2 ? nav.explored_areas : nav.unexplored_areas;

			if (areas.empty())
				return;


			GRAPHICS->draw(nullptr, nullptr, skygfx::utils::MeshBuilder::Mode::Lines, [&](auto vertex) {
				using NavAreaIndex = size_t;

				std::unordered_set<NavAreaIndex> blacklist;

				for (auto area : areas)
				{
					auto v1 = area->position;

					auto area_index = reinterpret_cast<NavAreaIndex>(area.get());

					blacklist.insert(area_index);

					for (auto dir : Directions)
					{
						if (!area->neighbours.contains(dir))
							continue;

						auto neighbour = area->neighbours.at(dir);

						if (!neighbour.has_value())
							continue;

						auto neighbour_nn = neighbour.value().lock();
						auto neighbour_index = reinterpret_cast<NavAreaIndex>(neighbour_nn.get());

						if (blacklist.contains(neighbour_index))
							continue;

						auto opposite_dir = OppositeDirections.at(dir);

						glm::vec4 color;
						if (!neighbour_nn->neighbours.contains(opposite_dir))
							color = { Graphics::Color::Red, 0.5f };
						else if (!neighbour_nn->neighbours.at(opposite_dir).has_value())
							color = { Graphics::Color::Blue, 0.5f };
						else
							color = { Graphics::Color::White, 0.5f };

						auto v2 = neighbour_nn->position;

						vertex(skygfx::utils::Mesh::Vertex{
							.pos = { worldToScreen(v1), 0.0f },
							.color = color
						});
						vertex(skygfx::utils::Mesh::Vertex{
							.pos = { worldToScreen(v2), 0.0f },
							.color = color
						});
					}
				}
			});
		}
	});
}

void GameplayViewNode::touch(Touch type, const glm::vec2& pos)
{
	HL::GameplayViewNode::touch(type, pos);

	if (CLIENT->getState() < HL::BaseClient::State::GameStarted)
		return;

	auto unprojected_pos = getBackgroundNode()->unproject(pos);

	auto target = screenToWorld(unprojected_pos);
	CLIENT->setCustomMoveTarget(target);

	if (type == Touch::Begin)
	{
		auto circle = std::make_shared<Scene::Circle>();
		circle->setPivot(0.5f);
		circle->setPosition(unprojected_pos);
		circle->setRadius(0.0f);
		getBackgroundNode()->attach(circle);

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
}

void GameplayScreen::draw()
{
	Shared::SceneHelpers::StandardScreen::draw();

	auto gameplay_holder = IMSCENE->spawn<Shared::SceneHelpers::SafeArea>(*getContent());
	auto gui_holder = IMSCENE->spawn<Shared::SceneHelpers::SafeArea>(*getContent());

	auto state = CLIENT->getState();
	auto dTime = FRAME->getTimeDelta();

	static std::weak_ptr<Scene::Node> LoadingLabel;

	if (state == HL::BaseClient::State::GameStarted && LoadingLabel.expired())
	{
		auto gameplay_view = IMSCENE->spawn<GameplayViewNode>(*gameplay_holder);
		gameplay_view->setStretch(1.0f);
		IMSCENE->dontKillWhileHaveChilds();

		{
			auto checkbox = IMSCENE->spawn<Shared::SceneHelpers::Smoother<Shared::SceneHelpers::Checkbox>>(*gui_holder);
			checkbox->setSize({ 96.0f, 24.0f });
			checkbox->setAnchor({ 1.0f, 0.0f });
			checkbox->setPosition({ -8.0f, 36.0f });
			checkbox->getLabel()->setText(L"NAV");
			checkbox->getLabel()->setFontSize(10.0f);
			checkbox->getOuterRectangle()->setRounding(0.5f);
			checkbox->getInnerRectangle()->setRounding(0.5f);
			checkbox->setChecked(CLIENT->getUseNavMovement());
			checkbox->setChangeCallback([](bool checked) {
				CLIENT->setUseNavMovement(checked);
			});

			if (IMSCENE->isFirstCall())
				checkbox->setPivot({ 0.0f, 0.0f });
			else
				checkbox->setPivot({ 1.0f, 0.0f });

			IMSCENE->destroyAction(Actions::Collection::MakeSequence(
				Actions::Collection::Execute([checkbox] {
					checkbox->setSmoothTransform(false);
				}),
				Actions::Collection::ChangePivot(checkbox, { 0.0f, 0.0f }, 0.25f, Easing::CubicIn)
			));
		}

		{
			auto checkbox = IMSCENE->spawn<Shared::SceneHelpers::Smoother<Shared::SceneHelpers::Checkbox>>(*gui_holder);
			checkbox->setSize({ 96.0f, 24.0f });
			checkbox->setAnchor({ 1.0f, 0.0f });
			checkbox->setPosition({ -8.0f, 64.0f });
			checkbox->getLabel()->setText(L"CENTERIZED");
			checkbox->getLabel()->setFontSize(10.0f);
			checkbox->getOuterRectangle()->setRounding(0.5f);
			checkbox->getInnerRectangle()->setRounding(0.5f);
			checkbox->setChecked(gameplay_view->isCenterized());
			checkbox->setChangeCallback([gameplay_view](bool checked) {
				gameplay_view->setCenterized(checked);
			});

			if (IMSCENE->isFirstCall())
				checkbox->setPivot({ 0.0f, 0.0f });
			else
				checkbox->setPivot({ 1.0f, 0.0f });

			IMSCENE->destroyAction(Actions::Collection::MakeSequence(
				Actions::Collection::Execute([checkbox] {
					checkbox->setSmoothTransform(false);
				}),
				Actions::Collection::ChangePivot(checkbox, { 0.0f, 0.0f }, 0.25f, Easing::CubicIn)
			));
		}
	}
	else if (state != HL::BaseClient::State::Disconnected && state != HL::BaseClient::State::GameStarted)
	{
		auto label = IMSCENE->spawn<Scene::Label>(*gameplay_holder);
		label->setText(L"LOADING...");
		label->setFontSize(32.0f);
		label->setAnchor(0.5f);
		label->setPivot(0.5f);
		IMSCENE->showAndHideWithScale();
		LoadingLabel = label;

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

				auto progressbar = IMSCENE->spawn<Scene::ClippableStencil<Shared::SceneHelpers::Progressbar>>(*gameplay_holder);
				progressbar->setWidth(512.0f);
				progressbar->setHeight(8.0f);
				progressbar->setAnchor(0.5f);
				progressbar->setPivot(0.5f);
				progressbar->setY(y + 32.0f);
				progressbar->setRounding(1.0f);
				progressbar->setSlicedSpriteOptimizationEnabled(false); // this enable nice clipping
				progressbar->setProgress(sky::ease_towards(progressbar->getProgress(), progress, dTime));
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

	if (state == HL::BaseClient::State::Disconnected && !gameplay_holder->hasNodes())
	{
		auto editbox = IMSCENE->spawn<Shared::SceneHelpers::BouncingButtonBehavior<Shared::SceneHelpers::Editbox>>(*gui_holder);
		editbox->setRounding(0.5f);
		editbox->setSize({ 192.0f, 32.0f });
		editbox->setAnchor(0.5);
		editbox->setPivot(0.5f);
		editbox->setY(-24.0f);
		IMSCENE->showAndHideWithScale();
		if (IMSCENE->isFirstCall())
		{
		//	editbox->getLabel()->setText("192.168.0.106:27015");
			editbox->getLabel()->setText(L"127.0.0.1:27015");
		}

		auto button = IMSCENE->spawn<Shared::SceneHelpers::Smoother<Shared::SceneHelpers::BouncingButtonBehavior<Shared::SceneHelpers::RectangleButton>>>(*gui_holder);
		if (IMSCENE->isFirstCall())
		{
			button->getLabel()->setFontSize(18.0f);
		}
		button->getLabel()->setText(L"CONNECT");
		button->getLabel()->setFontSize(sky::ease_towards(button->getLabel()->getFontSize(), 18.0f, dTime));
		button->setSize({ 192.0f, 48.0f });
		button->setAnchor(0.5f);
		button->setPivot(0.5f);
		button->setRounding(0.5f);
		button->setPosition({ 0.0f, 24.0f });
		button->setClickCallback([editbox = std::weak_ptr(editbox)] {
			if (editbox.expired())
				return;

			CONSOLE->execute("connect " + sky::to_string(editbox.lock()->getLabel()->getText()));
		});
	}
	else
	{
		auto button = IMSCENE->spawn<Shared::SceneHelpers::Smoother<Shared::SceneHelpers::BouncingButtonBehavior<Shared::SceneHelpers::RectangleButton>>>(*gui_holder);
		button->getLabel()->setText(L"DISCONNECT");
		button->getLabel()->setFontSize(sky::ease_towards(button->getLabel()->getFontSize(), 10.0f, dTime));
		button->setSize({ 96.0f, 24.0f });
		button->setAnchor({ 1.0f, 0.0f });
		button->setPivot({ 1.0f, 0.0f });
		button->setPosition({ -8.0f, 8.0f });
		button->setRounding(0.5f);
		button->setClickCallback([] {
			CONSOLE->execute("disconnect");
		});
	}
}
