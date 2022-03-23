const APIGW='http://'+window.location.host+'/api/';

var gaugeOptions = {

    chart: {
        type: 'solidgauge'
    },

    title: null,

    pane: {
        center: ['50%', '85%'],
        size: '140%',
        startAngle: -90,
        endAngle: 90,
        background: {
            backgroundColor:
                Highcharts.defaultOptions.legend.backgroundColor || '#EEE',
            innerRadius: '60%',
            outerRadius: '100%',
            shape: 'arc'
        }
    },

    tooltip: {
        enabled: false
    },

    // the value axis
    yAxis: {
        stops: [
            [0.1, '#55BF3B'], // green
            [0.5, '#DDDF0D'], // yellow
            [0.9, '#DF5353'] // red
        ],
        lineWidth: 1,
        minorTickInterval: null,
        tickAmount: 8,
        title: {
            y: -70
        },
        labels: {
            y: 16
        }
    },

    plotOptions: {
        solidgauge: {
            dataLabels: {
                y: 5,
                borderWidth: 0,
                useHTML: true
            }
        }
    }
};

var Rooms = [];
var servoState = [0, 0, 0, 0, 0, 0, 0, 0, 0];
//var sensorTemp = [0, 0, 0, 0, 0, 0, 0, 0, 0];
//var sensorText = ["","","","","","","","","","",""];
var sensorInfo = [{},{},{},{},{},{},{},{},{},{},{}];

const app = document.getElementById('rooms_canvas');

var requestRoom  = new XMLHttpRequest();
var requestServo = new XMLHttpRequest();
var requestSensor= new XMLHttpRequest();
var requestPut   = new XMLHttpRequest();

// requestRoom.open('GET', APIGW+'room/list', true);

function verbalState(i)
{
    //sn="valve "+i;
    sn="";
    console.log("display servoState for servo "+i+"from servoState"+servoState);

    // servoState array starts at 0, where 0 is relais 16
    // ToDo: add remaining time for Closed/Loop
    //       add taget dt for Open

    switch(servoState[i]) {
        case 0:
            return sn+" Open";
        case 1:
            return sn+" Closed";
        case 2:
            return sn+" Opening";
        case 3:
            return sn+" Closing";
        default:
            return sn+" Special"
    }
}
var influx=30.0;

requestSensor.onload = function()
{
    var data = JSON.parse(this.response);

    if (requestSensor.status >= 200 && requestSensor.status < 400) {
        data["sensors"].forEach(sensor => {
            if (sensor.servonr > 0)
            {     
                temperature = Math.floor(sensor.temperature*10)/10;   
                delta = Math.floor((influx - temperature)*10)/10;
                sensorInfo[sensor.servonr] = {
                    temperature: temperature,
                    targetdelta: sensor.deltatemp,
                    delta: delta,
                    text: "&Delta;t<sub>"+sensor.servonr+"</sub>: "+influx+"-"+temperature+"="+delta+" | " + sensor.deltatemp  
                };
            }
            if (sensor.name == "influx")
                influx = Math.floor(sensor.temperature*10)/10;
        });
    }

}
requestServo.onload = function() {

    var data = JSON.parse(this.response);
    if (requestServo.status >= 200 && requestServo.status < 400) {
        servoState = data["servos"];
    }
}
requestRoom.onload = function () {

  // Begin accessing JSON data here

  var data = JSON.parse(this.response);

  if (requestRoom.status >= 200 && requestRoom.status < 400) {
    data["rooms"].forEach(room => {
        var chart;

        // when no chart for room, create one else just update based on IDs

        if( ( document.getElementById('card_'+room.name)) == null )
        {
            const card  = document.createElement('div');
            card.setAttribute('class', 'card');
            card.setAttribute('id', 'card_'+room.name);

            app.appendChild(card);  // add the card to the canvas

// HEADER
            var header = document.createElement('h1');
            header.setAttribute('id', 'h1_'+room.name);
            header.textContent = room.name;
// CHART
            const chartContainer = document.createElement('div');
            chartContainer.setAttribute("class","chart-container");
            chartContainer.setAttribute("id", 'container-'+room.name);
// SLIDER
            var sliderContainer = document.createElement('div');
            sliderContainer.setAttribute("class","slidercontainer");

            var slider = document.createElement('input');
            slider.setAttribute("type", "range");
            slider.setAttribute("min", "15");
            slider.setAttribute("max", "25");
            slider.setAttribute("value",room.target);
            slider.setAttribute("step", "0.1");
            slider.setAttribute("class", "slider");
            slider.setAttribute("id", "sl_"+room.name);

            //    <input type="range" min="15" max="25" value="50" class="slider" id="myRange"></input>

            var valuedisplay = document.createElement('p');
            var span = document.createElement('span');
            span.setAttribute("id", "vd_"+room.name);
            valuedisplay.appendChild(span);

            // <p>Value: <span id="demo"></span></p>
            sliderContainer.appendChild(slider);
            sliderContainer.appendChild(valuedisplay);

            span.innerHTML = slider.value;

            slider.oninput = function() {
                span.innerHTML = this.value;
            }

            slider.onmouseup = function() {
                console.log("mouse up");
                requestPut.open('PUT', APIGW+'room?room='+room.name+'&temp='+this.value, true);
                requestPut.send();  
            }

            slider.ontouchend = function() {
                console.log("touch end");
                requestPut.open('PUT', APIGW+'room?room='+room.name+'&temp='+this.value, true);
                requestPut.send();  
            }
// VALVES            
            const valveContainer = document.createElement('div');
            valveContainer.setAttribute("class","valve-list-container");
            valveContainer.setAttribute("id", 'vl-container-'+room.name);

            var valvelist = document.createElement('ul');
            valvelist.setAttribute('class','valve-list');
            
            for (i=0 ; i < room.servocount ; i++)
            {
                var valve = document.createElement('li');
                valve.setAttribute('class', 'valve');
                    
                var valvetext = document.createElement('p');
                valvetext.setAttribute( 'id', "valve-txt-"+room.servos[i]);
                valvetext.innerHTML = "30 &#8451; | "+verbalState(room.servos[i]);
        
                var progress = document.createElement('meter');
                progress.setAttribute( 'id', "progress-"+room.servos[i]);
                progress.setAttribute('class', 'valve-1');
                progress.setAttribute('max', 35);
                progress.setAttribute('min', 15);
                /*
                progress.setAttribute('high',32);
                progress.setAttribute('low', 20);
                */
                progress.setAttribute('value', 20);
                
                valve.appendChild(valvetext);
                valve.appendChild(progress);

                valvelist.appendChild(valve);
            }    
            valveContainer.appendChild(valvelist);

            card.appendChild(header);
            card.appendChild(chartContainer);
            card.appendChild(sliderContainer);
            card.appendChild(valveContainer);

            var info_id = 'info_'+room.name;

            Rooms[room.id] = Highcharts.chart('container-'+room.name, Highcharts.merge(gaugeOptions, {
                yAxis: {
                    min: 15,
                    max: 25,
                    title: {
                        text: 'room temperature'
                    }
                },
            
                credits: {
                    enabled: false
                },
                
                series: [{
                    name: 'C',
                    data: [Math.floor(room.actual)],
                    dataLabels: {
                        format:
                            '<div style="text-align:center">' +
                            '<span style="font-size:25px">{y}&deg;C</span><br/>' +
                            '<span style="font-size:10px;opacity:0.4" id='+info_id+'>degrees</span>' +
                            '</div>'
                    },
                    tooltip: {
                        valueSuffix: ' degrees'
                    }
                }]
            
            }));
        } 

        chart = Rooms[room.id];

        // update gauge with actual temperature
        
        var t = Math.floor(room.actual*10.0)/10.0;
        var point = chart.series[0].points[0];
        var temptext="";

        point.update(t);
        
        // update large display temperature

        if (t > room.target)
        {
            temptext=t+">"+room.target+" [CLOSED]";
        } else {
            temptext=t+"<"+room.target+" [OPEN]";
        }
        document.getElementById ("info_"+room.name).innerHTML=temptext;
        
        // update UGFH loop info

        for (i=0 ; i < room.servocount ; i++)
        {
            var t=sensorInfo[room.servos[i]].temperature;
            var o=influx-sensorInfo[room.servos[i]].targetdelta;

            console.log("updating progress for "+room.name+" with "+i+" "+servoState[ room.servos[i]] );
            var valvetext=document.getElementById("valve-txt-"+room.servos[i]);
            valvetext.innerHTML = sensorInfo[room.servos[i]].text+" | "+verbalState(room.servos[i]);
            
            /*
            var progress=document.getElementById( "progress-"+room.servos[i]);
    
            progress.setAttribute('value', t);   
            progress.setAttribute('optimum', o);
            */
        }  
        
        //  update slider (might have been changed from other browser)

        document.getElementById("sl_"+room.name).value = room.target; 
        document.getElementById("vd_"+room.name).innerHTML = room.target; 
    });
  } else {
    const errorMessage = document.createElement('marquee');
    errorMessage.textContent = `Gah, it's not working!`;
    app.appendChild(errorMessage);
  }
}

function refreshRoomData() {
    requestServo.open('GET', APIGW+'servo/statusarray', true);
    requestServo.send();
 
    requestSensor.open('GET', APIGW+'sensor/list', true);
    requestSensor.send();

    requestRoom.open('GET', APIGW+'room/list', true);
    requestRoom.send();

};

// requestRoom.send();

refreshRoomData(); // initial

var timer = setInterval(refreshRoomData, 15 * 1000); // repeat every 15s
