#include "gameplay_screen.h"
#include <HL/gameplay_view_node.h>
#include "application.h"

using namespace XClient;

GameplayScreen::GameplayScreen()
{
	auto gameplay_view_node = std::make_shared<HL::GameplayViewNode>(CLIENT);
	gameplay_view_node->setStretch(1.0f);
	getContent()->attach(gameplay_view_node);

	setTapCallback([this, gameplay_view_node](const auto& pos) {
		CLIENT->setMoveTarget(gameplay_view_node->screenToWorld(pos));

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
	});
}
