const fs = require('fs');
const mqtt = require('mqtt');
const Bonfire = require('bonfire-addon');

const argv = require('yargs')
  .alias('j', 'config')
  .alias('a', 'address')
  .argv;

let settings = JSON.parse(fs.readFileSync(argv.config)).settings;

var ppid = process.ppid;
setInterval(() => {
  if (ppid != process.ppid)
    exit(1);
}, 1000);

var mqtt_cli = mqtt.connect('mqtt://' + settings.host);
mqtt_cli.on('connect', () => {
  console.log("on connect!");
});
mqtt_cli.on('reconnect', () => {
  console.log("on reconnect!");
});
mqtt_cli.on('disconnect', () => {
  console.log("on disconnect!");
});
mqtt_cli.on('close', () => {
  console.log("on close!");
});
mqtt_cli.on('error', (err) => {
  console.log(err);
});

var bf = new Bonfire();
bf.connect(argv.address);
bf.subscribe('tag/update');
bf.on('tag/update', (content) => {
  if (!mqtt_cli.connected) return;
  try {
    const cnt = JSON.parse(content);
    const tmp = {
      'tag': cnt.name,
      'value': cnt.value,
      'ts': cnt.value_ts
    };
    mqtt_cli.publish(settings.topic, JSON.stringify(tmp), {
      qos: settings.qos,
      retain: settings.retain
    });
  } catch (ex) {
    console .log(ex);
  }
});
