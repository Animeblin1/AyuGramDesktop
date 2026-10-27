#include "ayu/ui/document_preview.h"

#include "ayu/ayu_settings.h"
#include "ayu/features/streamer_mode/streamer_mode.h"
#include "data/data_peer.h"
#include "main/main_session.h"
#include "ui/widgets/rp_window.h"
#include "ui/widgets/labels.h"
#include "webview/webview_embed.h"
#include "webview/webview_data_stream_memory.h"
#include "styles/style_iv.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"

#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QUrl>
#include <QtCore/QUuid>

namespace Ayu {
namespace {

#ifdef Q_OS_WIN
class PdfPreview final : public Ui::RpWindow {
public:
	explicit PdfPreview(QByteArray bytes, const QString &name)
	: _bytes(std::move(bytes))
	, _id("pdf/" + QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString() + ".pdf") {
		setAttribute(Qt::WA_DeleteOnClose);
		setTitle(name);
		setWindowTitle(name);
		setMinimumSize(st::ivEditorWindowMinSize);
		resize(st::ivEditorWindowDefaultSize);
		_browser = std::make_unique<Webview::Window>(body(), Webview::WindowConfig{
			.opaqueBg = st::windowBg->c,
			.safe = true,
		});
		if (!_browser->valid() || !_browser->widget()) {
			return;
		}
		_browser->setDataRequestHandler([=](Webview::DataRequest request) {
			if (request.id != _id) {
				return Webview::DataResult::Failed;
			}
			request.done({
				.stream = std::make_unique<Webview::DataStreamFromMemory>(
					_bytes, "application/pdf"),
			});
			return Webview::DataResult::Done;
		});
		_browser->setNavigationPolicyHandler([=](const QString &url, bool newWindow) {
			// ponytail: pinned lib_webview can't intercept PDF-viewer
			// subresources; navigation policy only gates top-level moves.
			const auto target = QUrl(url);
			return !newWindow
				&& target.scheme() == u"http"_q
				&& target.host() == u"desktop-app-resource"_q
				&& target.path() == QString::fromStdString('/' + _id)
				&& target.query().isEmpty()
				&& target.userInfo().isEmpty()
				&& target.port(-1) == -1;
		});
		_browser->setPermissionHandler([](auto, auto done) { done(false); });
		_browser->setNavigationDoneHandler([=](bool success) {
			if (!success) {
				showError();
			}
		});
		const auto widget = _browser->widget();
		QObject::connect(widget, &QObject::destroyed, this, [=] { showError(); });
		body()->sizeValue() | rpl::on_next([=](QSize size) {
			if (const auto widget = _browser->widget()) {
				widget->setGeometry(QRect(QPoint(), size));
			}
		}, lifetime());
		widget->show();
		_browser->navigateToData(QString::fromStdString(_id));
	}

	~PdfPreview() override {
		if (const auto widget = _browser ? _browser->widget() : nullptr) {
			QObject::disconnect(widget, nullptr, this, nullptr);
		}
	}

	[[nodiscard]] bool ready() const {
		return _browser && _browser->valid() && _browser->widget();
	}

private:
	void showError() {
		if (_error) {
			return;
		}
		_error = Ui::CreateChild<Ui::FlatLabel>(body(),
			rpl::single(u"Не удалось показать PDF. Закройте окно и выберите «Открыть с помощью» в меню файла."_q),
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
	std::string _id;
	std::unique_ptr<Webview::Window> _browser;
	Ui::FlatLabel *_error = nullptr;
};
#endif // Q_OS_WIN

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
		|| bytes.size() > kMaxBytes || !bytes.startsWith("%PDF-")) {
		return false;
	}
	const auto preview = new PdfPreview(bytes, QFileInfo(path).fileName());
	if (!preview->ready()) {
		delete preview;
		return false;
	}
	if (peer) {
		peer->session().lifetime().add(crl::guard(preview, [=] { preview->close(); }));
	}
	if (AyuSettings::getInstance().streamerMode()) {
		AyuFeatures::StreamerMode::hideWidgetWindow(preview);
	}
	preview->show();
	return true;
#else
	return false;
#endif // Q_OS_WIN
}

} // namespace Ayu
