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

	const auto& clientdata = CLIENT->getClientData();
	STATS_INDICATE_GROUP("clientdata", "origin", fmt::format("{:.0f} {:.0f} {:.0f}", clientdata.origin.x, clientdata.origin.y, clientdata.origin.z));
	STATS_INDICATE_GROUP("clientdata", "flags", clientdata.flags);
	STATS_INDICATE_GROUP("clientdata", "maxspeed", fmt::format("{:.0f}", clientdata.maxspeed));

	drawCustomMoveTarget();
}

void GameplayViewNode::drawCustomMoveTarget()
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
	
	auto node = IMSCENE->attachTemporaryNode<HL::GenericDrawNode>(*this);
	node->setStretch(1.0f);
	node->setDrawCallback([node, start_scr, mid_scr, end_scr] {
		GRAPHICS->draw(Renderer::Topology::LineList, {
			{ { start_scr, 0.0f }, { Graphics::Color::Lime, 1.0f } },
			{ { mid_scr, 0.0f }, { Graphics::Color::Lime, 1.0f } },
			{ { mid_scr, 0.0f }, { Graphics::Color::Red, 1.0f } },
			{ { end_scr, 0.0f }, { Graphics::Color::Red, 1.0f } }
		});
	});

	auto circle = IMSCENE->attachTemporaryNode<Scene::Circle>(*this);
	circle->setPosition(end_scr);
	circle->setPivot(0.5f);
	circle->setColor(trace_result.fraction < 1.0f ? Graphics::Color::Red : Graphics::Color::Lime);
	circle->setRadius(4.0f);
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
}
