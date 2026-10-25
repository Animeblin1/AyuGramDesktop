// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/features/activity/activity_feed.h"

#include "ayu/ayu_settings.h"
#include "base/flat_map.h"
#include "data/data_peer.h"
#include "data/data_user.h"
#include "lang/lang_keys.h"

namespace Ayu::ActivityFeed {
namespace {

struct Entry {
	QString text;
	bool typing = false;
	crl::time until = 0;
};

base::flat_map<PeerId, Entry> &Entries() {
	static base::flat_map<PeerId, Entry> entries;
	return entries;
}

rpl::event_stream<> &ChangedStream() {
	static rpl::event_stream<> stream;
	return stream;
}

} // namespace

void HandleSendAction(
		not_null<UserData*> from,
		not_null<PeerData*> where,
		const QString &actionText,
		const QString &topicName,
		bool typing,
		TimeId when) {
	const auto &settings = AyuSettings::getInstance();
	if (!settings.liveUserActivity() || actionText.isEmpty()) {
		return;
	}
	auto &entry = Entries()[from->id];
	if (where->isUser()) {
		// A private chat with the viewer - the action speaks for itself.
		entry.text = actionText;
	} else {
		entry.text = QString("%1 — «%2»").arg(actionText, where->name());
		if (!topicName.isEmpty()) {
			entry.text += QString(" (%1)").arg(topicName);
		}
	}
	entry.typing = typing;
	entry.until = crl::now() + 7000;
	ChangedStream().fire({});
}

std::optional<Activity> CurrentActivity(not_null<UserData*> user) {
	const auto &settings = AyuSettings::getInstance();
	if (!settings.liveUserActivity()) {
		return std::nullopt;
	}
	auto &entries = Entries();
	const auto i = entries.find(user->id);
	if (i == entries.end()) {
		return std::nullopt;
	}
	if (crl::now() >= i->second.until) {
		entries.erase(i);
		ChangedStream().fire({});
		return std::nullopt;
	}
	return Activity{ i->second.text, i->second.typing };
}

rpl::producer<> ActivityChanged() {
	return ChangedStream().events();
}

} // namespace Ayu::ActivityFeed
