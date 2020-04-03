//messagequeue
const MessageQueue = require('message-queue-pebble');

//clay
const Clay = require('pebble-clay');
const clayConfig = require('./config.json');
const clay = new Clay(clayConfig, null, { autoHandleEvents: false });

var darSkyKey = '';
var configuration = null;
var location = [0,0];

//ready
Pebble.addEventListener("ready",
    function(e) {
        configuration = JSON.parse(localStorage.getItem("configuration"));
        if (configuration){
            navigator.geolocation.getCurrentPosition(geoLocationSuccess, geoLocationError, geoLocationOptions);
            console.log(JSON.stringify(configuration));
        } else {
            Pebble.showSimpleNotificationOnPebble("Configuration Needed", "Please visit the watch face configuration page inside the Pebble phone app.");
        }
    }
);

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
    console.log('location error (' + err.code + '): ' + err.message);
    Pebble.showSimpleNotificationOnPebble("Error", "Geolocation fetch failed.");
};

Pebble.addEventListener('showConfiguration', function(e) {
    Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function(e) {
    if (e && !e.response) {
        return;
    }
    console.log('web view closed');
    configuration = clay.getSettings(e.response, false);
    localStorage.setItem("configuration", JSON.stringify(configuration));
    console.log(JSON.stringify(configuration));
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
                
                let dailyHighBuffer = new ArrayBuffer(6);
                let dailyHighView = new Uint8Array(dailyHighBuffer);

                let dailyLowBuffer = new ArrayBuffer(6);
                let dailyLowView = new Uint8Array(dailyLowBuffer);

                //get daily higs and lows for the next 6 days
                for (let i = 0; i < 6; i++) {
                    dailyHighView[i] = Math.round(response.daily.data[i].temperatureHigh);
                    dailyLowView[i] = Math.round(response.daily.data[i].temperatureLow);
                }

                console.log(JSON.stringify("dailyHighView"));
                console.log(JSON.stringify(dailyHighView));
                console.log(JSON.stringify("dailyLowView"));
                console.log(JSON.stringify(dailyLowView));

                //duplicate the array for ordering
                let orderedHighs = [...dailyHighView];
                orderedHighs.sort((a, b) => (a < b) ? 1 : -1);
                console.log(JSON.stringify("orderedHighs"));
                console.log(JSON.stringify(orderedHighs));
                
                let orderedLows = [...dailyLowView];
                orderedLows.sort((a, b) => (a > b) ? 1 : -1);
                console.log(JSON.stringify("orderedLows"));
                console.log(JSON.stringify(orderedLows));

                //pull highs and lows
                var weeklyHigh = orderedHighs[0];
                var weeklyLow = orderedLows[0];
                //compute range to use as scaling ratio
                var weeklyTemperatureRange = weeklyHigh - weeklyLow;
                var temperatureScale = 55/weeklyTemperatureRange;
                console.log("weeklyTemperatureRange");
                console.log(weeklyTemperatureRange);

                //create 144 length arrays for each piece of information
                let temperatureBuffer = new ArrayBuffer(144);
                let temperatureView = new Uint8Array(temperatureBuffer);

                let cloudCoverBuffer = new ArrayBuffer(144);
                let cloudCoverView = new Uint8Array(cloudCoverBuffer);

                let precipTypeBuffer = new ArrayBuffer(144);
                let precipTypeView = new Uint8Array(precipTypeBuffer);
                
                let precipProbabilityBuffer = new ArrayBuffer(144);
                let precipProbabilityView = new Uint8Array(precipProbabilityBuffer);

                let humidityBuffer = new ArrayBuffer(144);
                let humidityView = new Uint8Array(humidityBuffer);
                
                let pressureBuffer = new ArrayBuffer(144);
                let pressureView = new Uint8Array(pressureBuffer);
                
                // fill arrays
                for(let i = 0; i < 144; i++){
                    temperatureView[i] = Math.round((weeklyHigh - Math.round(response.hourly.data[i].temperature)) * temperatureScale);
                    cloudCoverView[i] = Math.round(response.hourly.data[i].cloudCover*10);
                    precipTypeView[i] = returnPrecipType(response.hourly.data[i].precipType);
                    precipProbabilityView[i] = Math.round(response.hourly.data[i].precipProbability*10);
                    humidityView[i] = Math.round(response.hourly.data[i].humidity*100);
                    pressureView[i] = Math.round(response.hourly.data[i].pressure*.1);
                }

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

                
                // console.log("temperatureView");
                // console.log(JSON.stringify(temperatureView));
                // console.log("cloudCoverView");
                // console.log(JSON.stringify(cloudCoverView));
                // console.log("precipTypeView");
                // console.log(JSON.stringify(precipTypeView));
                // console.log("precipProbabilityView");
                // console.log(JSON.stringify(precipProbabilityView));
                // console.log("humidityView");
                // console.log(JSON.stringify(humidityView));
                // console.log("pressureView");
                // console.log(JSON.stringify(pressureView));

                MessageQueue.sendAppMessage({
                    GraphTemperature: Array.from(temperatureView),
                    GraphCloudCover: Array.from(cloudCoverView),
                    GraphPrecipType: Array.from(precipTypeView),
                    GraphPrecipProb: Array.from(precipProbabilityView),
                    GraphHumidity: Array.from(humidityView),
                    GraphPressure: Array.from(pressureView)
                });

            }
        }
    }
    req.send();
}
