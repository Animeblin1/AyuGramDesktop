// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/features/activity/activity_feed.h"

#include "ayu/ayu_settings.h"
#include "base/flat_map.h"
#include "base/timer.h"
#include "data/data_peer.h"
#include "data/data_user.h"
#include "main/main_session.h"

namespace Ayu::ActivityFeed {
namespace {

struct Entry {
	Activity activity;
	PeerId where;
	MsgId topic = 0;
	crl::time until = 0;
};

rpl::event_stream<> &ChangedStream() {
	static rpl::event_stream<> stream;
	return stream;
}

struct Feed {
	base::flat_map<PeerId, Entry> entries;
	base::Timer timer{ [=] {
		const auto now = crl::now();
		auto changed = false;
		for (auto i = entries.begin(); i != entries.end();) {
			if (i->second.until <= now) {
				i = entries.erase(i);
				changed = true;
			} else {
				++i;
			}
		}
		if (entries.empty()) {
			timer.cancel();
		}
		if (changed) {
			ChangedStream().fire({});
		}
	} };
};

Feed &SessionFeed(not_null<UserData*> user) {
	static base::flat_map<Main::Session*, Feed*> feeds;
	const auto session = &user->session();
	auto &feed = feeds[session];
	if (!feed) {
		feed = session->lifetime().make_state<Feed>();
		session->lifetime().add([=] { feeds.erase(session); });
	}
	return *feed;
}

} // namespace

void HandleSendAction(
		not_null<UserData*> from,
		not_null<PeerData*> where,
		const QString &actionText,
		const QString &topicName,
		bool typing,
		TimeId when,
		MsgId topic,
		bool cancel) {
	if (!AyuSettings::getInstance().liveUserActivity() || !when) {
		return;
	}
	auto &feed = SessionFeed(from);
	if (cancel) {
		const auto i = feed.entries.find(from->id);
		if (i != feed.entries.end()
			&& i->second.where == where->id
			&& i->second.topic == topic) {
			feed.entries.erase(i);
			ChangedStream().fire({});
		}
		return;
	} else if (actionText.isEmpty()) {
		return;
	}
	auto &entry = feed.entries[from->id];
	entry.activity.text = where->isUser()
		? actionText
		: u"%1 в «%2»"_q.arg(actionText, where->name());
	if (!where->isUser() && !topicName.isEmpty()) {
		entry.activity.text += u" в «%1»"_q.arg(topicName);
	}
	entry.activity.typing = typing;
	entry.where = where->id;
	entry.topic = topic;
	entry.until = crl::now() + 7000;
	if (!feed.timer.isActive()) {
		feed.timer.callEach(1000);
	}
	ChangedStream().fire({});
}

std::optional<Activity> CurrentActivity(not_null<UserData*> user) {
	if (!AyuSettings::getInstance().liveUserActivity()) {
		return std::nullopt;
	}
	const auto &entries = SessionFeed(user).entries;
	const auto i = entries.find(user->id);
	return (i != entries.end() && crl::now() < i->second.until)
		? std::make_optional(i->second.activity)
		: std::nullopt;
}

rpl::producer<> ActivityChanged() {
	return ChangedStream().events();
}

} // namespace Ayu::ActivityFeed
