function plot(log, callback) {

    var charts = document.getElementById('charts')

    while (charts.firstChild)
        charts.removeChild(charts.firstChild);

    var jumps = log.split("||");

    plotAjax(jumps, jumps.length - 2, callback);
}

function plotAjax(jumps, i, callback) {

    var jump = jumps[i].trim();
    var jumpData = jump.split("|");
    var id = Number(jumpData[0]);

    if (ids && !ids.includes(id.toString())) {
        --i;
        if (i >= 0) {
            plotAjax(jumps, i);
        }
        return;
    }

    var jumpNumber = Number(jumpData[1]);
    var dateTime = new Date(Number(jumpData[2]) * 1000);
    var location = jumpData[3];
    var aircraft = jumpData[4];
    var details;
    if (jumpData.length == 6)
        details = getJumpDetails(jumpData[5]);

    var canvas = document.createElement("canvas");
    canvas.id = "chart_" + id;
    charts.appendChild(canvas);
    var div = document.createElement("div");
    div.id = "div_" + id;
    charts.appendChild(div);

    $.ajax({
        url: "logData_" + id + ".txt",
        dataType: "text",
        success: function (data) {

            console.log("logData_" + id + ".txt loaded");

            var readings = data.split(";");

            var time_arr = new Array(readings.length);
            var altitide_arr = new Array(readings.length);
            var speed_arr = new Array(readings.length);

            for (var j = 0; j < readings.length; j++) {
                var reading = readings[j].split(",");
                var time = Number(reading[0]) / 100;
                var altitude = Number(reading[1]) / 100;
                var ofst = (offset > j) ? j : offset;
                var readingOffset = readings[j - ofst].split(",");
                var timeOffset = Number(readingOffset[0]) / 100;
                var altitudeOffset = Number(readingOffset[1]) / 100;
                var speed = ((altitudeOffset - altitude) / (time - timeOffset)) * 3.6;

                time_arr[j] = time;
                altitide_arr[j] = altitude;
                speed_arr[j] = speed.toFixed(2);
            }

            var title = "jump " + jumpNumber;

            if (!details)
                details = calculateJumpDetails(id, time_arr, altitide_arr);

            drawChart(canvas, title, time_arr, altitide_arr, speed_arr, details);
            console.log("chart drawn");

            displayjumpDetails(dateTime, location, aircraft, details, div);
            console.log("details displayed");

            --i;
            if (i >= 0) {
                plotAjax(jumps, i, callback);
            }
            else {
                if (callback && typeof callback === "function")
                    callback();
            }
        }
    });
}

function drawChart(canvas, title, time_arr, altitide_arr, speed_arr, details) {

    var freefall_arr = new Array(time_arr.length);
    for (var i = 0; i < time_arr.length; i++) {
        freefall_arr[i] = (i >= details.indexOfExit && i <= details.indexOfOpening) ? altitide_arr[i] : NaN;
    }

    var chart = new Chart(canvas, {
        type: 'line',

        data: {
            labels: time_arr,
            datasets: [{
                label: "Altitude",
                borderWidth: lineWidth,
                borderColor: '#3366cc',
                backgroundColor: 'transparent',
                data: altitide_arr,
                yAxisID: "y-axis-left",
                pointHitRadius: 5
            },
            {
                label: "Speed",
                borderWidth: lineWidth,
                borderColor: '#dc3912',
                backgroundColor: 'transparent',
                data: speed_arr,
                yAxisID: "y-axis-right",
                pointHitRadius: 5
            },
            {
                label: "Freefall",
                borderWidth: lineWidth,
                borderColor: 'rgba(51, 102, 204, 0.2)',
                backgroundColor: 'rgba(51, 102, 204, 0.2)',
                data: freefall_arr,
                yAxisID: "y-axis-left",
                pointHitRadius: 0
            }]
        },

        options: {
            title: {
                display: true,
                text: title,
                fontSize: Math.max(windowWidth / 50, 14)
            },
            elements: {
                point: {
                    radius: 0
                }
            },
            legend: {
                labels: {
                    boxWidth: 1
                }
            },
            tooltips: {
                mode: 'index',
                backgroundColor: 'rgba(255,255,255,0.9)',
                titleFontColor: '#666',
                bodyFontColor: '#666',
                footerFontColor: '#666',
                borderColor: 'rgba(0,0,0,0.33)',
                borderWidth: 1,
                displayColors: false,
                position: 'nearest',
                callbacks: {
                    title: function (tooltipItems, data) {
                        var tooltipItem = tooltipItems[0];
                        return data.labels[tooltipItem.index] + " s";
                    },
                    label: function (tooltipItems, data) {
                        var labelSuffix = tooltipItems.datasetIndex == 0 ? " m" : " km/h";
                        return data.datasets[tooltipItems.datasetIndex].label + ': ' + tooltipItems.yLabel + labelSuffix;
                    }
                },
                filter: function (tooltipItem) {
                    return tooltipItem.datasetIndex < 2;
                }
            },
            responsive: true,
            onResize: function (chart, size) {
                windowWidth = document.body.clientWidth || document.documentElement.clientWidth || window.innerWidth;
                lineWidth = Math.max(windowWidth / 600, 1);
                chart.data.datasets.forEach(function (dataset) {
                    dataset.borderWidth = lineWidth;
                });
                chart.options.title.fontSize = Math.max(windowWidth / 50, 14);
            },
            scales: {
                xAxes: [{
                    scaleLabel: {
                        display: true,
                        labelString: 'Time (s)'
                    },
                    ticks: {
                        maxRotation: 0,
                        minRotation: 0
                    }
                }],
                yAxes: [{
                    type: "linear",
                    display: true,
                    position: "left",
                    id: "y-axis-left",
                    scaleLabel: {
                        display: true,
                        labelString: 'Altitude (m)'
                    },
                },
                {
                    type: "linear",
                    display: true,
                    position: "right",
                    id: "y-axis-right",
                    scaleLabel: {
                        display: true,
                        labelString: 'Speed (km/h)'
                    },
                }]
            }
        }
    });
}


function getJumpDetails(jumpDataDetails) {

    var d_arr = jumpDataDetails.split(";");

    var details = new Object();
    details.exitAltitude = Number(d_arr[0]);
    details.exitTime = Number(d_arr[1]);
    details.openingAltitude = Number(d_arr[2]);
    details.openingTime = Number(d_arr[3]);
    details.maxSpeed = Number(d_arr[4]).toFixed(2);
    details.maxSpeedTime = d_arr[5];
    details.averageSpeed = Number(d_arr[6]).toFixed(2);
    details.indexOfExit = Number(d_arr[7]);
    details.indexOfOpening = Number(d_arr[8]);

    console.log("jump details extracted");

    return details;
}

function calculateJumpDetails(id, time_arr, altitide_arr) {
    var offsetConstant = 10;
    var exitSpeedConstant = 50;
    var openingSpeedConstant = 50;

    var speed_arr = new Array(time_arr.length);
    for (var i = 0; i < time_arr.length; i++) {
        var time = time_arr[i];
        var altitude = altitide_arr[i];
        var ofst = (offsetConstant > i) ? i : offsetConstant;
        var timeOffset = time_arr[i - ofst];
        var altitudeOffset = altitide_arr[i - ofst];
        var speed = ((altitudeOffset - altitude) / (time - timeOffset)) * 3.6;

        speed_arr[i] = isNaN(speed) ? 0 : speed;
    }

    var indexOfMaxSpeed = speed_arr.reduce((iMax, x, i, arr) => x > arr[iMax] ? i : iMax, 0);
    var indexOfAfterExit;
    for (var i = indexOfMaxSpeed - 1; i > -1; i--) {
        if (speed_arr[i] < exitSpeedConstant) {
            indexOfAfterExit = i;
            break;
        }
    }
    var indexOfExit = indexOfAfterExit;
    var speedAfterExit = speed_arr[indexOfAfterExit];
    for (var i = indexOfAfterExit - 1; i > -1; i--) {
        if (speed_arr[i] < speedAfterExit) {
            indexOfExit = i;
            speedAfterExit = speed_arr[i];
        }
        else {
            break;
        }
    }
    var indexOfOpening;
    for (var i = indexOfMaxSpeed + 1; i < speed_arr.length; i++) {
        if (speed_arr[i] < openingSpeedConstant) {
            indexOfOpening = i - (offsetConstant - 1);
            break;
        }
    }

    var avgSpeed = ((altitide_arr[indexOfExit] - altitide_arr[indexOfOpening]) / (time_arr[indexOfOpening] - time_arr[indexOfExit])) * 3.6;

    var details = new Object();
    details.exitAltitude = altitide_arr[indexOfExit];
    details.exitTime = time_arr[indexOfExit];
    details.openingAltitude = altitide_arr[indexOfOpening];
    details.openingTime = time_arr[indexOfOpening];
    details.maxSpeed = speed_arr[indexOfMaxSpeed].toFixed(2);
    details.maxSpeedTime = time_arr[indexOfMaxSpeed];
    details.averageSpeed = avgSpeed.toFixed(2);
    details.indexOfExit = indexOfExit;
    details.indexOfOpening = indexOfOpening;

    console.log("jump details calculated");

    return details;
}

function displayjumpDetails(dateTime, location, aircraft, details, div) {
    div.innerHTML = "<table>" +
        "<tr><td>Date and time of jump: </td><td>" + dateTime.toString() + "</td></tr>" +
        "<tr><td>Location: </td><td>" + location + "</td></tr>" +
        "<tr><td>Aircraft: </td><td>" + aircraft + "</td></tr>" +
        "<tr><td>Exit altitude: </td><td>" + details.exitAltitude + " m (at " + details.exitTime + " s)" + "</td></tr>" +
        "<tr><td>Opening altitude: </td><td>" + details.openingAltitude + " m (at " + details.openingTime + " s)" + "</td></tr>" +
        "<tr><td>Freefall time: </td><td>" + (details.openingTime - details.exitTime) + " s" + "</td></tr>" +
        "<tr><td>Max speed (5 sec avg): </td><td>" + details.maxSpeed + " km/h (at " + details.maxSpeedTime + " s)" + "</td></tr>" +
        "<tr><td>Average speed: </td><td>" + details.averageSpeed + " km/h" + "</td></tr>" +
        "</table>"
        ;
}

Date.prototype.toString = function () {
    return (
        this.getUTCFullYear() + "-" + padNumber(this.getUTCMonth() + 1) + "-" + padNumber(this.getUTCDate()) +
        " " +
        padNumber(this.getUTCHours()) + ":" + padNumber(this.getUTCMinutes())
    );
}

function padNumber(n) {
    return ("0" + n).slice(-2);
}

function goTo(t) {
    window.location.href = t;
}

document.addEventListener('touchstart', handleTouchStart, false);
document.addEventListener('touchend', handleTouchEnd, false);
var xDown = null;
var yDown = null;

function handleTouchStart(evt) {
    xDown = evt.touches[0].clientX;
    yDown = evt.touches[0].clientY;
};

function handleTouchEnd(evt) {
    if (!xDown || !yDown) {
        return;
    }

    var xUp = evt.changedTouches[0].clientX;
    var yUp = evt.changedTouches[0].clientY;

    var xDiff = xUp - xDown;
    var yDiff = yUp - yDown;
    var totalDiff = Math.sqrt(Math.pow(Math.abs(xDiff), 2) + Math.pow(Math.abs(yDiff), 2));

    if (totalDiff > 50) {

        if (Math.abs(xDiff) > Math.abs(yDiff)) {
            if (xDiff > 0) // right swipe 
                onSwipeRight(yDown);
            else // left swipe
                onSwipeLeft(yDown);
        }

        xDown = null;
        yDown = null;
    }
};
