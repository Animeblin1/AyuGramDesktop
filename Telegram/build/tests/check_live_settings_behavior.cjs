const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const root = path.resolve(__dirname, '../../..');
const read = file => fs.readFileSync(path.join(root, file), 'utf8');
const feed = read('Telegram/SourceFiles/ayu/features/activity/activity_feed.cpp');
assert.match(feed, /liveUserActivityValue\([\s\S]*?if \(!enabled\) \{[\s\S]*?entries\.clear\(\);[\s\S]*?timer\.cancel\(\);[\s\S]*?ChangedStream\(\)\.fire/);
const scroll = read('Telegram/lib_ui/ui/widgets/scroll_area.cpp');
assert.match(scroll, /!AyuUiSettings::isSmoothScroll\(\)[\s\S]*?Qt::NoScrollPhase[\s\S]*?pixelDelta\(\)\.isNull\(\)[\s\S]*?angleDelta\(\) \* AyuUiSettings::getWheelScrollStep\(\)/);
assert.match(scroll, /ev->setAccepted\(scaled\.isAccepted\(\)\)/);
const elastic = read('Telegram/lib_ui/ui/widgets/elastic_scroll.cpp');
assert.match(elastic, /phase == Qt::NoScrollPhase[\s\S]*?!AyuUiSettings::isSmoothScroll\(\) && !touch[\s\S]*?getWheelScrollStep\(\)[\s\S]*?requestBottomContent\(delta\)/);
const history = read('Telegram/SourceFiles/history/history_inner_widget.cpp');
assert.match(history, /selectScrollX2\(\)[\s\S]*?OppositeSigns\(_selectBoostSpeed[\s\S]*?std::copysign[\s\S]*?0\.15/);
assert.equal((history.match(/_selectScroll\.cancel\(\);\s*_selectBoostSpeed = 0\.;/g) || []).length, 4);
for (const file of ['history/history_inner_widget.cpp', 'history/view/history_view_list_widget.cpp', 'dialogs/dialogs_inner_widget.cpp']) {
  assert.match(read('Telegram/SourceFiles/' + file), /onlineIndicatorValue\(\s*\)/);
}
assert.match(read('Telegram/SourceFiles/dialogs/dialogs_inner_widget.cpp'), /senderMiniAvatarsValue\(\)[\s\S]*?_rowsScrollCache\.clear\(\)/);
const settings = read('Telegram/SourceFiles/ayu/ayu_settings.cpp');
for (const key of ['smoothCaret', 'smoothScroll']) {
  assert.ok(settings.includes(`s._${key} = j.value("${key}"`), `${key} must load after restart`);
}
const additional = read('Telegram/SourceFiles/ayu/ui/settings/settings_additional.cpp');
assert.match(additional, /unsigned\(result\.rgb\(\)\)/);
assert.match(additional, /rightJustified\(6, QChar\('0'\)\)/);
assert.match(additional, /ayu_CaretColorUseAuto/);
console.log('PASS: live settings source contracts (not runtime GUI tests).');
