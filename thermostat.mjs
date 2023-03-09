// @ts-check
// type HVACAction = 'off' | 'heating' | 'cooling' | 'fan' | 'idle';
// type HVACMode = 'off' | 'heat' | 'cool' | 'heat-cool' | 'fan-only';
// import sensor from 'node-dht-sensor';
import express from 'express';
import readline from 'readline';
// import { promisify } from 'util';
// import * as child_process from 'child_process';
import Libgpiod from 'node-libgpiod';
// const exec = promisify(child_process.exec);

const rl = readline.promises.createInterface({
  input: process.stdin,
  output: process.stdout,
});

const CHIP_0 = new Libgpiod.Chip(0);
const CHIP_1 = new Libgpiod.Chip(1);

// constants to utilize the `libgpiod` interface via `node-libgpiod`
// values taken from `lgpio header 7J1`
const PIN_HEATING = new Libgpiod.Line(CHIP_1, 93); // PIN 16
const PIN_COOLING = new Libgpiod.Line(CHIP_0, 6); // PIN 12
const PIN_FAN = new Libgpiod.Line(CHIP_1, 92); // PIN 10
// const PIN_SENSOR = new Libgpiod.Line(CHIP_1, 91); // PIN 8

PIN_HEATING.requestOutputMode();
PIN_COOLING.requestOutputMode();
PIN_FAN.requestOutputMode();

const POLLING_THROTTLE_TIME = 100;
const PORT = 4201;

async function sleep(milliseconds) {
  return new Promise((resolve) => setTimeout(resolve, milliseconds));
  // const start = process.hrtime.bigint();
  // while (true) {
  //   await new Promise((resolve) => setImmediate(resolve));
  //   const current = process.hrtime.bigint();
  //   const deltaMicroseconds = (current - start) / 1000n;
  //   if (deltaMicroseconds >= microseconds) break;
  // }
}

class TemperatureSensor {
  constructor(options) {
    this.options = options;
  }

  async sendStartSignal() {
    // this comes from the DHT22 data sheet:
    // https://cdn-shop.adafruit.com/datasheets/DHT22.pdf
    const { chip, line } = this.options;
    const pin = new Libgpiod.Line(chip, line);
    pin.requestOutputMode();

    try {
      // step 1: MCU send out start signal to AM2303
      pin.setValue(0);
      await sleep(100);

      // host computer send start signal and keep this signal at least 500μs
      pin.setValue(1);
      await sleep(400);

      // host pull up voltage and wait sensor's response
      pin.setValue(0);
      // await sleep(20);
    } finally {
      pin.release();
    }
  }

  async *readData() {
    const { chip, line } = this.options;
    const pin = new Libgpiod.Line(chip, line);
    pin.requestInputMode();

    try {
      let prev;

      while (true) {
        const value = pin.getValue();
        const timestamp = process.hrtime.bigint();

        if (value !== prev) {
          prev = value;
          yield { value, timestamp };
        }

        await new Promise((resolve) => setImmediate(resolve));
      }
    } finally {
      pin.release();
    }
  }

  async read() {
    await this.sendStartSignal();
    let i = 0;
    for await (const value of this.readData()) {
      console.log(value);
      if (i > 10) return;
      i++;
    }
    console.log('finsihed reading');
  }
}

const sensor = new TemperatureSensor({ chip: CHIP_1, line: 91 });

// const PINS = {
//   // heating:
// }

// if (!sensor.initialize(SENSOR_TYPE, PINS.sensor)) {
//   throw new Error(`Failed to initialize temperature sensor.`);
// }

class Thermostat {
  action = 'off';
  state = { mode: 'off', target: undefined, temperature: undefined };

  constructor(options) {
    this.options = options;
  }

  update(state) {
    this.state = { ...this.state, ...state };
    const action = Thermostat.calculateAction(this.state);

    if (this.action !== action) {
      this.action = action;
      this.options.onActionChange(action);
    }
  }

  static calculateAction(state) {
    if (state.mode === 'off') return 'off';
    if (state.mode === 'fan-only') return 'fan';
    if (!state.target) return 'idle';
    if (!state.temperature) return 'idle';

    const lower = Array.isArray(state.target) ? state.target[0] : state.target;
    const upper = Array.isArray(state.target) ? state.target[1] : state.target;
    const allowsHeat = state.mode === 'heat' || state.mode === 'heat-cool';
    const allowsCool = state.mode === 'cool' || state.mode === 'heat-cool';

    if (allowsHeat && state.temperature < upper) return 'heating';
    if (allowsCool && state.temperature > lower) return 'cooling';

    return 'idle';
  }
}

async function handleActionChange(action) {
  switch (action) {
    case 'cooling': {
      PIN_COOLING.setValue(1);
      PIN_HEATING.setValue(0);
      PIN_FAN.setValue(0);
      return;
    }
    case 'heating': {
      PIN_COOLING.setValue(0);
      PIN_HEATING.setValue(1);
      PIN_FAN.setValue(0);
      return;
    }
    case 'fan': {
      PIN_COOLING.setValue(0);
      PIN_HEATING.setValue(0);
      PIN_FAN.setValue(1);
      return;
    }
    default: {
      PIN_COOLING.setValue(0);
      PIN_HEATING.setValue(0);
      PIN_FAN.setValue(0);
      return;
    }
  }
}

const thermostat = new Thermostat({
  onActionChange: handleActionChange,
});

const app = express();
const api = express.Router();

api.use(express.json());

api.get('/', (_req, res) => {
  res.json({ ...thermostat.state, action: thermostat.action });
});

api.put('/', (req, res) => {
  const { mode, target } = req.body;

  thermostat.update({
    ...(mode && { mode }),
    ...(target && { target }),
  });

  res.json({
    ...thermostat.state,
    action: thermostat.action,
  });
});

app.use('/thermostat', api);

await new Promise((resolve, reject) => {
  const server = app.listen(PORT, resolve);
  server.addListener('error', reject);
});
console.log(`Server up on ${PORT}`);

while (true) {
  const start = Date.now();
  const temperature = parseFloat(await rl.question('New temp? '));
  // const { temperature } = await sensor.promises.read(SENSOR_TYPE, PINS.sensor);

  thermostat.update({ temperature });

  const delta = Date.now() - start;
  if (delta < POLLING_THROTTLE_TIME) {
    await new Promise((resolve) =>
      setTimeout(resolve, POLLING_THROTTLE_TIME - delta)
    );
  }

  sensor.read();
}
