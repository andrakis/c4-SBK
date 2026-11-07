// web-sbk.mjs - Super Blit Kart in the browser, driven with real keys: on C4IX's
// display, in the Terminal window it was started from (-window), and in a window
// of grin's under C4DOS. Each takes the keys, drives a kart and gives the machine
// back; the screenshots are for looking at. Over CDP, in a tab of its own in one
// of the user's Chromes (c4's libjs/tests/cdp.mjs), against a page served from c4's
// tree through the https proxy, so that it is a secure context as the public page is:
//
//   node libjs/serve.mjs PORT --isolate &
//   node games/sbk/c4/web-sbk.mjs http://127.0.0.1:9224 https://PORT.code.home.stargazer.onl OUTDIR

import { writeFileSync, mkdirSync } from 'node:fs';
import { Browser } from '../../../libjs/tests/cdp.mjs';

const [cdp = 'http://127.0.0.1:9224', base = 'http://127.0.0.1:8472', outdir = '/tmp/web-sbk'] = process.argv.slice(2);
mkdirSync(outdir, { recursive: true });
let fail = 0;
const ok = (what, cond, extra = '') => {
  console.log(`web-sbk: ${what} ${cond ? 'OK' : 'FAILED'}${!cond && extra ? '\n  ' + String(extra).slice(-300) : ''}`);
  if (!cond) fail = 1;
  return cond;
};
const sleep = ms => new Promise(r => setTimeout(r, ms));

const KEY = {
  Enter: [13, 'Enter', '\r'], Escape: [27, 'Escape'], ArrowLeft: [37, 'ArrowLeft'], ArrowUp: [38, 'ArrowUp'],
  ArrowRight: [39, 'ArrowRight'], ArrowDown: [40, 'ArrowDown'], z: [90, 'KeyZ', 'z'], x: [88, 'KeyX', 'x'], c: [67, 'KeyC', 'c'],
};

const b = await Browser.connect(cdp);
console.log(`web-sbk: ${b.version}`);
const tab = await b.newTab(false);
try {
  const shown = 'window.c4m.display.shownText';
  const shownHas = (s, t = 20000) => tab.waitFor(`${shown}.some(t => ${s instanceof RegExp ? `${s}.test(t.s)` : `t.s.trimEnd() === ${JSON.stringify(s)}`})`, t);
  const find = async s => tab.eval(`${shown}.filter(t => ${s instanceof RegExp ? `${s}.test(t.s)` : `t.s.trimEnd() === ${JSON.stringify(s)}`})`);
  const at = async (x, y) => {
    const w = await tab.eval("document.getElementById('display').width"), h = await tab.eval("document.getElementById('display').height");
    return tab.eval(`(() => { const r = document.getElementById('display').getBoundingClientRect();
      return { x: r.left + r.width * ${x / w}, y: r.top + r.height * ${y / h} }; })()`);
  };
  const clickAt = async (x, y) => {
    const p = await at(x, y);
    await tab.mouse('mouseMoved', p.x, p.y);
    await tab.mouse('mousePressed', p.x, p.y, 'left', 1);
    await tab.mouse('mouseReleased', p.x, p.y, 'left', 0);
    await sleep(200);
  };
  const size = () => tab.eval("(() => { const c = document.getElementById('display'); return [c.width, c.height]; })()");
  const shot = async name => { writeFileSync(`${outdir}/${name}.png`, await tab.screenshot()); };
  const pixel = (x, y) => tab.eval(`(() => { const c = document.getElementById('display');
    return [...c.getContext('2d').getImageData(${x}, ${y}, 1, 1).data].slice(0, 3); })()`);
  const region = (x, y, w, h) => tab.eval(`(() => { const c = document.getElementById('display');
    const d = c.getContext('2d').getImageData(${x}, ${y}, ${w}, ${h}).data;
    let s = 0; for (let i = 0; i < d.length; i++) s = (s * 31 + d[i]) | 0; return s; })()`);
  const down = async k => {
    const [vk, code, text] = KEY[k];
    await tab.send('Input.dispatchKeyEvent', { type: text ? 'keyDown' : 'rawKeyDown', key: k, code, windowsVirtualKeyCode: vk, text });
  };
  const up = async k => {
    const [vk, code] = KEY[k];
    await tab.send('Input.dispatchKeyEvent', { type: 'keyUp', key: k, code, windowsVirtualKeyCode: vk });
  };
  const tap = async (k, hold = 150) => { await down(k); await sleep(hold); await up(k); await sleep(250); };

  // a race from the main menu: Enter for Race, a kart and a track; the
  // countdown; drive, turn each way; show what changes in the picture
  const race = async (who, rx, ry, rw, rh, desert, drive = 0) => {
    await tap('Enter');             // Race
    await sleep(600);
    await shot(`${who}-kart-select`);
    await tap('Enter');             // the kart
    await sleep(1500);
    await shot(`${who}-track-select`);
    if (desert) {
      await tap('ArrowDown');
      await sleep(2500);
      await shot(`${who}-track-select-desert`);
    }
    await tap('Enter');             // the track
    await sleep(1200);
    await shot(`${who}-countdown`);
    await sleep(3200);
    await down('z');
    await sleep(2500);
    await shot(`${who}-driving`);
    const a = await region(rx, ry, rw, rh);
    await sleep(700);
    const c = await region(rx, ry, rw, rh);
    ok(`${who}: Z accelerates: the track goes by`, a !== c, a);
    await sleep(drive);             // and on: straight ahead soon leaves the road
    await shot(`${who}-further`);
    await down('ArrowLeft');
    await sleep(900);
    await up('ArrowLeft');
    await shot(`${who}-left`);
    await down('ArrowRight');
    await sleep(1500);
    await up('ArrowRight');
    await shot(`${who}-right`);
    await up('z');
    await sleep(300);
  };

  await tab.goto(`${base}/libjs/web/?persist=reset`);
  const loaded = await tab.waitFor('window.c4m && window.c4m.display', 90000);
  ok('the page is the page, not a sign-in', loaded, await tab.eval('location.href'));
  ok('a secure context', await tab.eval('window.isSecureContext && typeof SharedArrayBuffer !== "undefined"'));
  await tab.eval('window.c4m.display.recordText = true');
  ok('the C4IX desktop boots, a Terminal open', loaded && await shownHas(/^Terminal - task/, 180000));
  await sleep(1500);
  const [W0, H0] = await size();
  const term = (await find(/^Terminal - task/)).filter(t => t.y < H0 - 30).pop();
  const termX = term.x + 5, termY = term.y + 5;
  await clickAt(termX, termY);
  await tab.type('sbk -stats\n');

  // ---- on C4IX's display ----
  ok('sbk takes the display', await tab.waitFor(`document.getElementById('display').width === 640 && document.getElementById('display').height === 480`, 60000), await size());
  await sleep(4500);
  await shot('c4ix-1-intro');
  const sky = await pixel(10, 10);
  ok('the sky is upstream\'s colour, once the fade is over', sky[0] === 0x63 && sky[1] === 0x9b && sky[2] === 0xff, sky);
  await tap('z');                   // Press a button!
  await sleep(1500);
  await shot('c4ix-2-menu');
  await race('c4ix', 0, 300, 640, 180, true, 25000);   // the desert, for long enough to time it
  await tap('Escape');              // pause
  await sleep(300);
  await shot('c4ix-3-paused');
  const p1 = await region(0, 300, 640, 180);
  await sleep(700);
  const p2 = await region(0, 300, 640, 180);
  ok('Esc pauses: nothing moves', p1 === p2);
  await tap('ArrowDown');           // Restart
  await tap('ArrowDown');           // Quit
  await tap('Enter');
  await sleep(1000);
  await shot('c4ix-4-main-menu');
  const sky2 = await pixel(10, 10);
  ok('Quit is the main menu again', sky2[0] === 0x63 && sky2[1] === 0x9b, sky2);
  await sleep(300);
  await tap('Escape');              // Esc on the main menu: the game ends
  ok('Esc on the main menu gives the display back', await tab.waitFor(`document.getElementById('display').width === ${W0}`, 30000), await size());
  await sleep(2000);
  await tab.eval('window.c4m.display.shownText = []');
  await shownHas(/^Terminal - task/, 30000);
  await sleep(1000);
  await shot('c4ix-5-desktop-again');
  {
    const s = await find(/^sbk: /);
    console.log('web-sbk: on the display:', s.map(t => t.s.trim().replace('sbk: ', '')).join(' | '));
  }

  // ---- in the window it was started from ----
  await clickAt(termX, termY);
  await tab.type('sbk -window -stats\n');
  await sleep(7000);
  ok('-window leaves the display to the desktop', (await size())[0] === W0, await size());
  await shot('window-1-intro');
  // the Terminal's canvas: start where the title was
  const wx = term.x - 20, wy = term.y + 30;
  await tap('z');
  await sleep(2500);
  await shot('window-2-menu');
  await race('window', wx, wy + 200, 500, 150);
  await tap('Escape');
  await sleep(400);
  await tap('ArrowDown');
  await tap('ArrowDown');
  await tap('Enter');
  await sleep(1500);
  await shot('window-3-main-menu');
  await tap('Escape');
  await sleep(2500);
  await shot('window-4-closed');
  ok('no errors on the page', !tab.errors.length, tab.errors.join('\n'));

  // ---- under grin, C4DOS's graphical shell, in a window of grin's ----
  {
    await tab.goto(`${base}/libjs/web/?system=c4dos&persist=reset`);
    const up2 = await tab.waitFor('window.c4m && document.getElementById("terminal").innerText.includes("Starting grin")', 90000);
    await tab.eval('window.c4m.display.recordText = true');
    await sleep(500);
    await clickAt(620, 300);
    ok('the page boots into grin', up2 && await shownHas('Program Manager', 60000));
    const clickAt0 = async (x, y) => {
      const p = await at(x, y);
      await tab.mouse('mouseMoved', p.x, p.y);
      await tab.mouse('mousePressed', p.x, p.y, 'left', 1);
      await tab.mouse('mouseReleased', p.x, p.y, 'left', 0);
      await sleep(60);
    };
    const clickT = async (t, dbl) => {
      await clickAt0(t.x + 5, t.y + 5);
      if (dbl) await clickAt0(t.x + 5, t.y + 5);
      await sleep(400);
    };
    await clickT((await find('Program Manager')).pop());
    await clickT((await find('Main')).sort((a, c) => a.x - c.x)[0]);
    const all = await find('DOS Prompt');
    await clickT(all.filter(t => t.x > 140)[0] || all[all.length - 1], true);
    ok('a DOS Prompt in grin', await shownHas('DOS Prompt 1', 20000));
    await tab.type('CD \\GAMES\\SBK\n');
    await sleep(800);
    await tab.type('sbk\n');
    ok('sbk opens a window of grin\'s', await shownHas('Super Blit Kart', 30000));
    await sleep(6000);
    await shot('grin-1-intro');
    const title = (await find('Super Blit Kart')).pop();
    console.log(`web-sbk: grin's title at ${title.x},${title.y}, display ${await size()}`);
    await tap('z');
    await sleep(3000);
    await shot('grin-2-menu');
    const x0 = Math.max(0, title.x - 200), y0 = title.y + 120;
    await race('grin', x0, y0, 400, 100);
    await tap('Escape');
    await sleep(500);
    await tap('ArrowDown');
    await tap('ArrowDown');
    await tap('Enter');
    await sleep(1500);
    await tap('Escape');
    await sleep(2500);
    await shot('grin-3-closed');
    ok('Esc on the main menu closes it', await tab.waitFor(`!${shown}.some(t => t.s.trimEnd() === 'Super Blit Kart')`, 15000));
    ok('no errors on the page under grin', !tab.errors.length, tab.errors.join('\n'));
  }
} finally {
  await tab.close();
  b.disconnect();
}
console.log(`web-sbk: ${fail ? 'FAILED' : 'all OK'}`);
process.exit(fail);
