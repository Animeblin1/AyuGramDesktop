const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const root = path.resolve(__dirname, '../../..');
const read = file => fs.readFileSync(path.join(root, file), 'utf8');
const dir = 'Telegram/SourceFiles/ayu/ui/settings/';
const additional = read(dir + 'settings_additional.cpp');
const old = read(dir + 'settings_appearance.cpp') + read(dir + 'settings_chats.cpp');
for (const id of ['smoothCaret', 'caretBlink', 'caretSpeedSlider', 'blinkMsSlider', 'caretAccent', 'caretColor', 'caretReset', 'scrollSpeedSlider', 'smoothScroll', 'profileGradientFromAvatar', 'selectScrollX2', 'webAppsWindowed', 'senderMiniAvatars', 'onlineIndicator', 'liveUserActivity', 'inlineCalculator']) {
  assert.ok(additional.includes(`u"ayu/${id}"_q`), `Missing migrated control ${id}`);
  assert.ok(!old.includes(`u"ayu/${id}"_q`), `Duplicate control left in old category ${id}`);
}
const main = read(dir + 'settings_main.cpp');
assert.match(main, /AyuAdditional::Id\(\)/);
assert.match(main, /tr::ayu_CategoryAdditional\(\)/);
assert.match(additional, /\.parentId = AyuMain::Id\(\)/);
assert.match(read('Telegram/CMakeLists.txt'), /ayu\/ui\/settings\/settings_additional.cpp/);
assert.doesNotMatch(additional, /field->setFocusFast\(\)/, 'Opening the settings category must not steal focus');
console.log('PASS: additional settings migration and registration source contracts (not GUI validation).');
