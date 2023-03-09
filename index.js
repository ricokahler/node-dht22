const { DHT22 } = require('bindings')('node-dht22');

const dht22 = new DHT22(1, 91);

console.log(dht22.read());
