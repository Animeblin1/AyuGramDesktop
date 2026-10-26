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
assert.match(feed, /entry\.until > latest->until/);
assert.doesNotMatch(feed, /ranges::find_if\(entries/);

const setters = read('Telegram/SourceFiles/ayu/ayu_settings.cpp');
for (const [name, min, max] of [['CaretSpeed', 0, 100], ['ScrollSpeed', 0, 100], ['BlinkMs', 200, 1000]]) {
  assert.ok(setters.includes(`void AyuSettings::set${name}(int val) {\n\tval = std::clamp(val, ${min}, ${max});`) || setters.includes(`void AyuSettings::set${name}(int val) {\r\n\tval = std::clamp(val, ${min}, ${max});`), `${name} must validate at the setter boundary`);
}

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
assert.match(appearance, /wheelScrollStepValue\(/);
const sliderHeader = read('Telegram/SourceFiles/ayu/ui/settings/ayu_builder.h');
const fields = [...sliderHeader.match(/struct SliderArgs \{([\s\S]*?)\n\s*\};/)[1].matchAll(/^\s*[^;\r\n]+?\s+(\w+)(?:\s*=.*)?;/gm)].map(m => m[1]);
for (const source of [appearance, read('Telegram/SourceFiles/ayu/ui/settings/settings_chats.cpp')]) {
  for (const block of source.matchAll(/ayu\.addSlider\(\{([\s\S]*?)\n\t\}\);/g)) {
    const names = [...block[1].matchAll(/^\t\t\.(\w+)\s*=/gm)].map(m => m[1]);
    const indices = names.map(name => { assert.ok(fields.includes(name), `Unknown slider field ${name}`); return fields.indexOf(name); });
    assert.deepEqual(indices, [...indices].sort((a, b) => a - b), `C++ slider designators out of order: ${names}`);
  }
}

console.log('PASS: focused feature-audit source contracts (not runtime tests).');
