#!/usr/bin/env node
/**
 * Patches pebble-clay so that in the emulator we use pebblejs://close# as the
 * return URL: in emulator we use a placeholder $$$RETURN_TO$$$ so the SDK can
 * inject the localhost return URL before opening (see patch-sdk-browser.py).
 * This makes "Save" work when using pebble emu-app-config without the dead proxy.
 * Run after npm install (see package.json postinstall).
 */
const fs = require('fs');
const path = require('path');

const clayIndex = path.join(__dirname, '..', 'node_modules', 'pebble-clay', 'index.js');
if (!fs.existsSync(clayIndex)) {
  console.log('patch-clay: pebble-clay not installed, skip');
  return;
}

let content = fs.readFileSync(clayIndex, 'utf8');

const oldBlock = `Clay.prototype.generateUrl = function() {
  var settings = {};
  var emulator = !Pebble || Pebble.platform === 'pypkjs';
  var returnTo = emulator ? '$$$RETURN_TO$$$' : 'pebblejs://close#';

  try {
    settings = JSON.parse(localStorage.getItem('clay-settings')) || {};
  } catch (e) {
    console.error(e.toString());
  }

  var compiledHtml = configPageHtml
    .replace('$$RETURN_TO$$', returnTo)
    .replace('$$CUSTOM_FN$$', toSource(this.customFn))
    .replace('$$CONFIG$$', toSource(this.config))
    .replace('$$SETTINGS$$', toSource(settings))
    .replace('$$COMPONENTS$$', toSource(this.components))
    .replace('$$META$$', toSource(this.meta));

  // if we are in the emulator then we need to proxy the data via a webpage to
  // obtain the return_to.
  // @todo calculate this from the Pebble object or something
  if (emulator) {
    return Clay.encodeDataUri(
      compiledHtml,
      'http://clay.pebble.com.s3-website-us-west-2.amazonaws.com/#'
    );
  }

  return Clay.encodeDataUri(compiledHtml);
};`;

const newBlock = `Clay.prototype.generateUrl = function() {
  var settings = {};
  var emulator = !Pebble || Pebble.platform === 'pypkjs';
  // Placeholder so emu-app-config can inject localhost return URL (SDK replaces before opening).
  // Real device uses pebblejs://close#
  var returnTo = emulator ? '$$$RETURN_TO$$$' : 'pebblejs://close#';

  try {
    settings = JSON.parse(localStorage.getItem('clay-settings')) || {};
  } catch (e) {
    console.error(e.toString());
  }

  var compiledHtml = configPageHtml
    .replace('$$RETURN_TO$$', returnTo)
    .replace('$$CUSTOM_FN$$', toSource(this.customFn))
    .replace('$$CONFIG$$', toSource(this.config))
    .replace('$$SETTINGS$$', toSource(settings))
    .replace('$$COMPONENTS$$', toSource(this.components))
    .replace('$$META$$', toSource(this.meta));

  // Emulator: return data URI only (no proxy - clay.pebble.com is defunct).
  return Clay.encodeDataUri(compiledHtml);
};`;

if (content.includes("'http://clay.pebble.com.s3-website-us-west-2.amazonaws.com/#'")) {
  content = content.replace(oldBlock, newBlock);
  fs.writeFileSync(clayIndex, content);
  console.log('patch-clay: pebble-clay patched (emulator uses $$$RETURN_TO$$$ placeholder)');
} else if (content.includes('$$$RETURN_TO$$$')) {
  console.log('patch-clay: pebble-clay already patched');
} else {
  console.warn('patch-clay: pebble-clay index.js format changed, patch may need update');
}
