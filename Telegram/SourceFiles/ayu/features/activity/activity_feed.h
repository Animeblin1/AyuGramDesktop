// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "rpl/rpl.h"

#include <optional>

class PeerData;
class UserData;

namespace Ayu::ActivityFeed {

struct Activity {
	QString text;
	// True when the action is a text-typing one - the UI can attach
	// the classic typing dots animation to it.
	bool typing = false;
};

// Aggregates the public send-action updates (typing, media recording /
// uploading, ...) the client already receives for every chat it is a
// member of, and exposes the freshest one per user for the status line.
void HandleSendAction(
	not_null<UserData*> from,
	not_null<PeerData*> where,
	const QString &actionText,
	const QString &topicName,
	bool typing,
	TimeId when,
	MsgId topic = 0,
	bool cancel = false);

// e.g. { "печатает — «Chat Name» (Topic)" }, nullopt if the user is idle.
[[nodiscard]] std::optional<Activity> CurrentActivity(
	not_null<UserData*> user);

[[nodiscard]] rpl::producer<> ActivityChanged();

} // namespace Ayu::ActivityFeed
