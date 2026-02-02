# open-weather-graph
OpenWeatherGraph Pebble Watchface... inspired by Tomas Kafka's original WeatherGraph

![screenshot](https://raw.githubusercontent.com/pauleeeeee/open-weather-graph/master/screenshot.png)

## Weather API
This watchface uses [Pirate Weather](https://pirateweather.net/) (Dark Sky–compatible). Get a free API key at [pirate-weather.apiable.io](https://pirate-weather.apiable.io/) and enter it in the watchface configuration in the Pebble app.

### Before deploying to the Rebble App Store
1. **Build the .pbw without your dev API key**  
   Unset `OPEN_WEATHER_GRAPH_API_KEY` (or use a clean environment), then run `npm install` and `pebble build`. The postinstall script writes an empty key into `api-key-injected.js` when the env var is unset, so store users will use the key they enter in the config page.
2. **Confirm no secrets are committed**  
   `src/pkjs/api-key-injected.js` and `.lock-waf_*` build files are in `.gitignore`. If `.lock-waf_linux_build` (or any lock file) was ever committed, remove it from the repo and rotate your Pirate Weather API key.
3. **Rebble submission**  
   Upload the `.pbw` from `build/` at [dev-portal.rebble.io](https://dev-portal.rebble.io/). Provide app name, description, release notes, and at least one screenshot (e.g. `screenshot.png`). See [Rebble Help – App Store submission](https://help.rebble.io/appstore-submission/).

### Development / emulator
To use your API key without typing it in the config each time, set the environment variable and inject it before building:

```bash
export OPEN_WEATHER_GRAPH_API_KEY=your_key_here
npm run inject-api-key
```

Then build and run as usual. The file `src/pkjs/api-key-injected.js` is generated and **gitignored**; do not commit it. For **release builds**, do not run `inject-api-key` (or run it with the variable unset) so the app uses the key from the configuration page.

### Emulator and logs ("Connection refused")
If `pebble install --emulator basalt --logs` (or diorite/aplite) says **Connection refused**, the tool is connecting to the emulator’s log port before the emulator is ready. Use either approach:

1. **Install first, then attach logs**  
   - Run: `pebble install --emulator basalt` (no `--logs`).  
   - Wait until the emulator window is up and the app is installed.  
   - Then run: `pebble install --emulator basalt --logs` again (or use the tool’s log command if you have one). The second run usually connects.

2. **Start the emulator, then install with logs**  
   - Start the emulator: `pebble emu basalt` (or your target).  
   - When the emulator is fully running, in another terminal run: `pebble install --emulator basalt --logs`.

For more detail on why the connection fails, use:  
`pebble install --emulator basalt --logs --debug-phonesim --debug`

### Safely exiting the emulator (avoid needing `pebble wipe`)
If you close the emulator window or press **Ctrl+C** in the terminal running `pebble install --emulator … --logs`, the pebble tool can end up in a bad state and you’ll need `pebble wipe` before using the emulator again. To exit cleanly:

1. **Kill the emulator via the tool first** (in another terminal, from the project directory):
   ```bash
   pebble kill
   ```
   This stops both the emulator and the phone simulator and lets the tool clean up.

2. **Then** in the terminal that’s running `pebble install --emulator … --logs`, press **Ctrl+C**. The process will see the connection drop and exit.

If you already closed the window or hit Ctrl+C and the tool is borked, run:
```bash
pebble wipe
```
Then you can start the emulator again with `pebble install --emulator basalt --logs`.

### Phone simulator window not opening
When you run `pebble install --emulator basalt`, the SDK normally starts two things: the **watch emulator** (QEMU) and the **phone simulator** (pypkjs, a Python GUI). If only the watch window appears and no phone window opens:

- **WSL / headless:** The phone simulator is a graphical app. It needs a display (e.g. X11). On WSL, set `DISPLAY` (e.g. use WSLg or an X server like VcXsrv) so the phone window can open. Without a display, the phone simulator may fail to start or show nothing.
- **Debug:** Run with `--debug-phonesim --debug` to see why the phone simulator might not be starting:  
  `pebble install --emulator basalt --debug-phonesim --debug`

**Workaround — open config in the browser:** With the **watch emulator already running** (watch window is up), open a second terminal in the project directory and run:

```bash
pebble emu-app-config
```

That opens the app's configuration page (e.g. Clay) in your system browser so you can set the API key and options without the phone simulator window. **Save** should work: this project patches `pebble-clay` so that in the emulator the config page redirects to `pebblejs://close#<data>` instead of the defunct Clay proxy. After you tap **Save**, the browser navigates to that URL and the SDK (or OS) should hand the config to the phone simulator so it gets sent to the watch. (For apps with `enableMultiJS`, `emu-app-config` can sometimes timeout; if it does, use the [injected API key](#development--emulator) for local dev.) Run `npm run postinstall` or `npm install` to re-apply the Clay patch if you reinstalled dependencies.

### Clay + SDK patch (emu-app-config Save)
The Clay config page used to redirect "Save" through a proxy at clay.pebble.com, which no longer exists. This repo uses two patches so **Save** works when using `pebble emu-app-config`:

1. **Clay** (automatic): `postinstall` runs `scripts/patch-clay.js`, which patches `node_modules/pebble-clay` so that in the emulator we return a **data URI** with a placeholder `$$$RETURN_TO$$$` instead of the proxy URL.
2. **SDK** (run once): Run `python3 scripts/patch-sdk-browser.py` once after installing the pebble tool (and again if you reinstall/update it). That patches the SDK's `browser.py` so that when the config URL is a data URI, it injects `http://localhost:PORT/close?` into the HTML before opening. When you tap **Save**, the page then redirects to that URL and the SDK receives the config and sends it to the watch.

**Steps:** After `npm install`, run `pebble build` (so the .pbw contains the patched Clay), run `python3 scripts/patch-sdk-browser.py`, then start the emulator and use `pebble emu-app-config`. If Save still doesn't send config, use the [injected API key](#development--emulator) for local dev.

### Testing the Clay config page in the emulator
Clay is designed to work in the emulator. It detects the emulator (e.g. when `Pebble.platform === 'pypkjs'`) and generates a config URL that is proxied via a Pebble-hosted page so the “Save” flow can return correctly.

1. Run the app in the emulator: `pebble install --emulator basalt` (or aplite/diorite).
2. In the **phone simulator** window (the companion “phone” that opens with the emulator), open the Pebble app, go to your watchface (OpenWeatherGraph), and tap **Configuration**.
3. The Clay config page should open (in the simulator’s browser or system browser, depending on your SDK). Enter your Pirate Weather API key and temperature units, then tap **Save**.
4. The page closes and the JS sends the settings to the watch; the watchface should then fetch weather using the new config.

If the phone window never appears, use the [workaround above](#phone-simulator-window-not-opening): run `pebble emu-app-config` to open the config in your browser. For local development you can also use the [injected API key](#development--emulator) (`OPEN_WEATHER_GRAPH_API_KEY` + `npm run inject-api-key`) and skip the config page.
