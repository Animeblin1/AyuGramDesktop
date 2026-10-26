const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const root = path.resolve(__dirname, '../../..');
const read = p => fs.readFileSync(path.join(root, p), 'utf8');

const list = read('Telegram/SourceFiles/history/view/history_view_list_widget.cpp');
assert.match(list, /onlineIndicator\(\)[\s\S]*?Data::IsUserOnline/);
assert.match(list, /p\.save\(\)[\s\S]*?dialogsOnlineBadgeFg[\s\S]*?p\.restore\(\)/);

const feed = read('Telegram/SourceFiles/ayu/features/activity/activity_feed.cpp');
assert.match(feed, /std::tuple<PeerId, PeerId, MsgId>/);
assert.match(feed, /std::tuple\{ from->id, where->id, topic \}/);

const history = read('Telegram/SourceFiles/history/history.cpp');
assert.match(history, /Ayu::ActivityFeed::ClearActivity\(from, peer, item->topicRootId\(\)\)/);

const settings = read('Telegram/SourceFiles/ayu/ayu_settings.h');
assert.match(settings, /wheelScrollStepValue\(\)/);

const builder = read('Telegram/SourceFiles/ayu/ui/settings/ayu_builder.cpp');
assert.match(builder, /std::move\(args\.value\)|std::move\(value\)/);
assert.match(builder, /value \? std::move\(value\) : rpl::single/);

const appearance = read('Telegram/SourceFiles/ayu/ui/settings/settings_appearance.cpp');
assert.match(appearance, /settings->smoothCaretValue\(\)/);
assert.match(appearance, /settings->caretSpeedValue\(\)/);
assert.match(appearance, /settings->wheelScrollStepValue\(\)/);

console.log('PASS: focused feature-audit source contracts (not runtime tests).');
