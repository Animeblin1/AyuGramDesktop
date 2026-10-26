#include "ayu/ui/settings/settings_additional.h"

#include "ayu/ui/settings/ayu_builder.h"
#include "ayu/ui/settings/settings_main.h"
#include "ayu/ayu_settings.h"
#include "lang_auto.h"
#include "settings/settings_builder.h"
#include "settings/settings_common.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/fields/number_input.h"
#include "ui/widgets/continuous_sliders.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

#include <QtCore/QTimer>
#include <QtWidgets/QColorDialog>
#include <QtWidgets/QTextEdit>

#include "styles/style_ayu_styles.h"
#include "styles/style_boxes.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"

namespace Settings {

using namespace Builder;
using namespace AyuBuilder;

namespace {

void BuildCaretPreview(SectionBuilder &builder) {
	builder.add([=](const WidgetContext &ctx) -> SectionBuilder::WidgetToAdd {
		auto container = object_ptr<Ui::RpWidget>(ctx.container);
		const auto raw = container.data();
		const auto pad = st::settingsButtonNoIcon.padding;
		raw->setFixedHeight(
			pad.top() + st::defaultInputField.heightMin + pad.bottom());

		const auto field = Ui::CreateChild<Ui::InputField>(
			raw,
			st::defaultInputField,
			tr::ayu_CaretPreviewLabel(),
			u"Hello, smooth caret! abcdefg ABCDEFG 01234"_q);
		raw->sizeValue(
		) | rpl::on_next([=](QSize size) {
			field->setGeometry(
				pad.left(),
				pad.top(),
				size.width() - pad.left() - pad.right(),
				st::defaultInputField.heightMin);
		}, field->lifetime());

		// Demo: glide the caret between the ends - only while visible,
		// so the timer doesn't tick inside a hidden section.
		const auto timer = new QTimer(raw);
		timer->setInterval(1800);
		QObject::connect(timer, &QTimer::timeout, [=] {
			const auto edit = field->rawTextEdit();
			if (!edit->hasFocus()) {
				return;
			}
			auto cursor = edit->textCursor();
			const auto size = edit->toPlainText().size();
			cursor.setPosition((cursor.position() == size) ? 0 : size);
			edit->setTextCursor(cursor);
		});
		raw->shownValue(
		) | rpl::on_next([=](bool shown) {
			if (shown) {
				timer->start();
			} else {
				timer->stop();
			}
		}, field->lifetime());

		return {
			.widget = std::move(container),
		};
	});
}

void BuildWheelStep(SectionBuilder &builder) {
	const auto settings = &AyuSettings::getInstance();
	builder.add([=](const WidgetContext &ctx) -> SectionBuilder::WidgetToAdd {
		const auto container = ctx.container;
		auto wrap = object_ptr<Ui::VerticalLayout>(container);
		const auto row = wrap->add(object_ptr<Ui::RpWidget>(wrap));
		const auto pad = st::settingsButtonNoIcon.padding;
		row->setFixedHeight(
			pad.top() + st::defaultInputField.heightMin + pad.bottom());
		const auto title = Ui::CreateChild<Ui::FlatLabel>(
			row,
			tr::ayu_WheelScrollStep(),
			st::defaultFlatLabel);
		constexpr auto kMin = 50;
		constexpr auto kMax = 400;
		constexpr auto kStep = 10;
		constexpr auto kCount = ((kMax - kMin) / kStep) + 1;
		const auto field = Ui::CreateChild<Ui::NumberInput>(
			row,
			st::defaultInputField,
			rpl::single(QString()),
			QString::number(settings->wheelScrollStep()),
			kMax);
		row->widthValue() | rpl::on_next([=](int width) {
			const auto fieldWidth = st::defaultInputField.style.font->width(u"888 %"_q)
				+ st::defaultInputField.textMargins.left()
				+ st::defaultInputField.textMargins.right();
			field->resize(fieldWidth, st::defaultInputField.heightMin);
			field->moveToRight(pad.right(), pad.top());
			title->resizeToWidth(std::max(
				0,
				width - pad.left() - pad.right() - fieldWidth - pad.left()));
			title->moveToLeft(pad.left(), (row->height() - title->height()) / 2);
		}, row->lifetime());
		auto sliderWithLabel = MakeSliderWithLabel(
			container,
			st::autoDownloadLimitSlider,
			st::settingsScaleLabel,
			0,
			st::settingsScaleLabel.style.font->width(u"888 %"_q),
			true);
		const auto slider = sliderWithLabel.slider;
		const auto label = sliderWithLabel.label;
		wrap->add(
			std::move(sliderWithLabel.widget),
			st::recentStickersLimitPadding);
		slider->setPseudoDiscrete(
			kCount,
			[](int index) { return kMin + index * kStep; },
			settings->wheelScrollStep(),
			[](int value) { AyuSettings::getInstance().setWheelScrollStep(value); },
			[](int value) { AyuSettings::getInstance().setWheelScrollStep(value); });
		settings->wheelScrollStepValue() | rpl::on_next([=](int value) {
			label->setText(QString::number(value) + u" %"_q);
			const auto text = QString::number(value);
			if (field->getLastText() != text) {
				field->setText(text);
			}
			slider->setValue((value - kMin) / float64(kMax - kMin));
		}, wrap->lifetime());
		QObject::connect(field, &Ui::NumberInput::changed, row, [=] {
			auto ok = false;
			const auto value = field->getLastText().toInt(&ok);
			if (ok && value >= kMin && value <= kMax) {
				AyuSettings::getInstance().setWheelScrollStep(value);
			}
		});
		QObject::connect(field, &Ui::NumberInput::blurred, row, [=] {
			field->setText(QString::number(settings->wheelScrollStep()));
		});
		return { .widget = std::move(wrap) };
	}, [] {
		return SearchEntry{
			.id = u"ayu/wheelScrollStep"_q,
			.title = tr::ayu_WheelScrollStep(tr::now),
		};
	});
}

void BuildScrolling(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	const auto settings = &AyuSettings::getInstance();
	builder.addSubsectionTitle(tr::ayu_AdditionalScrollingHeader());

	ayu.addSettingToggle({
		.id = u"ayu/smoothScroll"_q,
		.title = tr::ayu_SmoothScroll(),
		.getter = &AyuSettings::smoothScroll,
		.setter = &AyuSettings::setSmoothScroll,
	});

	ayu.addSlider({
		.id = u"ayu/scrollSpeedSlider"_q,
		.title = tr::ayu_ScrollSmoothness(),
		.steps = 101,
		.current = settings->scrollSpeed(),
		.onChanged = [=](int val) {
			AyuSettings::getInstance().setScrollSpeed(val);
		},
		.formatLabel = [](int val) {
			return QString::number(val) + u" %"_q;
		},
		.value = settings->scrollSpeedValue(),
	});

	BuildWheelStep(builder);

	ayu.addSettingToggle({
		.id = u"ayu/selectScrollX2"_q,
		.title = tr::ayu_SelectionScrollX2(),
		.getter = &AyuSettings::selectScrollX2,
		.setter = &AyuSettings::setSelectScrollX2,
	});

	ayu.addSectionDivider();
}

void BuildCaretStudio(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	auto *settings = &AyuSettings::getInstance();

	builder.addSubsectionTitle(tr::ayu_AdditionalCaretHeader());

	BuildCaretPreview(builder);

	ayu.addSettingToggle({
		.id = u"ayu/smoothCaret"_q,
		.title = tr::ayu_SmoothCaret(),
		.getter = &AyuSettings::smoothCaret,
		.setter = &AyuSettings::setSmoothCaret,
		.value = settings->smoothCaretValue(),
	});
	ayu.addSettingToggle({
		.id = u"ayu/caretBlink"_q,
		.title = tr::ayu_CaretBlink(),
		.getter = &AyuSettings::caretBlink,
		.setter = &AyuSettings::setCaretBlink,
		.value = settings->caretBlinkValue(),
	});
	ayu.addSlider({
		.id = u"ayu/caretSpeedSlider"_q,
		.title = tr::ayu_CaretSpeed(),
		.steps = 101,
		.current = settings->caretSpeed(),
		.onChanged = [=](int val) {
			AyuSettings::getInstance().setCaretSpeed(val);
		},
		.formatLabel = [](int val) {
			return QString::number(val) + u" %"_q;
		},
		.value = settings->caretSpeedValue(),
	});
	ayu.addSlider({
		.id = u"ayu/blinkMsSlider"_q,
		.title = tr::ayu_BlinkRate(),
		.steps = 801,
		.current = settings->blinkMs() - 200,
		.onChanged = [=](int val) {
			AyuSettings::getInstance().setBlinkMs(val + 200);
		},
		.formatLabel = [](int val) {
			return QString::number(val + 200) + u" ms"_q;
		},
		.value = settings->blinkMsValue() | rpl::map([](int value) { return value - 200; }),
	});
	ayu.addSettingToggle({
		.id = u"ayu/caretAccent"_q,
		.title = tr::ayu_CaretAccent(),
		.getter = &AyuSettings::caretAccent,
		.setter = &AyuSettings::setCaretAccent,
		.value = settings->caretAccentValue(),
	});

	builder.addButton({
		.id = u"ayu/caretColor"_q,
		.title = tr::ayu_CaretColor(),
		.st = &st::settingsButtonNoIcon,
		.label = settings->caretCustomColorValue() | rpl::map([](unsigned color) {
			return color
				? (u"#"_q + QString::number(
					color & 0xFFFFFFu,
					16).right(6).toUpper())
				: tr::ayu_CaretColorAuto(tr::now);
		}),
		.onClick = [=] {
			const auto initial = settings->caretCustomColor()
				? QColor::fromRgb(settings->caretCustomColor() & 0xFFFFFFu)
				: QColor(0, 122, 255);
			const auto result = QColorDialog::getColor(
				initial,
				nullptr,
				tr::ayu_CaretColor(tr::now));
			if (result.isValid()) {
				AyuSettings::getInstance().setCaretCustomColor(
					unsigned(result.rgb() & 0xFFFFFFu));
			}
		},
	});

	builder.addButton({
		.id = u"ayu/caretReset"_q,
		.title = tr::ayu_CaretReset(),
		.st = &st::settingsButtonNoIcon,
		.onClick = [=] {
			auto &s = AyuSettings::getInstance();
			s.setCaretSpeed(50);
			s.setCaretBlink(true);
			s.setBlinkMs(500);
			s.setCaretAccent(false);
			s.setCaretCustomColor(0);
			s.setSmoothCaret(true);
		},
	});

	ayu.addSectionDivider();
}

void BuildProfileAndWindows(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	builder.addSubsectionTitle(tr::ayu_AdditionalProfileWindowsHeader());

	ayu.addSettingToggle({
		.id = u"ayu/profileGradientFromAvatar"_q,
		.title = tr::ayu_ProfileGradientFromAvatar(),
		.getter = &AyuSettings::profileGradientFromAvatar,
		.setter = &AyuSettings::setProfileGradientFromAvatar,
	});

	ayu.addSettingToggle({
		.id = u"ayu/webAppsWindowed"_q,
		.title = tr::ayu_WebAppsWindowed(),
		.getter = &AyuSettings::webAppsWindowed,
		.setter = &AyuSettings::setWebAppsWindowed,
	});

	ayu.addSectionDivider();
}

void BuildChatActivityAndTools(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	builder.addSubsectionTitle(tr::ayu_AdditionalChatToolsHeader());

	ayu.addSettingToggle({
		.id = u"ayu/senderMiniAvatars"_q,
		.title = tr::ayu_MiniAvatars(),
		.getter = &AyuSettings::senderMiniAvatars,
		.setter = &AyuSettings::setSenderMiniAvatars,
	});

	ayu.addSettingToggle({
		.id = u"ayu/onlineIndicator"_q,
		.title = tr::ayu_OnlineIndicator(),
		.getter = &AyuSettings::onlineIndicator,
		.setter = &AyuSettings::setOnlineIndicator,
	});

	ayu.addSettingToggle({
		.id = u"ayu/liveUserActivity"_q,
		.title = tr::ayu_LiveUserActivity(),
		.getter = &AyuSettings::liveUserActivity,
		.setter = &AyuSettings::setLiveUserActivity,
	});

	ayu.addSettingToggle({
		.id = u"ayu/inlineCalculator"_q,
		.title = tr::ayu_InlineCalculator(),
		.getter = &AyuSettings::inlineCalculator,
		.setter = &AyuSettings::setInlineCalculator,
	});
}

const auto kMeta = BuildHelper({
	.id = AyuAdditional::Id(),
	.parentId = AyuMain::Id(),
	.title = &tr::ayu_CategoryAdditional,
	.icon = &st::menuIconSettings,
}, [](SectionBuilder &builder) {
	auto ayu = AyuSectionBuilder(builder);

	builder.addSkip();
	BuildScrolling(builder, ayu);
	BuildCaretStudio(builder, ayu);
	BuildProfileAndWindows(builder, ayu);
	BuildChatActivityAndTools(builder, ayu);
	builder.addSkip();
});

} // namespace

rpl::producer<QString> AyuAdditional::title() {
	return tr::ayu_CategoryAdditional();
}

AyuAdditional::AyuAdditional(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

void AyuAdditional::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kMeta.build);
	Ui::ResizeFitChild(this, content);
}

Type AyuAdditionalId() {
	return AyuAdditional::Id();
}

} // namespace Settings
