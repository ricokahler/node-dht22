import { DHT22 } from './index.mjs';

const dht22 = new DHT22(1, 91);

const { temperature } = await dht22.read();

console.log(temperature * (9 / 5) + 32);
