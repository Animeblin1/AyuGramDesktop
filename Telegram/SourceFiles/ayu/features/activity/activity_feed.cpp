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
	crl::time until = 0;
};

rpl::event_stream<> &ChangedStream() {
	static rpl::event_stream<> stream;
	return stream;
}

struct Feed {
	base::flat_map<std::tuple<PeerId, PeerId, MsgId>, Entry> entries;
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
		AyuSettings::getInstance().liveUserActivityValue(
		) | rpl::on_next([=](bool enabled) {
			if (!enabled) {
				feed->entries.clear();
				feed->timer.cancel();
			}
			ChangedStream().fire({});
		}, session->lifetime());
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
		if (feed.entries.erase(std::tuple{ from->id, where->id, topic })) {
			ChangedStream().fire({});
		}
		return;
	} else if (actionText.isEmpty()) {
		return;
	}
	auto &entry = feed.entries[{ from->id, where->id, topic }];
	entry.activity.text = where->isUser()
		? actionText
		: u"%1 в «%2»"_q.arg(actionText, where->name());
	if (!where->isUser() && !topicName.isEmpty()) {
		entry.activity.text += u" в «%1»"_q.arg(topicName);
	}
	entry.activity.typing = typing;
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
	const auto now = crl::now();
	const Entry *latest = nullptr;
	for (const auto &[key, entry] : entries) {
		if (std::get<0>(key) == user->id
			&& now < entry.until
			&& (!latest || entry.until > latest->until)) {
			latest = &entry;
		}
	}
	return latest ? std::make_optional(latest->activity) : std::nullopt;
}

void ClearActivity(
		not_null<UserData*> from,
		not_null<PeerData*> where,
		MsgId topic) {
	auto &feed = SessionFeed(from);
	const auto key = std::tuple{ from->id, where->id, topic };
	const auto i = feed.entries.find(key);
	if (i != feed.entries.end()) {
		feed.entries.erase(i);
		ChangedStream().fire({});
	}
}

rpl::producer<> ActivityChanged() {
	return ChangedStream().events();
}

} // namespace Ayu::ActivityFeed
