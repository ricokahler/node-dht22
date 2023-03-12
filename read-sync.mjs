import bindings from 'bindings';
const { DHT22 } = bindings('node-dht22');

const [chip, pin] = process.argv.slice(2).map((i) => parseInt(i, 10));

try {
  const result = new DHT22(chip, pin).read();
  process.send(JSON.stringify(result));
} catch (e) {}
