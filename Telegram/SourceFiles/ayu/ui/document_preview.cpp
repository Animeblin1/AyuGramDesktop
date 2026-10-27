#include "ayu/ui/document_preview.h"

#include "ayu/ayu_settings.h"
#include "ayu/features/streamer_mode/streamer_mode.h"
#include "data/data_peer.h"
#include "main/main_session.h"
#include "ui/widgets/rp_window.h"
#include "ui/widgets/labels.h"
#include "webview/webview_embed.h"
#include "webview/webview_data_stream_memory.h"
#include "styles/style_box.h"
#include "styles/style_iv.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"

#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QUrl>
#include <QtCore/QUuid>

namespace Ayu {
namespace {

constexpr auto kMaxBytes = qint64(32) * 1024 * 1024;

// WebView window for untrusted local documents: the page body is served
// from an in-memory data stream, and the navigation policy pins the
// window to that single document (or, for remote links, to that host).
class WebFilePreview final : public Ui::RpWindow {
public:
	WebFilePreview(
		QByteArray bytes,
		std::string mime,
		const QString &name,
		const QString &prefix)
	: _bytes(std::move(bytes))
	, _mime(std::move(mime))
	, _isLocal(true) {
		setup(name, prefix);
		navigate();
	}

	WebFilePreview(const QString &url, const QString &title)
	: _remoteUrl(url)
	, _isLocal(false) {
		setAttribute(Qt::WA_DeleteOnClose);
		const auto host = QUrl(url).host();
		setTitle(host.isEmpty() ? title : host);
		setWindowTitle(host.isEmpty() ? title : host);
		setMinimumSize(st::ivEditorWindowMinSize);
		resize(st::ivEditorWindowDefaultSize);
		_browser = std::make_unique<Webview::Window>(body(), Webview::WindowConfig{
			.opaqueBg = st::windowBg->c,
			.safe = true,
		});
		setupBrowser();
	}

	~WebFilePreview() override = default;

	[[nodiscard]] bool ready() const {
		return _browser && _browser->valid() && _browser->widget();
	}

private:
	void setup(const QString &name, const QString &prefix) {
		setAttribute(Qt::WA_DeleteOnClose);
		setTitle(name);
		setWindowTitle(name);
		setMinimumSize(st::ivEditorWindowMinSize);
		resize(st::ivEditorWindowDefaultSize);
		_browser = std::make_unique<Webview::Window>(body(), Webview::WindowConfig{
			.opaqueBg = st::windowBg->c,
			.safe = true,
		});
		setupBrowser();
		_id = (prefix + QUuid::createUuid().toString(QUuid::WithoutBraces))
			.toStdString();
	}
	void setupBrowser() {
		if (!_browser->valid() || !_browser->widget()) {
			return;
		}
		_browser->setPermissionHandler([](auto, auto done) { done(false); });
		_browser->setNavigationPolicyHandler(
			[=](const QString &url, bool newWindow) {
				return navigationAllowed(url, newWindow);
			});
		_browser->setNavigationDoneHandler([=](bool success) {
			if (!success && !_isLocal) {
				showError();
			}
		});
		_browser->setDataRequestHandler([=](Webview::DataRequest request) {
			if (!_isLocal) {
				return Webview::DataResult::Failed;
			}
			// The synthetic id may arrive with a percent-encoded path
			// prefix added by the embed layer; strip anything before it.
			const auto &id = _id;
			const auto &raw = request.id;
			const auto pos = raw.find(id);
			if (pos == std::string::npos) {
				return Webview::DataResult::Failed;
			}
			request.done({
				.stream = std::make_unique<Webview::DataStreamFromMemory>(
					_bytes, _mime),
			});
			return Webview::DataResult::Done;
		});
		const auto widget = _browser->widget();
		QObject::connect(widget, &QObject::destroyed, this, [=] {
			if (_isLocal) {
				showError();
			}
		});
		body()->sizeValue() | rpl::on_next([=](QSize size) {
			if (const auto widget = _browser->widget()) {
				widget->setGeometry(QRect(QPoint(), size));
			}
		}, lifetime());
		widget->show();
	}

	void navigate() {
		if (!_browser || !_browser->valid()) {
			return;
		}
		if (_isLocal) {
			_browser->navigateToData(QString::fromStdString(_id));
		} else {
			_browser->navigate(_remoteUrl);
		}
	}

	[[nodiscard]] bool navigationAllowed(
			const QString &url,
			bool newWindow) const {
		if (newWindow) {
			return false;
		}
		const auto target = QUrl(url);
		if (_isLocal) {
			return target.scheme() == u"http"_q
				&& target.host() == u"desktop-app-resource"_q
				&& target.path().endsWith(
					QString::fromStdString('/' + _id));
		}
		const auto initial = QUrl(_remoteUrl);
		// Pin remote pages to the original host: any other host is denied.
		return target.scheme() == initial.scheme()
			&& target.host() == initial.host()
			&& target.port(initial.port(-2)) == initial.port(-2);
	}

	void showError() {
		if (_error) {
			return;
		}
		_error = Ui::CreateChild<Ui::FlatLabel>(body(),
			rpl::single(u"Не удалось показать документ. Закройте окно и выберите «Открыть с помощью» в меню файла."_q),
			st::boxLabel);
		body()->widthValue() | rpl::on_next([=](int width) {
			const auto padding = st::boxPadding;
			_error->resizeToWidth(std::max(0, width - padding.left() - padding.right()));
			_error->move(padding.left(), padding.top());
		}, _error->lifetime());
		_error->show();
		_error->raise();
	}

	QByteArray _bytes;
	std::string _mime;
	std::string _id;
	QString _remoteUrl;
	bool _isLocal = true;
	std::unique_ptr<Webview::Window> _browser;
	Ui::FlatLabel *_error = nullptr;
};

[[nodiscard]] std::unique_ptr<WebFilePreview> CreateLocalPreview(
		const QByteArray &bytes,
		const std::string &mime,
		const QString &name,
		const QString &prefix) {
	auto preview = std::make_unique<WebFilePreview>(
		bytes, mime, name, prefix);
	if (!preview->ready()) {
		preview.reset();
	}
	return preview;
}

} // namespace

bool TryShowPdfPreview(const QString &path, PeerData *peer) {
#ifdef Q_OS_WIN
	constexpr auto kMaxBytes = qint64(32) * 1024 * 1024;
	if (QFileInfo(path).suffix().compare(u"pdf"_q, Qt::CaseInsensitive) != 0) {
		return false;
	}
	auto file = QFile(path);
	if (!file.open(QIODevice::ReadOnly) || file.size() > kMaxBytes) {
		return false;
	}
	const auto bytes = file.read(kMaxBytes + 1);
	if (file.error() != QFileDevice::NoError || !file.atEnd()
		|| bytes.size() > kMaxBytes
		|| !bytes.startsWith(QByteArrayLiteral("%PDF-"))) {
		return false;
	}
	auto preview = CreateLocalPreview(
		bytes, "application/pdf", QFileInfo(path).fileName(), u"pdf/"_q);
	if (!preview) {
		return false;
	}
	const auto raw = preview.get();
	if (peer) {
		peer->session().lifetime().add(crl::guard(raw, [=] { raw->close(); }));
	}
	if (AyuSettings::getInstance().streamerMode()) {
		AyuFeatures::StreamerMode::hideWidgetWindow(raw);
	}
	raw->show();
	(void)preview.release();
	return true;
#else
	return false;
#endif // Q_OS_WIN
}

bool TryShowWebPreview(const QString &path, const QString &title) {
#ifdef Q_OS_WIN
	constexpr auto kMaxBytes = 8 * 1024 * 1024;
	const auto suffix = QFileInfo(path).suffix().toLower();
	if (suffix != u"html"_q && suffix != u"htm"_q) {
		return false;
	}
	auto file = QFile(path);
	if (!file.open(QIODevice::ReadOnly) || file.size() > kMaxBytes) {
		return false;
	}
	const auto bytes = file.readAll();
	if (file.error() != QFileDevice::NoError
		|| bytes.left(8 * 1024).indexOf(char(0)) >= 0) {
		return false;
	}
	auto preview = CreateLocalPreview(
		bytes, "text/html; charset=utf-8", title, u"html/"_q);
	if (!preview) {
		return false;
	}
	const auto raw = preview.get();
	if (AyuSettings::getInstance().streamerMode()) {
		AyuFeatures::StreamerMode::hideWidgetWindow(raw);
	}
	raw->show();
	(void)preview.release();
	return true;
#else
	return false;
#endif // Q_OS_WIN
}

bool TryShowHtmlPreview(const QString &path, PeerData *peer) {
#ifdef Q_OS_WIN
	return TryShowWebPreview(path, QFileInfo(path).fileName());
#else
	return false;
#endif // Q_OS_WIN
}

bool TryOpenLinkInWebview(const QString &url) {
#ifdef Q_OS_WIN
	const auto target = QUrl(url);
	if (!target.isValid()
		|| (target.scheme() != u"http"_q
			&& target.scheme() != u"https"_q)) {
		return false;
	}
	auto preview = std::make_unique<WebFilePreview>(
		target.toString(), target.host());
	if (!preview->ready()) {
		preview.reset();
		return false;
	}
	if (AyuSettings::getInstance().streamerMode()) {
		AyuFeatures::StreamerMode::hideWidgetWindow(preview.get());
	}
	preview->show();
	(void)preview.release();
	return true;
#else
	return false;
#endif // Q_OS_WIN
}

} // namespace Ayu
