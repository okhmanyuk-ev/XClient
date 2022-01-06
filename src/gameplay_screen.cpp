#include "gameplay_screen.h"
#include "application.h"

using namespace XClient;

GameplayViewNode::GameplayViewNode() : HL::GameplayViewNode(CLIENT)
{
	setTouchable(true);
}

GameplayScreen::GameplayScreen()
{
	auto gameplay_view_node = std::make_shared<GameplayViewNode>();
	gameplay_view_node->setStretch(1.0f);
	getContent()->attach(gameplay_view_node);
}

void GameplayViewNode::draw()
{
	HL::GameplayViewNode::draw();

	const auto& clientdata = CLIENT->getClientData();
	STATS_INDICATE_GROUP("clientdata", "origin", fmt::format("x: {:.0f}, y: {:.0f}, z: {:.0f}", clientdata.origin.x, clientdata.origin.y, clientdata.origin.z));
	STATS_INDICATE_GROUP("clientdata", "flags", clientdata.flags);

	auto move_target = CLIENT->getCustomMoveTarget();

	if (!move_target.has_value())
		return;

	auto start_pos = CLIENT->getClientData().origin;
	auto end_pos = move_target.value();
	end_pos.z = start_pos.z;

	auto trace_result = CLIENT->getBsp().traceLine(start_pos, end_pos);

	auto mid_pos = trace_result.endpos;

	auto start_scr = worldToScreen(start_pos);
	auto mid_scr = worldToScreen(mid_pos);
	auto end_scr = worldToScreen(end_pos);
	
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

	auto target = screenToWorld(pos / PLATFORM->getScale());
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
