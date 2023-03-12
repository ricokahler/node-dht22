import bindings from 'bindings';
import { fork } from 'node:child_process';
const Native = bindings('node-dht22');

export class DHT22 {
  constructor(chip, pin) {
    this.chip = chip;
    this.pin = pin;
    this.nativeDht22 = new Native.DHT22(chip, pin);
  }

  readSync() {
    return this.nativeDht22();
  }

  async read(attempts = 5) {
    try {
      const childProcess = fork('./read-sync.mjs', [this.chip, this.pin]);

      const message = await Promise.race([
        new Promise((resolve, reject) => {
          childProcess.on('message', resolve);
          childProcess.on('error', reject);
        }),
        new Promise((resolve) => setTimeout(() => resolve('TIMER'), 2000)),
      ]);

      childProcess.kill();

      if (message === 'TIMER') {
        throw new Error('Timeout');
      }

      return JSON.parse(message);
    } catch (e) {
      if (attempts) {
        await new Promise((resolve) => setTimeout(resolve, 1000));
        return this.read(attempts - 1);
      } else {
        throw e;
      }
    }
  }
}
