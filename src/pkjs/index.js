//messagequeue
const MessageQueue = require('message-queue-pebble');

//clay
const Clay = require('pebble-clay');
const clayConfig = require('./config.json');
const clay = new Clay(clayConfig, null, { autoHandleEvents: false });

// Optional: loaded from api-key-injected.js (API key + optional fixed coordinates) for dev
var DEV_API_KEY = '';
var DEV_LAT = null;
var DEV_LON = null;
try {
  var injected = require('./api-key-injected.js');
  if (injected && injected.apiKey) DEV_API_KEY = injected.apiKey;
  if (injected && injected.lat != null && injected.lon != null) {
    DEV_LAT = injected.lat;
    DEV_LON = injected.lon;
  }
} catch (e) {}

var configuration = null;
var location = [0,0];
var lastUpdate = 0;

function hasApiKey() {
  var cfg = getConfiguration();
  var key = (typeof DEV_API_KEY !== 'undefined' && DEV_API_KEY)
    ? DEV_API_KEY
    : (cfg && cfg.APIkey && cfg.APIkey.value ? cfg.APIkey.value : '');
  return !!key;
}

/** Returns saved config, or default config when using injected API key and no config saved yet. */
function getConfiguration() {
  var saved = configuration || JSON.parse(localStorage.getItem("configuration") || "null");
  if (saved) return saved;
  if (typeof DEV_API_KEY !== 'undefined' && DEV_API_KEY) {
    return {
      APIkey: { value: DEV_API_KEY },
      TemperatureUnits: { value: "Farenheit" },
      HorizonDays: { value: "6" }
    };
  }
  return null;
}

function tryFetchWeather() {
  configuration = JSON.parse(localStorage.getItem("configuration") || "null");
  if (!configuration && DEV_API_KEY) {
    configuration = getConfiguration();
  }
  lastUpdate = parseInt(localStorage.getItem("lastUpdate") || "0", 10);
  var now = new Date().getTime();
  if (!hasApiKey()) {
    console.log("[pkjs] tryFetchWeather: no API key, skip");
    return;
  }
  if (lastUpdate && (now - lastUpdate) < 3600000) {
    console.log("[pkjs] tryFetchWeather: cache fresh (" + Math.round((now - lastUpdate) / 60000) + " min ago), skip");
    return;
  }
  if (DEV_LAT != null && DEV_LON != null) {
    location[0] = DEV_LAT;
    location[1] = DEV_LON;
    console.log("[pkjs] user coordinates (injected): " + location[0] + ", " + location[1]);
    getWeather();
    return;
  }
  console.log("[pkjs] tryFetchWeather: getting location...");
  navigator.geolocation.getCurrentPosition(geoLocationSuccess, geoLocationError, geoLocationOptions);
}

// On app ready: fetch weather if we have API key (and cache is stale or missing)
Pebble.addEventListener("ready", function(e) {
  configuration = JSON.parse(localStorage.getItem("configuration") || "null");
  console.log("[pkjs] ready, hasApiKey=" + hasApiKey());
  tryFetchWeather();
});

Pebble.addEventListener('appmessage', function(e) {
  var getWeather = e.payload["GetWeather"];
    if (getWeather) {
    configuration = JSON.parse(localStorage.getItem("configuration") || "null");
    if (!configuration && DEV_API_KEY) configuration = getConfiguration();
    console.log("[pkjs] GetWeather from watch, hasApiKey=" + hasApiKey());
    if (hasApiKey()) {
      if (DEV_LAT != null && DEV_LON != null) {
        location[0] = DEV_LAT;
        location[1] = DEV_LON;
        console.log("[pkjs] user coordinates (injected): " + location[0] + ", " + location[1]);
        getWeather();
      } else {
        navigator.geolocation.getCurrentPosition(geoLocationSuccess, geoLocationError, geoLocationOptions);
      }
    }
  }
});

var geoLocationOptions = {
    enableHighAccuracy: true,
    timeout: 10000
};

function geoLocationSuccess(pos) {
    location[0] = pos.coords.latitude;
    location[1] = pos.coords.longitude;
    console.log("[pkjs] user coordinates: latitude " + location[0] + ", longitude " + location[1]);
    getWeather();
}

function geoLocationError(err) {
    console.log("[pkjs] geo error " + err.code + ": " + err.message);
}

Pebble.addEventListener('showConfiguration', function(e) {
    Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function(e) {
    if (e && !e.response) {
        return;
    }
    //console.log('web view closed');
    configuration = clay.getSettings(e.response, false);
    localStorage.setItem("configuration", JSON.stringify(configuration));
    //console.log(JSON.stringify(configuration));
    navigator.geolocation.getCurrentPosition(geoLocationSuccess, geoLocationError, geoLocationOptions);
});

function getWeather(){
    configuration = configuration || getConfiguration();
    if (!configuration) {
        console.log("[pkjs] getWeather: no configuration");
        return;
    }
    var apiKey = (typeof DEV_API_KEY !== 'undefined' && DEV_API_KEY) ? DEV_API_KEY : (configuration.APIkey && configuration.APIkey.value ? configuration.APIkey.value : '');
    if (!apiKey) {
        console.log("[pkjs] getWeather: no API key");
        return;
    }
    var req = new XMLHttpRequest();
    var unitsParam = (configuration.TemperatureUnits && configuration.TemperatureUnits.value === 'Celsius') ? '&units=si' : '&units=us';
    var apiURL = 'https://api.pirateweather.net/forecast/' + apiKey + '/' + location[0] + ',' + location[1] + '?exclude=minutely,alerts,flags&extend=hourly' + unitsParam;
    console.log("[pkjs] getWeather: request start");
    req.open('GET', apiURL, true);
    req.timeout = 15000;
    req.ontimeout = function() { console.log("[pkjs] getWeather: request timeout (emulator may block network; try a real device)"); };
    req.onload = function(e) {
        if (req.readyState == 4) {
            if(req.status == 200) {

                var response = JSON.parse(req.responseText);
                // console.log(JSON.stringify(response));

                // Horizon: 1–6 days from config, default 6
                var D = 6;
                if (configuration.HorizonDays && configuration.HorizonDays.value) {
                  D = parseInt(configuration.HorizonDays.value, 10);
                  if (isNaN(D) || D < 1 || D > 6) D = 6;
                }
                var totalHours = D * 24;
                var hourlyData = response.hourly.data;

                var dailyHighs = [];
                var dailyLows = [];
                // var dailyHighsAndLows = [];

                var dayOfTheWeekBuffer = new ArrayBuffer(7);
                var dayOfTheWeekView = new Uint8Array(dayOfTheWeekBuffer);

                var dailyHighsBuffer = new ArrayBuffer(7);
                var dailyHighsView = new Uint8Array(dailyHighsBuffer);

                var dailyLowsBuffer = new ArrayBuffer(7);
                var dailyLowsView = new Uint8Array(dailyLowsBuffer);

                for (i = 0; i < 7; i++) {
                  dailyHighs.push(Math.round(response.daily.data[i].temperatureHigh));
                  dailyLows.push(Math.round(response.daily.data[i].temperatureLow));
                  dayOfTheWeekView[i] = new Date(response.daily.data[i].time * 1000).getDay();
                  // dailyHighsAndLows.push(""+Math.round(response.daily.data[i].temperatureHigh)+"\n"+Math.round(response.daily.data[i].temperatureLow));
                  dailyHighsView[i] = Math.round(t(response.daily.data[i].temperatureHigh)) & 255;
                  dailyLowsView[i] = Math.round(t(response.daily.data[i].temperatureLow)) & 255;
                }

                //console.log(JSON.stringify("dailyHighs"));
                //console.log(JSON.stringify(dailyHighs));
                //console.log(JSON.stringify("dailyLows"));
                //console.log(JSON.stringify(dailyLows));
                //console.log(JSON.stringify("dailyHighsView"));
                //console.log(JSON.stringify(dailyHighsView));
                //console.log(JSON.stringify("dailyLowsView"));
                //console.log(JSON.stringify(dailyLowsView));
                // //console.log(JSON.stringify("dailyHighsAndLows"));
                // //console.log(JSON.stringify(dailyHighsAndLows));

                //duplicate the array for ordering
                // var orderedHighs = [...dailyHighs];
                var orderedHighs = [];
                for (i = 0; i < Math.min(D, dailyHighs.length); i++) {
                    orderedHighs[i] = dailyHighs[i];
                }
                orderedHighs.sort(function (a, b) {
                    return (a < b) ? 1 : -1;
                });
                
                //console.log(JSON.stringify("orderedHighs"));
                //console.log(JSON.stringify(orderedHighs));
                
                // var orderedLows = [...dailyLows];
                var orderedLows = [];
                for (i = 0; i < Math.min(D, dailyLows.length); i++) {
                    orderedLows[i] = dailyLows[i];
                }
                orderedLows.sort(function (a, b) {
                    return (a > b) ? 1 : -1;
                });

                //console.log(JSON.stringify("orderedLows"));
                //console.log(JSON.stringify(orderedLows));

                //pull highs and lows
                var weeklyHigh = orderedHighs[0];
                var weeklyLow = orderedLows[0];

                //compute range to use as scaling ratio
                var weeklyTemperatureRange = weeklyHigh - weeklyLow;
                var temperatureScale = 1;
                var temperatureOffset = 10;
                if (weeklyTemperatureRange > 55) {
                    temperatureScale = 55/weeklyTemperatureRange;
                } else {
                  temperatureOffset += Math.round((55-weeklyTemperatureRange)/2)
                }

                // var temperatureScale = 55/weeklyTemperatureRange;
                
                //console.log("weeklyTemperatureRange");
                //console.log(weeklyTemperatureRange);

                //console.log("temperatureOffset");
                //console.log(temperatureOffset);

                //create 144 length arrays for each piece of information
                var temperatureBuffer = new ArrayBuffer(144);
                var temperatureView = new Uint8Array(temperatureBuffer);

                var cloudCoverBuffer = new ArrayBuffer(144);
                var cloudCoverView = new Uint8Array(cloudCoverBuffer);

                var precipTypeBuffer = new ArrayBuffer(144);
                var precipTypeView = new Uint8Array(precipTypeBuffer);
                
                var precipProbabilityBuffer = new ArrayBuffer(144);
                var precipProbabilityView = new Uint8Array(precipProbabilityBuffer);

                var humidityBuffer = new ArrayBuffer(144);
                var humidityView = new Uint8Array(humidityBuffer);
                
                var pressureBuffer = new ArrayBuffer(144);
                var pressureView = new Uint8Array(pressureBuffer);

                var windSpeedBuffer = new ArrayBuffer(144);
                var windSpeedView = new Uint8Array(windSpeedBuffer);

                var dayMarkerBuffer = new ArrayBuffer(6);
                var dayMarkerView = new Uint8Array(dayMarkerBuffer);

                
                // Resample D*24 hours to 144 columns with linear interpolation
                for (var j = 0; j < 144; j++) {
                  var h = (j * totalHours) / 144;
                  var h0 = Math.floor(h);
                  var h1 = Math.min(h0 + 1, totalHours - 1);
                  var frac = h - h0;
                  var d0 = hourlyData[h0];
                  var d1 = hourlyData[h1];

                  var tempRaw = (1 - frac) * d0.temperature + frac * d1.temperature;
                  var temp = Math.round((weeklyHigh - Math.round(tempRaw)) * temperatureScale) + temperatureOffset;
                  temperatureView[j] = Math.max(0, Math.min(255, temp));

                  var cloudRaw = (1 - frac) * (d0.cloudCover || 0) + frac * (d1.cloudCover || 0);
                  cloudCoverView[j] = Math.min(10, Math.round(cloudRaw * 10));

                  var precipIdx = frac < 0.5 ? h0 : h1;
                  precipTypeView[j] = returnPrecipType(hourlyData[precipIdx].precipType);
                  var precipProbRaw = (1 - frac) * (hourlyData[h0].precipProbability || 0) + frac * (hourlyData[h1].precipProbability || 0);
                  logPrecipProbability(j, precipProbRaw, precipTypeView[j]);
                  precipProbabilityView[j] = Math.round(temp * (precipProbRaw * 10));

                  humidityView[j] = Math.round(((1 - frac) * (hourlyData[h0].humidity || 0) + frac * (hourlyData[h1].humidity || 0)) * 100);
                  pressureView[j] = Math.round(((1 - frac) * (hourlyData[h0].pressure || 0) + frac * (hourlyData[h1].pressure || 0)) * 0.1);

                  var windRaw = (1 - frac) * (hourlyData[h0].windGust || 0) + frac * (hourlyData[h1].windGust || 0);
                  var windSpeed = Math.round(windRaw / 10);
                  if (windSpeed > 4) windSpeed = 4;
                  windSpeed = windSpeed * 2;
                  windSpeedView[j] = (j % 2 ? 9 - windSpeed : 9 + windSpeed);
                }

                // Day markers: boundaries at hours 24, 48, ..., (D-1)*24 → pixel positions
                for (var k = 0; k < D - 1; k++) {
                  var boundaryHour = (k + 1) * 24;
                  dayMarkerView[k] = Math.min(143, Math.round((boundaryHour / totalHours) * 144));
                }
                for (var k = D - 1; k < 6; k++) dayMarkerView[k] = 0;

                //helper function to change precip type from string to int
                function returnPrecipType(type){
                    if (!type){
                        return 0;
                    } else if (type == "rain") {
                        return 1;
                    } else {
                        //sleet or snow are the other available strings
                        return 2;
                    }
                }

                // function getDayString(int){
                //   switch(int){
                //     case 0:
                //       return "Su";
                //     case 1:
                //       return "Mo";
                //     case 2:
                //       return "Tu";
                //     case 3: 
                //       return "We";
                //     case 4:
                //       return "Th";
                //     case 5:
                //       return "Fr";
                //     case 6:
                //       return "Sa";
                //   }
                // }

                
                //console.log("temperatureView");
                //console.log(JSON.stringify(temperatureView));
                //console.log("cloudCoverView");
                //console.log(JSON.stringify(cloudCoverView));
                //console.log("precipTypeView");
                //console.log(JSON.stringify(precipTypeView));
                //console.log("precipProbabilityView");
                //console.log(JSON.stringify(precipProbabilityView));
                //console.log("humidityView");
                //console.log(JSON.stringify(humidityView));
                //console.log("pressureView");
                //console.log(JSON.stringify(pressureView));
                //console.log("windSpeedView");
                //console.log(JSON.stringify(windSpeedView));
                //console.log("dayMarkerView");
                //console.log(JSON.stringify(dayMarkerView));
                //console.log("dayOfTheWeekView");
                //console.log(JSON.stringify(dayOfTheWeekView));
                //console.log("temp");
                //console.log(JSON.stringify(t(response.currently.temperature)));

                var currently = Math.round(t(response.currently.temperature));
                //console.log(currently.toString());

                var message = {
                  "GraphTemperature": Array.from(temperatureView),
                  "GraphCloudCover": Array.from(cloudCoverView),
                  "GraphPrecipType": Array.from(precipTypeView),
                  "GraphPrecipProb": Array.from(precipProbabilityView),
                  //"GraphHumidity": Array.from(humidityView),
                  // "GraphPressure": Array.from(pressureView),
                  "GraphWindSpeed": Array.from(windSpeedView),
                  "DailyHighs": Array.from(dailyHighsView),
                  "DailyLows": Array.from(dailyLowsView),
                  "DayMarkers": Array.from(dayMarkerView),
                  "DaysOfTheWeek": Array.from(dayOfTheWeekView),
                  "CurrentTemperature": currently.toString() + "°",
                  "HorizonDays": D
                }

                console.log("[pkjs] getWeather: 200 OK, current=" + message.CurrentTemperature);
                console.log(JSON.stringify(message));

                Pebble.sendAppMessage(message, function(success){
                  if (success) {
                    console.log("[pkjs] sendAppMessage success");
                  } else {
                    console.log("[pkjs] sendAppMessage failed");
                  }
                  localStorage.setItem("lastUpdate", new Date().getTime());
                  lastUpdate = new Date().getTime();
                });
            } else {
                console.log("[pkjs] getWeather: HTTP " + req.status);
            }
        }
    };
    req.onerror = function() { console.log("[pkjs] getWeather: network error (check emulator network; try a real device)"); };
    req.send();
}

//convert to celsius or not
function t(temp) {
  if (!configuration.TemperatureUnits.value || configuration.TemperatureUnits.value == "Farenheit") {
    return temp;
  } else {
    //celsius
    return (temp - 32) * 5/9;
  }
}


// polyfill for ancient iOS JS environment
// Production steps of ECMA-262, Edition 6, 22.1.2.1
if (!Array.from) {
    Array.from = (function () {
      var toStr = Object.prototype.toString;
      var isCallable = function (fn) {
        return typeof fn === 'function' || toStr.call(fn) === '[object Function]';
      };
      var toInteger = function (value) {
        var number = Number(value);
        if (isNaN(number)) { return 0; }
        if (number === 0 || !isFinite(number)) { return number; }
        return (number > 0 ? 1 : -1) * Math.floor(Math.abs(number));
      };
      var maxSafeInteger = Math.pow(2, 53) - 1;
      var toLength = function (value) {
        var len = toInteger(value);
        return Math.min(Math.max(len, 0), maxSafeInteger);
      };
  
      // The length property of the from method is 1.
      return function from(arrayLike/*, mapFn, thisArg */) {
        // 1. Let C be the this value.
        var C = this;
  
        // 2. Let items be ToObject(arrayLike).
        var items = Object(arrayLike);
  
        // 3. ReturnIfAbrupt(items).
        if (arrayLike == null) {
          throw new TypeError('Array.from requires an array-like object - not null or undefined');
        }
  
        // 4. If mapfn is undefined, then let mapping be false.
        var mapFn = arguments.length > 1 ? arguments[1] : void undefined;
        var T;
        if (typeof mapFn !== 'undefined') {
          // 5. else
          // 5. a If IsCallable(mapfn) is false, throw a TypeError exception.
          if (!isCallable(mapFn)) {
            throw new TypeError('Array.from: when provided, the second argument must be a function');
          }
  
          // 5. b. If thisArg was supplied, let T be thisArg; else let T be undefined.
          if (arguments.length > 2) {
            T = arguments[2];
          }
        }
  
        // 10. Let lenValue be Get(items, "length").
        // 11. Let len be ToLength(lenValue).
        var len = toLength(items.length);
  
        // 13. If IsConstructor(C) is true, then
        // 13. a. Let A be the result of calling the [[Construct]] internal method 
        // of C with an argument list containing the single item len.
        // 14. a. Else, Let A be ArrayCreate(len).
        var A = isCallable(C) ? Object(new C(len)) : new Array(len);
  
        // 16. Let k be 0.
        var k = 0;
        // 17. Repeat, while k < len… (also steps a - h)
        var kValue;
        while (k < len) {
          kValue = items[k];
          if (mapFn) {
            A[k] = typeof T === 'undefined' ? mapFn(kValue, k) : mapFn.call(T, kValue, k);
          } else {
            A[k] = kValue;
          }
          k += 1;
        }
        // 18. Let putStatus be Put(A, "length", len, true).
        A.length = len;
        // 20. Return A.
        return A;
      };
    }());
  }