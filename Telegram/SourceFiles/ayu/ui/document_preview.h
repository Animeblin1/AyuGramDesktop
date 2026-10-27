#pragma once

class PeerData;

namespace Ayu {

[[nodiscard]] bool TryShowPdfPreview(
	const QString &path,
	PeerData *peer = nullptr);

} // namespace Ayu
