#pragma once

class PeerData;

namespace Ayu {

[[nodiscard]] bool TryShowPdfPreview(
	const QString &path,
	PeerData *peer = nullptr);

// Renders a local .html/.htm file in a sandboxed WebView window.
[[nodiscard]] bool TryShowHtmlPreview(
	const QString &path,
	PeerData *peer = nullptr);

// Opens any http(s) link in a sandboxed WebView window of this app.
[[nodiscard]] bool TryOpenLinkInWebview(const QString &url);

// For local html files: opens them in a sandboxed WebView window.
[[nodiscard]] bool TryShowWebPreview(const QString &path, const QString &title);

} // namespace Ayu
