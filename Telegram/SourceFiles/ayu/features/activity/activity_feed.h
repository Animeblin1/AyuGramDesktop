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

// Aggregates the public send-action updates (typing, media recording /
// uploading, ...) the client already receives for every chat it is a
// member of, and exposes the freshest one per user for the status line.
void HandleSendAction(
	not_null<UserData*> from,
	not_null<PeerData*> where,
	const QString &actionText,
	TimeId when);

// e.g. "печатает — «Chat Name»", nullopt if the user is idle.
[[nodiscard]] std::optional<QString> CurrentActivity(not_null<UserData*> user);

[[nodiscard]] rpl::producer<> ActivityChanged();

} // namespace Ayu::ActivityFeed
