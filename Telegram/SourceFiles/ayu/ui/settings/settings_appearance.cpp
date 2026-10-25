// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/settings/settings_appearance.h"

#include "lang_auto.h"
#include "ayu/ayu_settings.h"
#include "ayu/ayu_ui_settings.h"
#include "ayu/ui/boxes/font_selector.h"
#include "ayu/ui/components/avatar_corners_preview.h"
#include "ayu/ui/components/icon_picker.h"
#include "ayu/ui/settings/ayu_builder.h"
#include "ayu/ui/settings/settings_ayu_utils.h"
#include "ayu/ui/settings/settings_main.h"
#include "inline_bots/bot_attach_web_view.h"
#include "main/main_session.h"
#include "settings/settings_builder.h"
#include "settings/settings_common.h"
#include "styles/style_ayu_icons.h"
#include "styles/style_ayu_styles.h"
#include "styles/style_boxes.h"
#include "styles/style_dialogs.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "ui/painter.h"
#include "ui/widgets/continuous_sliders.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/padding_wrap.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

#include <QtCore/QTimer>
#include <QtWidgets/QColorDialog>
#include <QtWidgets/QTextEdit>

namespace Settings {

using namespace Builder;
using namespace AyuBuilder;

namespace {

bool HasDrawerBots(not_null<Window::SessionController*> controller) {
	// todo: maybe iterate through all accounts
	const auto bots = &controller->session().attachWebView();
	for (const auto &bot : bots->attachBots()) {
		if (!bot.inMainMenu || !bot.media) {
			continue;
		}
		return true;
	}
	return false;
}

void BuildAppIcon(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	builder.addSubsectionTitle({
		.id = u"ayu/appIcon"_q,
		.title = tr::ayu_AppIconHeader(),
	});

	builder.add([](const WidgetContext &ctx) -> SectionBuilder::WidgetToAdd {
		return {
			.widget = object_ptr<IconPicker>(ctx.container),
			.margin = st::settingsButtonNoIcon.padding,
		};
	});

#if defined Q_OS_WIN || defined Q_OS_MAC
	builder.addDivider();
	builder.addSkip();
	ayu.addSettingToggle({
		.id = u"ayu/hideNotificationBadge"_q,
		.title = tr::ayu_HideNotificationBadge(),
		.getter = &AyuSettings::hideNotificationBadge,
		.setter = &AyuSettings::setHideNotificationBadge,
	});
	builder.addSkip();
	builder.addDividerText(tr::ayu_HideNotificationBadgeDescription());
	builder.addSkip();
#else
    builder.addDivider();
    builder.addSkip();
#endif
}

void BuildAvatarCorners(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	auto *settings = &AyuSettings::getInstance();
	const auto controller = builder.controller();

	const auto mapRadius = [](int val)
	{
		if (val == 0) {
			return tr::ayu_AvatarCornersSquare(tr::now).toUpper();
		} else if (val == AyuUiSettings::kMaxAvatarCorners) {
			return tr::ayu_AvatarCornersCircle(tr::now).toUpper();
		}
		return QString::number(val);
	};

	builder.add([=](const WidgetContext &ctx) -> SectionBuilder::WidgetToAdd {
		const auto container = ctx.container;
		auto title = object_ptr<Ui::FlatLabel>(
			container,
			tr::ayu_AvatarCorners(),
			st::defaultSubsectionTitle);
		const auto titleRaw = title.data();

		const auto badge = Ui::CreateChild<Ui::PaddingWrap<Ui::FlatLabel>>(
			container,
			object_ptr<Ui::FlatLabel>(
				container,
				settings->avatarCornersValue() | rpl::map(mapRadius),
				st::settingsPremiumNewBadge),
			st::ayuBetaBadgePadding);
		badge->show();
		badge->setAttribute(Qt::WA_TransparentForMouseEvents);
		badge->paintRequest() | rpl::on_next([=] {
			auto p = QPainter(badge);
			auto hq = PainterHighQualityEnabler(p);
			p.setPen(Qt::NoPen);
			p.setBrush(st::windowBgActive);
			const auto r = st::ayuBetaBadgePadding.left();
			p.drawRoundedRect(badge->rect(), r, r);
		}, badge->lifetime());

		titleRaw->geometryValue() | rpl::on_next([=](QRect geometry) {
			badge->moveToLeft(
				geometry.x()
					+ titleRaw->textMaxWidth()
					+ st::settingsPremiumNewBadgePosition.x(),
				geometry.y()
					+ (geometry.height() - badge->height()) / 2);
		}, badge->lifetime());

		return {
			.widget = std::move(title),
			.margin = st::defaultSubsectionTitlePadding,
		};
	}, [] {
		return SearchEntry{
			.id = u"ayu/avatarCorners"_q,
			.title = tr::ayu_AvatarCorners(tr::now),
		};
	});

	auto *previewRaw = static_cast<AvatarCornersPreview*>(nullptr);
	builder.add([&](const Builder::WidgetContext &ctx) -> SectionBuilder::WidgetToAdd {
		auto preview = object_ptr<AvatarCornersPreview>(
			ctx.container,
			controller);
		previewRaw = preview.data();
		const auto vMargin = st::settingsButtonNoIcon.padding
			- st::defaultDialogRow.padding;
		return {
			.widget = std::move(preview),
			.margin = QMargins(0, vMargin.top(), 0, vMargin.bottom()),
		};
	});

	ayu.addSlider({
		.id = u"ayu/avatarCornersSlider"_q,
		.title = rpl::single(QString()),
		.showTitle = false,
		.steps = AyuUiSettings::kMaxAvatarCorners + 1,
		.current = settings->avatarCorners(),
		.formatLabel = [](int val) {
			return QString::number(val) + u" px"_q;
		},
		.onChanged = [=](int val) {
			AyuSettings::getInstance().setAvatarCorners(val);
			if (previewRaw) {
				previewRaw->update();
			}
		},
		.onFinalChanged = [=](int val) {
			AyuSettings::getInstance().setAvatarCorners(val);
			ShowRestartPrompt(controller);
		},
	});

	ayu.addSettingToggle({
		.id = u"ayu/singleCornerRadius"_q,
		.title = tr::ayu_SingleCornerRadius(),
		.getter = &AyuSettings::singleCornerRadius,
		.setter = &AyuSettings::setSingleCornerRadius,
	});

	builder.addSkip();
	builder.addDividerText(tr::ayu_SingleCornerRadiusDescription());
	builder.addSkip();
}

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
			QString("Hello, smooth caret! abcdefg ABCDEFG 01234"));
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
			auto cursor = edit->textCursor();
			const auto size = edit->toPlainText().size();
			cursor.setPosition((cursor.position() == size) ? 0 : size);
			edit->setTextCursor(cursor);
		});
		raw->shownValue(
		) | rpl::on_next([=](bool shown) {
			if (shown) {
				field->setFocusFast();
				timer->start();
			} else {
				timer->stop();
			}
		}, field->lifetime());

		return {
			.widget = std::move(container),
			.margin = st::settingsButtonNoIcon.padding,
		};
	});
}

void BuildCaretStudio(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	auto *settings = &AyuSettings::getInstance();

	builder.addSubsectionTitle(tr::ayu_CaretStudioHeader());

	BuildCaretPreview(builder);

	ayu.addSettingToggle({
		.id = u"ayu/smoothCaret"_q,
		.title = tr::ayu_SmoothCaret(),
		.getter = &AyuSettings::smoothCaret,
		.setter = &AyuSettings::setSmoothCaret,
	});
	ayu.addSettingToggle({
		.id = u"ayu/caretBlink"_q,
		.title = tr::ayu_CaretBlink(),
		.getter = &AyuSettings::caretBlink,
		.setter = &AyuSettings::setCaretBlink,
	});
	ayu.addSlider({
		.id = u"ayu/caretSpeedSlider"_q,
		.title = tr::ayu_CaretSpeed(),
		.steps = 101,
		.current = settings->caretSpeed(),
		.formatLabel = [](int val) {
			return QString::number(val) + u" %"_q;
		},
		.onChanged = [=](int val) {
			AyuSettings::getInstance().setCaretSpeed(val);
		},
	});
	ayu.addSlider({
		.id = u"ayu/blinkMsSlider"_q,
		.title = tr::ayu_BlinkRate(),
		.steps = 801,
		.current = settings->blinkMs() - 200,
		.formatLabel = [](int val) {
			return QString::number(val + 200) + u" ms"_q;
		},
		.onChanged = [=](int val) {
			AyuSettings::getInstance().setBlinkMs(val + 200);
		},
	});
	ayu.addSettingToggle({
		.id = u"ayu/caretAccent"_q,
		.title = tr::ayu_CaretAccent(),
		.getter = &AyuSettings::caretAccent,
		.setter = &AyuSettings::setCaretAccent,
	});

	builder.addButton({
		.id = u"ayu/caretColor"_q,
		.title = tr::ayu_CaretColor(),
		.st = &st::settingsButtonNoIcon,
		.label = rpl::single(settings->caretCustomColor()
			? (QString(u"#") + QString::number(
				settings->caretCustomColor() & 0xFFFFFFu,
				16).right(6).toUpper())
			: tr::ayu_CaretColorAuto(tr::now)),
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
		.label = rpl::single(tr::ayu_CaretReset(tr::now)),
		.onClick = [=] {
			auto &s = AyuSettings::getInstance();
			s.setCaretSpeed(50);
			s.setCaretBlink(true);
			s.setBlinkMs(500);
			s.setCaretAccent(false);
			s.setCaretCustomColor(0);
			s.setScrollSpeed(50);
			s.setWheelScrollStep(100);
		},
	});

	ayu.addSlider({
		.id = u"ayu/scrollSpeedSlider"_q,
		.title = tr::ayu_ScrollSmoothness(),
		.steps = 101,
		.current = settings->scrollSpeed(),
		.formatLabel = [](int val) {
			return QString::number(val) + u" %"_q;
		},
		.onChanged = [=](int val) {
			AyuSettings::getInstance().setScrollSpeed(val);
		},
	});

		// Ayu: wheel notch step as a percentage, slider with a live label
		// plus a numeric input field synced both ways.
		builder.add([=](const WidgetContext &ctx) -> SectionBuilder::WidgetToAdd {
			const auto container = ctx.container;

			auto sliderWithLabel = MakeSliderWithLabel(
				container,
				st::autoDownloadLimitSlider,
				st::settingsScaleLabel,
				0,
				st::settingsScaleLabel.style.font->width("888 %"),
				true);
			const auto slider = sliderWithLabel.slider;
			const auto label = sliderWithLabel.label;

			constexpr auto kMin = 50;
			constexpr auto kMax = 400;
			constexpr auto kStep = 10;
			constexpr auto kCount = ((kMax - kMin) / kStep) + 1;
			const auto toPercent = [](int index) {
				return kMin + index * kStep;
			};

			const auto current = container->lifetime().make_state<int>(
				std::clamp(
					settings->wheelScrollStep(),
					kMin,
					kMax));

			auto field = object_ptr<Ui::InputField>(
				container,
				st::defaultInputField,
				tr::ayu_WheelScrollStep(),
				QString::number(*current));
			const auto fieldRaw = field.data();
			fieldRaw->setMaxLength(4);

			const auto applyPercent = [=](int percent) {
				const auto clamped = std::clamp(percent, kMin, kMax);
				if (*current == clamped) {
					return;
				}
				*current = clamped;
				AyuSettings::getInstance().setWheelScrollStep(clamped);
				label->setText(QString::number(clamped) + u" %"_q);
				if (fieldRaw->getLastText()
					!= QString::number(clamped)) {
					fieldRaw->setText(QString::number(clamped));
				}
				slider->setValue(
					(clamped - kMin) / float64(kMax - kMin));
			};

			slider->setPseudoDiscrete(
				kCount,
				toPercent,
				*current,
				[=](int percent) { applyPercent(percent); },
				[=](int percent) { applyPercent(percent); });
			label->setText(QString::number(*current) + u" %"_q);

			fieldRaw->changes(
			) | rpl::on_next([=] {
				auto ok = false;
				const auto value = fieldRaw->getLastText().toInt(&ok);
				if (ok && value >= kMin && value <= kMax) {
					applyPercent(value);
				}
			}, fieldRaw->lifetime());

			auto wrap = object_ptr<Ui::VerticalLayout>(container);
			const auto inner = wrap.data();
			inner->add(
				std::move(sliderWithLabel.widget),
				st::recentStickersLimitPadding);
			inner->add(
				std::move(field),
				st::settingsButtonNoIcon.padding);

			return {
				.widget = std::move(wrap),
			};
		});

	builder.addSkip();
}

void BuildAppearance(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	auto *settings = &AyuSettings::getInstance();

	builder.addSubsectionTitle(tr::ayu_CategoryAppearance());

	ayu.addSettingToggle({
		.id = u"ayu/materialSwitches"_q,
		.altIds = { u"ayu/newSwitchStyle"_q },
		.title = tr::ayu_MaterialSwitches(),
		.getter = &AyuSettings::materialSwitches,
		.setter = &AyuSettings::setMaterialSwitches,
	});
	BuildCaretStudio(builder, ayu);
	ayu.addSettingToggle({
		.id = u"ayu/smoothScroll"_q,
		.title = tr::ayu_SmoothScroll(),
		.getter = &AyuSettings::smoothScroll,
		.setter = &AyuSettings::setSmoothScroll,
	});
	ayu.addSettingToggle({
		.id = u"ayu/profileGradientFromAvatar"_q,
		.title = tr::ayu_ProfileGradientFromAvatar(),
		.getter = &AyuSettings::profileGradientFromAvatar,
		.setter = &AyuSettings::setProfileGradientFromAvatar,
	});
	ayu.addSettingToggle({
		.id = u"ayu/selectScrollX2"_q,
		.title = tr::ayu_SelectionScrollX2(),
		.getter = &AyuSettings::selectScrollX2,
		.setter = &AyuSettings::setSelectScrollX2,
	});
	ayu.addSettingToggle({
		.id = u"ayu/webAppsWindowed"_q,
		.title = tr::ayu_WebAppsWindowed(),
		.getter = &AyuSettings::webAppsWindowed,
		.setter = &AyuSettings::setWebAppsWindowed,
	});
	ayu.addSettingToggle({
		.id = u"ayu/disableCustomBackgrounds"_q,
		.altIds = { u"ayu/customThemes"_q },
		.title = tr::ayu_DisableCustomBackgrounds(),
		.getter = &AyuSettings::disableCustomBackgrounds,
		.setter = &AyuSettings::setDisableCustomBackgrounds,
	});
	ayu.addSettingToggle({
		.id = u"ayu/hidePremiumStatuses"_q,
		.title = tr::ayu_HidePremiumStatuses(),
		.getter = &AyuSettings::hidePremiumStatuses,
		.setter = &AyuSettings::setHidePremiumStatuses,
	});

	const auto controller = builder.controller();
	builder.addButton({
		.id = u"ayu/monoFont"_q,
		.title = tr::ayu_MonospaceFont(),
		.st = &st::settingsButtonNoIcon,
		.label = rpl::single(
			settings->monoFont().isEmpty()
				? tr::ayu_FontDefault(tr::now)
				: settings->monoFont()),
		.onClick = [=] {
			AyuUi::FontSelectorBox::Show(
				controller,
				[=](const QString &font) {
					AyuSettings::getInstance().setMonoFont(font);
				});
		},
	});

	ayu.addSectionDivider();
}

void BuildChatFolders(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	builder.addSubsectionTitle(tr::ayu_ChatFoldersHeader());

	ayu.addSettingToggle({
		.id = u"ayu/hideNotificationCounters"_q,
		.altIds = { u"ayu/tabCounter"_q },
		.title = tr::ayu_HideNotificationCounters(),
		.getter = &AyuSettings::hideNotificationCounters,
		.setter = &AyuSettings::setHideNotificationCounters,
	});
	ayu.addSettingToggle({
		.id = u"ayu/hideAllChatsFolder"_q,
		.altIds = { u"ayu/hideAllChats"_q },
		.title = tr::ayu_HideAllChats(),
		.getter = &AyuSettings::hideAllChatsFolder,
		.setter = &AyuSettings::setHideAllChatsFolder,
	});

	ayu.addSectionDivider();
}

void BuildTrayElements(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	builder.addSubsectionTitle(tr::ayu_TrayElementsHeader());

	ayu.addSettingToggle({
		.id = u"ayu/showGhostToggleInTray"_q,
		.title = tr::ayu_EnableGhostModeTray(),
		.getter = &AyuSettings::showGhostToggleInTray,
		.setter = &AyuSettings::setShowGhostToggleInTray,
	});

#if defined Q_OS_WIN || defined Q_OS_MAC
	ayu.addSettingToggle({
		.id = u"ayu/showStreamerToggleInTray"_q,
		.title = tr::ayu_EnableStreamerModeTray(),
		.getter = &AyuSettings::showStreamerToggleInTray,
		.setter = &AyuSettings::setShowStreamerToggleInTray,
	});
#endif

	ayu.addSectionDivider();
}

void BuildDrawerElements(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	builder.addSubsectionTitle(tr::ayu_DrawerElementsHeader());

	ayu.addSettingToggle({
		.id = u"ayu/showMyProfileInDrawer"_q,
		.title = tr::lng_menu_my_profile(),
		.getter = &AyuSettings::showMyProfileInDrawer,
		.setter = &AyuSettings::setShowMyProfileInDrawer,
		.icon = { &st::menuIconProfile },
	});

	const auto controller = builder.controller();
	if (controller && HasDrawerBots(controller)) {
		ayu.addSettingToggle({
			.id = u"ayu/showBotsInDrawer"_q,
			.title = tr::lng_filters_type_bots(),
			.getter = &AyuSettings::showBotsInDrawer,
			.setter = &AyuSettings::setShowBotsInDrawer,
			.icon = { &st::menuIconBot },
		});
	}

	ayu.addSettingToggle({
		.id = u"ayu/showNewGroupInDrawer"_q,
		.title = tr::lng_create_group_title(),
		.getter = &AyuSettings::showNewGroupInDrawer,
		.setter = &AyuSettings::setShowNewGroupInDrawer,
		.icon = { &st::menuIconGroups },
	});
	ayu.addSettingToggle({
		.id = u"ayu/showNewChannelInDrawer"_q,
		.title = tr::lng_create_channel_title(),
		.getter = &AyuSettings::showNewChannelInDrawer,
		.setter = &AyuSettings::setShowNewChannelInDrawer,
		.icon = { &st::menuIconChannel },
	});
	ayu.addSettingToggle({
		.id = u"ayu/showContactsInDrawer"_q,
		.title = tr::lng_menu_contacts(),
		.getter = &AyuSettings::showContactsInDrawer,
		.setter = &AyuSettings::setShowContactsInDrawer,
		.icon = { &st::menuIconUserShow },
	});
	ayu.addSettingToggle({
		.id = u"ayu/showCallsInDrawer"_q,
		.title = tr::lng_menu_calls(),
		.getter = &AyuSettings::showCallsInDrawer,
		.setter = &AyuSettings::setShowCallsInDrawer,
		.icon = { &st::menuIconPhone },
	});
	ayu.addSettingToggle({
		.id = u"ayu/showSavedMessagesInDrawer"_q,
		.title = tr::lng_saved_messages(),
		.getter = &AyuSettings::showSavedMessagesInDrawer,
		.setter = &AyuSettings::setShowSavedMessagesInDrawer,
		.icon = { &st::menuIconSavedMessages },
	});
	ayu.addSettingToggle({
		.id = u"ayu/showLReadToggleInDrawer"_q,
		.title = tr::ayu_LReadMessages(),
		.getter = &AyuSettings::showLReadToggleInDrawer,
		.setter = &AyuSettings::setShowLReadToggleInDrawer,
		.icon = { &st::ayuLReadMenuIcon },
	});
	ayu.addSettingToggle({
		.id = u"ayu/showSReadToggleInDrawer"_q,
		.title = tr::ayu_SReadMessages(),
		.getter = &AyuSettings::showSReadToggleInDrawer,
		.setter = &AyuSettings::setShowSReadToggleInDrawer,
		.icon = { &st::ayuSReadMenuIcon },
	});
	ayu.addSettingToggle({
		.id = u"ayu/showNightModeToggleInDrawer"_q,
		.title = tr::lng_menu_night_mode(),
		.getter = &AyuSettings::showNightModeToggleInDrawer,
		.setter = &AyuSettings::setShowNightModeToggleInDrawer,
		.icon = { &st::menuIconNightMode },
	});
	ayu.addSettingToggle({
		.id = u"ayu/showGhostToggleInDrawer"_q,
		.title = tr::ayu_GhostModeToggle(),
		.getter = &AyuSettings::showGhostToggleInDrawer,
		.setter = &AyuSettings::setShowGhostToggleInDrawer,
		.icon = { &st::ayuGhostIcon },
	});

#if defined Q_OS_WIN || defined Q_OS_MAC
	ayu.addSettingToggle({
		.id = u"ayu/showStreamerToggleInDrawer"_q,
		.title = tr::ayu_StreamerModeToggle(),
		.getter = &AyuSettings::showStreamerToggleInDrawer,
		.setter = &AyuSettings::setShowStreamerToggleInDrawer,
		.icon = { &st::ayuStreamerModeMenuIcon },
	});
#endif

	builder.addSkip();
}

const auto kMeta = BuildHelper({
	.id = AyuAppearance::Id(),
	.parentId = AyuMain::Id(),
	.title = &tr::ayu_CategoryAppearance,
	.icon = &st::menuIconPalette,
}, [](SectionBuilder &builder) {
	auto ayu = AyuSectionBuilder(builder);

	builder.addSkip();
	BuildAppIcon(builder, ayu);
	BuildAvatarCorners(builder, ayu);
	BuildAppearance(builder, ayu);
	BuildChatFolders(builder, ayu);
	BuildTrayElements(builder, ayu);
	BuildDrawerElements(builder, ayu);
	builder.addSkip();
});

} // namespace

rpl::producer<QString> AyuAppearance::title() {
	return tr::ayu_CategoryAppearance();
}

AyuAppearance::AyuAppearance(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

void AyuAppearance::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kMeta.build);
	Ui::ResizeFitChild(this, content);
}

Type AyuAppearanceId() {
	return AyuAppearance::Id();
}

} // namespace Settings
