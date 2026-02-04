# open-weather-graph
OpenWeatherGraph Pebble Watchface — inspired by Tomas Kafka's original WeatherGraph

![screenshot](https://raw.githubusercontent.com/pauleeeeee/open-weather-graph/master/screenshot.png)

## Weather API
This watchface uses [Pirate Weather](https://pirate-weather.net/) (Dark Sky–compatible). Get a free API key at [pirate-weather.apiable.io](https://pirate-weather.apiable.io/) and enter it in the watchface configuration in the Pebble app.

## Build and run
```bash
npm install
pebble build
pebble install --emulator basalt   # or your device
```

## Development — API key from a file
Put your Pirate Weather API key in **`api-key.txt`** in the project root (one line, no quotes). That file is gitignored. You can copy `api-key.example.txt` to `api-key.txt` and replace the placeholder. Then run `npm install` or `npm run inject-api-key`; the script writes the key into the bundle so the app uses it without the Clay config. If `api-key.txt` is missing or empty, the app falls back to the key from the config page.