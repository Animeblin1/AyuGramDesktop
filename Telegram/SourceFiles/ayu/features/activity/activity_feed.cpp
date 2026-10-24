// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/features/activity/activity_feed.h"

#include "ayu/ayu_settings.h"
#include "base/flat_map.h"
#include "base/unixtime.h"
#include "data/data_channel.h"
#include "data/data_chat.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "mtproto/mtproto_types.h"

namespace Ayu::ActivityFeed {
namespace {

struct Entry {
	PeerId chatId = 0;
	QString text;
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

[[nodiscard]] QString ActionText(const MTPSendMessageAction &action) {
	switch (action.type()) {
	case mtpc_sendMessageTypingAction:
		return tr::lng_typing(tr::now);
	case mtpc_sendMessageRecordVideoAction:
		return tr::lng_send_action_record_video(tr::now);
	case mtpc_sendMessageUploadVideoAction:
		return tr::lng_send_action_upload_video(tr::now);
	case mtpc_sendMessageRecordAudioAction:
		return tr::lng_send_action_record_audio(tr::now);
	case mtpc_sendMessageUploadAudioAction:
		return tr::lng_send_action_upload_audio(tr::now);
	case mtpc_sendMessageRecordRoundAction:
		return tr::lng_send_action_record_round(tr::now);
	case mtpc_sendMessageUploadRoundAction:
		return tr::lng_send_action_upload_round(tr::now);
	case mtpc_sendMessageUploadPhotoAction:
		return tr::lng_send_action_upload_photo(tr::now);
	case mtpc_sendMessageUploadDocumentAction:
		return tr::lng_send_action_upload_file(tr::now);
	case mtpc_sendMessageChooseStickerAction:
		return tr::lng_send_action_choose_sticker(tr::now);
	case mtpc_sendMessageChooseContactAction:
	case mtpc_sendMessageGeoLocationAction:
	case mtpc_sendMessageGamePlayAction:
	case mtpc_sendMessageHistoryImportAction:
	case mtpc_sendMessagePhoneCallAction:
		return tr::lng_typing(tr::now);
	default:
		return QString();
	}
}

} // namespace

void HandleSendAction(
		not_null<Main::Session*> session,
		not_null<UserData*> from,
		not_null<PeerData*> where,
		const MTPSendMessageAction &action,
		TimeId when) {
	const auto &settings = AyuSettings::getInstance();
	if (!settings.liveUserActivity()) {
		return;
	}
	if (action.type() == mtpc_sendMessageCancelAction) {
		if (Entries().remove(from->id)) {
			ChangedStream().fire({});
		}
		return;
	}
	const auto text = ActionText(action);
	if (text.isEmpty()) {
		return;
	}
	auto &entry = Entries()[from->id];
	entry.chatId = where->id;
	if (where->isUser() && where->id != from->id) {
		// A private chat with the viewer - the action speaks for itself.
		entry.text = text;
	} else if (where->isSelf()) {
		entry.text = text;
	} else {
		entry.text = QString("%1 — «%2»").arg(text, where->name());
	}
	entry.until = crl::now() + 7000;
	ChangedStream().fire({});
}

std::optional<QString> CurrentActivity(not_null<UserData*> user) {
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
	return i->second.text;
}

rpl::producer<> ActivityChanged() {
	return ChangedStream().events();
}

} // namespace Ayu::ActivityFeed
