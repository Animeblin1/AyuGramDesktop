// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/boxes/plugin_info_box.h"

#include "apiwrap.h"
#include "ayu/ayu_url_handlers.h"
#include "base/algorithm.h"
#include "base/zlib_help.h"
#include "boxes/abstract_box.h"
#include "core/application.h"
#include "core/file_utilities.h"
#include "core/ui_integration.h"
#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_file_origin.h"
#include "data/data_session.h"
#include "data/stickers/data_stickers_set.h"
#include "iv/iv_instance.h"
#include "lang/lang_keys.h"
#include "chat_helpers/stickers_lottie.h"
#include "history/view/media/history_view_sticker_player.h"
#include "main/main_session.h"
#include "ui/boxes/confirm_box.h"
#include "ui/toast/toast.h"
#include "ui/widgets/popup_menu.h"
#include "ui/basic_click_handlers.h"
#include "styles/style_ayu_styles.h"
#include "styles/style_boxes.h"
#include "styles/style_chat.h"
#include "styles/style_giveaway.h"
#include "styles/style_layers.h"
#include "styles/style_premium.h"
#include "ui/painter.h"
#include "ui/rect.h"
#include "ui/vertical_list.h"
#include "ui/layers/generic_box.h"
#include "ui/text/text_utilities.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/scroll_area.h"
#include "window/window_controller.h"
#include "window/window_session_controller.h"

#include "styles/style_menu_icons.h"

#include <QDir>
#include <QFile>
#include <QPainterPath>
#include <QRegularExpression>
#include <QStandardPaths>

#include <algorithm>
#include <array>

namespace Ui {
namespace {

// Opens any text in the IV viewer by writing a temp .md file with the
// content wrapped in a fenced code block.
void PreviewTextInIv(
		not_null<Window::SessionController*> controller,
		const QString &name,
		const QString &text) {
	const auto fence = text.contains(u"```"_q) ? u"~~~~"_q : u"```"_q;
	const auto ext = name.lastIndexOf(QChar('.'));
	const auto lang = (ext >= 0 && ext + 1 < name.size())
		? name.mid(ext + 1).toLower()
		: QString();
	const auto body = fence + lang + QChar('\n') + text + QChar('\n') + fence;

	const auto dir = QDir(
		QStandardPaths::writableLocation(QStandardPaths::TempLocation)
		+ u"/ayu_preview"_q);
	if (!dir.exists()) {
		QDir().mkpath(dir.absolutePath());
	}
	static auto counter = 0;
	const auto path = dir.filePath(
		u"preview_%1_%2.md"_q.arg(++counter).arg(name.section('.', 0, 0)));
	auto file = QFile(path);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)
		|| file.write(body.toUtf8()) != body.toUtf8().size()) {
		Ui::Toast::Show(tr::ayu_PluginCodeFail(tr::now));
		return;
	}
	Core::App().iv().showMarkdown(path, QVariant());
}

[[nodiscard]] QString ReadFileBounded(
		const QString &path,
		qint64 limit) {
	auto file = QFile(path);
	return file.open(QIODevice::ReadOnly)
		? QString::fromUtf8(file.read(limit))
		: QString();
}

[[nodiscard]] QByteArray ReadArchiveBounded(const QString &path, qint64 limit) {
	auto file = QFile(path);
	return file.open(QIODevice::ReadOnly) ? file.read(limit) : QByteArray();
}

[[nodiscard]] bool IsTextLikeArchiveEntry(const QString &name) {
	const auto ext = name.mid(name.lastIndexOf(QChar('.')) + 1).toLower();
	const auto list = std::array{
		u"yml"_q, u"yaml"_q, u"json"_q, u"xml"_q, u"py"_q, u"js"_q,
		u"html"_q, u"htm"_q, u"css"_q, u"txt"_q, u"md"_q, u"svg"_q,
		u"lat"_q, u"toml"_q, u"ini"_q, u"cfg"_q,
	};
	return std::find(list.begin(), list.end(), ext) != list.end();
}

void PreviewArchiveEntry(
		not_null<Window::SessionController*> controller,
		const QString &pluginPath,
		const QString &entryName) {
	const auto reader = std::make_shared<zlib::FileToRead>(
		ReadArchiveBounded(pluginPath, 1024 * 1024));
	if (reader->locateFile(
			entryName.toUtf8().constData(),
			zlib::kCaseInsensitive) != UNZ_OK) {
		return;
	}
	const auto bytes = reader->readCurrentFileContent(1024 * 1024);
	if (bytes.isEmpty()) {
		return;
	}
	if (IsTextLikeArchiveEntry(entryName)) {
		PreviewTextInIv(
			controller,
			entryName,
			QString::fromUtf8(bytes));
	} else {
		Ui::Toast::Show(tr::ayu_PluginArchiveBinary(tr::now));
	}
}

void ShowArchiveContents(
		not_null<Window::SessionController*> controller,
		const QString &pluginPath,
		const QByteArray &archive) {
	const auto entries = std::make_shared<std::vector<QString>>();
	const auto reader = std::make_shared<zlib::FileToRead>(archive);
	if (reader->goToFirstFile() == UNZ_OK) {
		do {
			const auto fileName = reader->getCurrentFileName();
			if (!fileName.isEmpty() && !fileName.endsWith(QChar('/'))) {
				entries->push_back(fileName);
			}
		} while (reader->goToNextFile() == UNZ_OK);
	}
	if (entries->empty()) {
		Ui::Toast::Show(tr::ayu_PluginArchiveEmpty(tr::now));
		return;
	}
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(tr::ayu_PluginArchiveContents());
		for (const auto &fileName : *entries) {
			const auto entryName = fileName;
			const auto row = box->addRow(object_ptr<Ui::FlatLabel>(
				box,
				rpl::single(entryName
					+ (IsTextLikeArchiveEntry(entryName)
						? QString()
						: u" (бинарный файл)"_q)),
				st::boxLabel));
			if (IsTextLikeArchiveEntry(entryName)) {
				row->setLink(1, std::make_shared<LambdaClickHandler>([=] {
					PreviewArchiveEntry(controller, pluginPath, entryName);
				}));
			}
		}
	}));
}

void ShowPluginMenu(
		not_null<Window::SessionController*> controller,
		const QString &pluginPath,
		const QString &name) {
	const auto suffix = QFileInfo(pluginPath).suffix().toLower();
	const auto menu = Ui::CreateChild<Ui::PopupMenu>(
		controller->window().widget(),
		st::popupMenuWithIcons);
	menu->addAction(
		tr::ayu_PluginViewCode(tr::now),
		crl::guard(controller, [=] {
			PreviewTextInIv(
				controller,
				name + u".plugin"_q,
				ReadFileBounded(pluginPath, 1024 * 1024));
		}),
		&st::menuIconBotCommands);
	if (suffix == u"elyx"_q || suffix == u"eaf"_q) {
		menu->addAction(
			tr::ayu_PluginArchiveContents(tr::now),
			crl::guard(controller, [=] {
				ShowArchiveContents(
					controller,
					pluginPath,
					ReadArchiveBounded(pluginPath, 1024 * 1024));
			}),
			&st::menuIconArchive);
	}
	menu->popup(QCursor::pos());
}

QString ExtractField(const QByteArray &data, const QString &key) {
	// supports probably all kinds of weird python syntax + both " and '
	// "description"
	// """description"""
	// (
	// 	"description\n\n"
	// 	"description\n"
	// )
	static const auto quotedValue = QRegularExpression(
		uR"field((?:"""((?:\\[\s\S]|(?!""")[^\\])*)"""|'''((?:\\[\s\S]|(?!''')[^\\])*)'''|(?!""")"((?:\\.|[^\r\n"\\])*)"|(?!''')'((?:\\.|[^\r\n'\\])*)'))field"_q);
	const auto re = QRegularExpression(
		uR"(__%1__\s*=\s*(?:%2|\((?:\s*%2)+\s*\)))"_q.arg(
			key,
			quotedValue.pattern()));
	const auto match = re.match(QString::fromUtf8(data));
	if (!match.hasMatch()) {
		return {};
	}
	auto result = QString();
	auto parts = quotedValue.globalMatch(match.captured());
	while (parts.hasNext()) {
		const auto partMatch = parts.next();
		auto part = partMatch.captured(1);
		for (auto i = 2
			; part.isNull() && i <= quotedValue.captureCount()
			; ++i) {
			part = partMatch.captured(i);
		}
		part.replace(u"\\n"_q, u"\n"_q);
		part.replace(u"\\t"_q, u"\t"_q);
		part.replace(u"\\\""_q, u"\""_q);
		part.replace(u"\\'"_q, u"'"_q);
		part.replace(u"\\\\"_q, u"\\"_q);
		result += part;
	}
	return result;
}

TextWithEntities ParsePluginDescriptionPlain(const QString &text) {
	auto result = TextWithEntities{ text };
	TextUtilities::ParseEntities(
		result,
		TextParseLinks | TextParseMentions);
	return result;
}

TextWithEntities ParsePluginDescriptionLinks(const QString &text) {
	static const auto linkExpression = QRegularExpression(
		uR"(\[([^\]\r\n]+)\]\(([^)\r\n]+)\))"_q);
	auto result = TextWithEntities();
	auto offset = 0;
	auto matches = linkExpression.globalMatch(text);
	while (matches.hasNext()) {
		const auto match = matches.next();
		const auto url = match.capturedView(2);
		if (!Ui::InputField::IsValidMarkdownLink(url)) {
			continue;
		}
		const auto parsed = QUrl(url.toString());
		if (!AyuUrlHandlers::IsWebUrl(parsed)
			&& parsed.scheme().compare(u"tg"_q, Qt::CaseInsensitive) != 0) {
			continue;
		}
		result.append(ParsePluginDescriptionPlain(
			text.mid(offset, match.capturedStart() - offset)));
		result.append(Ui::Text::Link(
			match.captured(1),
			url.toString()));
		offset = match.capturedEnd();
	}
	result.append(ParsePluginDescriptionPlain(text.mid(offset)));
	return result;
}

TextWithEntities ParsePluginDescription(const QString &text) {
	static const auto boldExpression = QRegularExpression(
		uR"(\*\*(.+?)\*\*)"_q,
		QRegularExpression::DotMatchesEverythingOption);
	auto result = TextWithEntities();
	auto offset = 0;
	auto matches = boldExpression.globalMatch(text);
	while (matches.hasNext()) {
		const auto match = matches.next();
		result.append(ParsePluginDescriptionLinks(
			text.mid(offset, match.capturedStart() - offset)));
		result.append(Ui::Text::Wrapped(
			ParsePluginDescriptionLinks(match.captured(1)),
			EntityType::Bold));
		offset = match.capturedEnd();
	}
	result.append(ParsePluginDescriptionLinks(text.mid(offset)));
	return result;
}

void FillPluginInfoBox(
	not_null<Ui::GenericBox*> box,
	not_null<Window::SessionController*> controller,
	const QString &pluginPath,
	const PluginMetadata& metadata,
	DocumentData *stickerDoc) {
	box->setStyle(st::giveawayGiftCodeBox);
	box->setNoContentMargin(true);
	box->setWidth(st::aboutWidth);
	box->verticalLayout()->resizeToWidth(box->width());

	box->addTopButton(st::pluginShowInFolder,
					  [=]
					  {
						  File::ShowInFolder(pluginPath);
					  });
	box->addTopButton(st::pluginMenuToggle,
					  [=]
					  {
						  ShowPluginMenu(controller, pluginPath, metadata.name);
					  });

	Ui::AddSkip(box->verticalLayout());
	Ui::AddSkip(box->verticalLayout());
	Ui::AddSkip(box->verticalLayout());

	if (stickerDoc) {
		const auto stickerSize = st::maxStickerSize / 3;
		const auto radius = stickerSize / 5;
		const auto widget = box->verticalLayout()->add(
			object_ptr<Ui::RpWidget>(box->verticalLayout()));
		widget->resize(Size(stickerSize));

		const auto media = stickerDoc->createMediaView();
		media->checkStickerLarge();
		const auto sticker = stickerDoc->sticker();

		const auto player = widget->lifetime().make_state<
			std::unique_ptr<HistoryView::StickerPlayer>>();

		const auto createPlayer = [=] {
			if (sticker->isLottie()) {
				*player = std::make_unique<HistoryView::LottiePlayer>(
					ChatHelpers::LottiePlayerFromDocument(
						media.get(),
						ChatHelpers::StickerLottieSize::StickerSet,
						Size(stickerSize),
						Lottie::Quality::High));
			} else if (sticker->isWebm()) {
				*player = std::make_unique<HistoryView::WebmPlayer>(
					media->owner()->location(),
					media->bytes(),
					Size(stickerSize));
			} else {
				*player = std::make_unique<HistoryView::StaticStickerPlayer>(
					media->owner()->location(),
					media->bytes(),
					Size(stickerSize));
			}
			(*player)->setRepaintCallback([=] { widget->update(); });
		};

		if (media->loaded()) {
			createPlayer();
		} else {
			rpl::single() | rpl::then(
				stickerDoc->owner().session().downloaderTaskFinished()
			) | rpl::filter([=] {
				return media->loaded();
			}) | rpl::take(1) | rpl::on_next([=] {
				createPlayer();
			}, widget->lifetime());
		}

		widget->paintRequest(
		) | rpl::on_next([=] {
			if (!*player || !(*player)->ready()) {
				return;
			}
			const auto frame = (*player)->frame(
				Size(stickerSize),
				QColor(0, 0, 0, 0),
				false,
				crl::now(),
				false);
			if (frame.image.isNull()) {
				return;
			}
			auto p = QPainter(widget);
			PainterHighQualityEnabler hq(p);
			const auto x = (widget->width() - stickerSize) / 2;
			QPainterPath path;
			path.addRoundedRect(
				QRectF(x, 0, stickerSize, stickerSize),
				radius,
				radius);
			p.setClipPath(path);
			p.drawImage(
				QRect(x, 0, stickerSize, stickerSize),
				frame.image);
			(*player)->markFrameShown();
		}, widget->lifetime());

		Ui::AddSkip(box->verticalLayout());
		Ui::AddSkip(box->verticalLayout());
	}

	box->verticalLayout()->add(
		object_ptr<Ui::FlatLabel>(
			box->verticalLayout(),
			rpl::single(metadata.name),
			st::boxTitle),
		st::boxRowPadding,
		style::al_top);

	{
		const auto versionLabel = box->verticalLayout()->add(
			object_ptr<Ui::FlatLabel>(
				box->verticalLayout(),
				st::pluginVersionAuthorLabel),
			st::boxRowPadding,
			style::al_justify);

		const auto versionPrefix = tr::ayu_PluginVersion(tr::now)
			+ u" "_q
			+ metadata.version;

		if (metadata.author.isEmpty()) {
			versionLabel->setText(
				versionPrefix
				+ u" \u2022 "_q
				+ tr::ayu_PluginNoAuthor(tr::now));
		} else {
			auto text = TextWithEntities{
				versionPrefix + u" \u2022 "_q,
			};
			static const auto usernameRe = QRegularExpression(
				u"@([A-Za-z0-9_]+)"_q);
			const auto &author = metadata.author;
			auto it = usernameRe.globalMatch(author);
			auto lastEnd = 0;
			while (it.hasNext()) {
				const auto match = it.next();
				if (match.capturedStart() > lastEnd) {
					text.append(TextWithEntities{
						author.mid(lastEnd,
								   match.capturedStart() - lastEnd)
					});
				}
				text.append(Ui::Text::Link(
					match.captured(0),
					controller->session().createInternalLinkFull(
						match.captured(1))));
				lastEnd = match.capturedEnd();
			}
			if (lastEnd < author.size()) {
				text.append(TextWithEntities{
					author.mid(lastEnd)
				});
			}
			if (lastEnd == 0) {
				text.append(TextWithEntities{author});
			}
			versionLabel->setMarkedText(
				text,
				Core::TextContext({
					.session = &controller->session(),
				}));
		}
	}

	Ui::AddSkip(box->verticalLayout());
	Ui::AddSkip(box->verticalLayout());

	{
		const auto hasDescription = !metadata.description.isEmpty();
		const auto descText = hasDescription
			? ParsePluginDescription(metadata.description)
			: TextWithEntities{tr::ayu_PluginNoDescription(tr::now)};

		const auto availableWidth = box->width()
			- st::boxRowPadding.left()
			- st::boxRowPadding.right();

		const auto container = box->verticalLayout()->add(
			object_ptr<Ui::RpWidget>(box->verticalLayout()),
			st::boxRowPadding);

		const auto scroll = Ui::CreateChild<Ui::ScrollArea>(
			container,
			st::boxScroll);
		const auto descLabel = scroll->setOwnedWidget(
			object_ptr<Ui::FlatLabel>(scroll, st::boxLabel));
		descLabel->setMarkedText(
			descText,
			Core::TextContext({
				.session = &controller->session(),
			}));
		descLabel->resizeToWidth(availableWidth);

		const auto needsScroll =
			descLabel->height() > st::maxPluginDescriptionHeight;
		const auto scrollPad = needsScroll
								   ? (st::boxScroll.width + st::lineWidth)
								   : 0;
		if (needsScroll) {
			descLabel->resizeToWidth(availableWidth - scrollPad);
		}

		const auto containerHeight = std::min(
			descLabel->height(),
			st::maxPluginDescriptionHeight);
		container->resize(availableWidth, containerHeight);

		container->sizeValue(
		) | rpl::on_next([=](QSize size)
						 {
							 scroll->setGeometry(0, 0, size.width(), size.height());
							 descLabel->resizeToWidth(size.width() - scrollPad);
						 },
						 container->lifetime());

		Ui::AddSkip(box->verticalLayout());
	}

	if (!metadata.requirements.isEmpty()) {
		const auto chipsContainer = box->verticalLayout()->add(
			object_ptr<Ui::RpWidget>(box->verticalLayout()),
			st::boxRowPadding);

		auto chips = std::vector<Ui::RoundButton*>();
		for (const auto &dep : metadata.requirements) {
			const auto chip = Ui::CreateChild<Ui::RoundButton>(
				chipsContainer,
				rpl::single(dep),
				st::defaultTableSmallButton);
			chip->setTextTransform(
				Ui::RoundButtonTextTransform::NoTransform);
			chip->setClickedCallback([dep] {
				File::OpenUrl(
					u"https://pypi.org/project/"_q + dep + u"/"_q);
			});
			chips.push_back(chip);
		}

		const auto chipsPtr = chipsContainer->lifetime().make_state<
			std::vector<Ui::RoundButton*>>(std::move(chips));

		chipsContainer->sizeValue(
		) | rpl::on_next([=](QSize size)
						 {
							 const auto skip = st::pluginChipSkip;
							 auto x = 0;
							 auto y = 0;
							 auto widthLeft = size.width();
							 auto firstRow = true;
							 for (const auto chip : *chipsPtr) {
								 const auto chipWidth = chip->width();
								 const auto chipHeight = chip->height();
								 if (!firstRow && chipWidth > widthLeft) {
									 x = 0;
									 y += chipHeight + skip;
									 widthLeft = size.width();
								 }
								 firstRow = false;
								 chip->moveToLeft(x, y);
								 x += chipWidth + skip;
								 widthLeft = size.width() - x;
							 }
							 if (!chipsPtr->empty()) {
								 const auto lastChip = chipsPtr->back();
								 chipsContainer->resize(
									 size.width(),
									 lastChip->y() + lastChip->height());
							 }
						 },
						 chipsContainer->lifetime());

		Ui::AddSkip(box->verticalLayout());
	}

	Ui::AddSkip(box->verticalLayout());

	box->verticalLayout()->add(
		object_ptr<Ui::FlatLabel>(
			box->verticalLayout(),
			tr::ayu_PluginsNotAvailable(),
			st::boxDividerLabel),
		st::boxRowPadding);

	Ui::AddSkip(box->verticalLayout());

	const auto closeButton = box->addButton(
		tr::lng_close(),
		[=] { box->closeBox(); });
	const auto buttonWidth = box->width()
		- rect::m::sum::h(st::starrefFooterBox.buttonPadding);
	closeButton->widthValue() | rpl::filter([=]
	{
		return (closeButton->widthNoMargins() != buttonWidth);
	}) | rpl::on_next([=]
					  {
						  closeButton->resizeToWidth(buttonWidth);
					  },
					  closeButton->lifetime());
}

}

PluginMetadata ParsePluginMetadata(const QByteArray &data) {
	auto metadata = PluginMetadata();

	metadata.id = ExtractField(data, u"id"_q);
	metadata.name = ExtractField(data, u"name"_q).left(100);
	metadata.description = ExtractField(data, u"description"_q).left(8000);
	metadata.author = ExtractField(data, u"author"_q).left(200);
	metadata.version = ExtractField(data, u"version"_q).left(32);
	metadata.icon = ExtractField(data, u"icon"_q);
	metadata.minVersion = ExtractField(data, u"min_version"_q);

	const auto reqString = ExtractField(data, u"requirements"_q);
	if (!reqString.isEmpty()) {
		for (const auto &part : reqString.split(',')) {
			const auto trimmed = part.trimmed();
			if (!trimmed.isEmpty()) {
				metadata.requirements.append(trimmed);
			}
		}
	}

	if (metadata.version.isEmpty()) {
		metadata.version = u"1.0"_q;
	}

	static const auto idRegex = QRegularExpression(
		u"^[a-zA-Z][a-zA-Z0-9_-]{1,31}$"_q);
	if (metadata.id.isEmpty()
		|| !idRegex.match(metadata.id).hasMatch()) {
		return {};
	}

	if (metadata.name.isEmpty()) {
		return {};
	}

	return metadata;
}

void ShowPluginInfoBox(
	not_null<Window::SessionController*> controller,
	const QString &pluginPath,
	PluginMetadata metadata) {
	const auto showBox = crl::guard(controller, [controller, pluginPath](
		PluginMetadata md,
		DocumentData *stickerDoc)
	{
		controller->show(Box(
			FillPluginInfoBox,
			controller,
			pluginPath,
			std::move(md),
			stickerDoc));
	});

	if (metadata.icon.isEmpty()) {
		showBox(std::move(metadata), nullptr);
		return;
	}

	const auto parts = metadata.icon.split('/');
	if (parts.size() != 2) {
		showBox(std::move(metadata), nullptr);
		return;
	}

	const auto& shortName = parts[0];
	const auto index = parts[1].toInt();
	const auto session = &controller->session();
	const auto shared = std::make_shared<PluginMetadata>(
		std::move(metadata));

	session->api().request(MTPmessages_GetStickerSet(
		Data::InputStickerSet({
			.shortName = shortName,
		}),
		MTP_int(0)
	)).done(crl::guard(controller, [=](const MTPmessages_StickerSet &result)
	{
		auto doc = (DocumentData*) nullptr;
		result.match([&](const MTPDmessages_stickerSet &data)
					 {
						 const auto &v = data.vdocuments().v;
						 if (index >= 0 && index < v.size()) {
							 doc = session->data().processDocument(v[index]);
						 }
					 },
					 [](const MTPDmessages_stickerSetNotModified &)
					 {
					 });

		if (!doc) {
			showBox(std::move(*shared), nullptr);
			return;
		}

		const auto media = doc->createMediaView();
		media->checkStickerLarge();

		if (media->loaded()) {
			showBox(std::move(*shared), doc);
			return;
		}

		rpl::single() | rpl::then(
			session->downloaderTaskFinished()
		) | rpl::filter([=] {
			return media->loaded();
		}) | rpl::take(1) | rpl::on_next([=] {
			showBox(std::move(*shared), doc);
		}, controller->lifetime());
		media->automaticLoad(doc->stickerSetOrigin(), nullptr);
	})).fail(crl::guard(controller, [=](const MTP::Error &)
	{
		showBox(std::move(*shared), nullptr);
	})).send();
}

} // namespace Ui
