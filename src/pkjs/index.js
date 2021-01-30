//messagequeue
const MessageQueue = require('message-queue-pebble');

//clay
const Clay = require('pebble-clay');
const clayConfig = require('./config.json');
const clay = new Clay(clayConfig, null, { autoHandleEvents: false });


var configuration = {
  DarkSkyKey: {value: 'e72e80e34606738a0fb55d5addda956a'},
  TemperatureUnits: {value: "Farenheit"}
}
var configuration = null;
//var location = [35.2020871,-101.8749806];
var location = [0,0];
var lastUpdate = 0;

//ready
Pebble.addEventListener("ready",
    function(e) {
        configuration = JSON.parse(localStorage.getItem("configuration"));
        lastUpdate = JSON.parse(localStorage.getItem("lastUpdate"));
        if ((new Date().getTime() - lastUpdate) > 3600000){
          if (configuration){
              //console.log("fetching fresh");
              navigator.geolocation.getCurrentPosition(geoLocationSuccess, geoLocationError, geoLocationOptions);
              //console.log(JSON.stringify(configuration));
          } else {
              Pebble.showSimpleNotificationOnPebble("Configuration Needed", "Please visit the watch face configuration page inside the Pebble phone app.");
          }
          // getWeather();
        } else {
          //console.log("too soon");
        }
        //getWeather();
    }
);

Pebble.addEventListener('appmessage', function(e) {
  var getWeather = e.payload["GetWeather"];
  if (getWeather) {
    navigator.geolocation.getCurrentPosition(geoLocationSuccess, geoLocationError, geoLocationOptions);
  }
});

var geoLocationOptions = {
    enableHighAccuracy: true,
    timeout: 10000
};

function geoLocationSuccess(pos) {
    location[0] = pos.coords.latitude;
    location[1] = pos.coords.longitude;
    getWeather();
};

function geoLocationError(err) {
    //console.log('location error (' + err.code + '): ' + err.message);
    Pebble.showSimpleNotificationOnPebble("Error", "Geolocation fetch failed.");
};

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
    var req = new XMLHttpRequest();
    req.open('GET', 'https://api.darksky.net/forecast/' + configuration.DarkSkyKey.value + '/' + location[0] + "," + location[1] + '?exclude=minutely,alerts,flags&extend=hourly', true);
    req.onload = function(e) {
        if (req.readyState == 4) {
          // 200 - HTTP OK
            if(req.status == 200) {

                var response = JSON.parse(req.responseText);

                // response.currently 
                //
                // response.hourly.data[]
                //
                // and response.daily.data[] 
                
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
                for (i = 0; i < dailyHighs.length; i++) {
                    orderedHighs[i] = dailyHighs[i];
                }
                orderedHighs.sort(function (a, b) {
                    return (a < b) ? 1 : -1;
                });
                
                //console.log(JSON.stringify("orderedHighs"));
                //console.log(JSON.stringify(orderedHighs));
                
                // var orderedLows = [...dailyLows];
                var orderedLows = [];
                for (i = 0; i < dailyLows.length; i++) {
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

                
                // fill arrays
                var dayMarkerIndex = 0;
                for(i = 0; i < 144; i++){
                    temperatureView[i] = Math.round((weeklyHigh - Math.round(response.hourly.data[i].temperature)) * temperatureScale) + temperatureOffset;
                    cloudCoverView[i] = Math.round(response.hourly.data[i].cloudCover*10);
                    precipTypeView[i] = returnPrecipType(response.hourly.data[i].precipType, response.hourly.data[i].precipProbability);
                    precipProbabilityView[i] = Math.round(response.hourly.data[i].precipProbability*10);
                    humidityView[i] = Math.round(response.hourly.data[i].humidity*100);
                    pressureView[i] = Math.round(response.hourly.data[i].pressure*.1);
                    var windSpeed = Math.round(response.hourly.data[i].windGust/10);
                    if (windSpeed > 4) {windSpeed = 4};
                    windSpeedView[i] = ( i % 2 ? 5 - windSpeed : 5 + windSpeed);


                    //set day marker
                    if(i < 143){                      
                      var thisDate = new Date(response.hourly.data[i].time*1000);
                      var thatDate = new Date(response.hourly.data[i+1].time*1000);
                      if(thisDate.getDate() != thatDate.getDate()){
                        dayMarkerView[dayMarkerIndex] = i;
                        dayMarkerIndex++;
                      }
                    }
                }

                //helper function to change precip type from string to int
                function returnPrecipType(type, chance){
                    if (!type || chance < .10){
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
                console.log("windSpeedView");
                console.log(JSON.stringify(windSpeedView));
                //console.log("dayMarkerView");
                //console.log(JSON.stringify(dayMarkerView));
                //console.log("dayOfTheWeekView");
                //console.log(JSON.stringify(dayOfTheWeekView));
                //console.log("temp");
                //console.log(JSON.stringify(t(response.currently.temperature)));

                var currently = Math.round(t(response.currently.temperature));
                //console.log(currently.toString());

                Pebble.sendAppMessage({
                    "GraphTemperature": Array.from(temperatureView),
                    "GraphCloudCover": Array.from(cloudCoverView),
                    "GraphPrecipType": Array.from(precipTypeView),
                    "GraphPrecipProb": Array.from(precipProbabilityView),
                    "GraphHumidity": Array.from(humidityView),
                    // "GraphPressure": Array.from(pressureView),
                    "GraphWindSpeed": Array.from(windSpeedView),
                    "DailyHighs": Array.from(dailyHighsView),
                    "DailyLows": Array.from(dailyLowsView),
                    "DayMarkers": Array.from(dayMarkerView),
                    "DaysOfTheWeek": Array.from(dayOfTheWeekView),
                    "CurrentTemperature": currently.toString() + "°"
                }, function(success){
                  localStorage.setItem("lastUpdate", new Date().getTime());
                  lastUpdate = new Date().getTime();
                });
            }
        }
    }
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