// adapted from here:
// https://github.com/adafruit/Adafruit_Python_DHT/blob/8f5e2c4d6ebba8836f6d31ec9a0c171948e3237d/source/Raspberry_Pi_2/pi_2_dht_read.c
#include "dht22.hh"

Nan::Persistent<v8::Function> DHT22::constructor;

NAN_MODULE_INIT(DHT22::Init)
{
  v8::Local<v8::FunctionTemplate> tpl = Nan::New<v8::FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("DHT22").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  Nan::SetPrototypeMethod(tpl, "read", read);

  constructor.Reset(Nan::GetFunction(tpl).ToLocalChecked());
  Nan::Set(target, Nan::New("DHT22").ToLocalChecked(), Nan::GetFunction(tpl).ToLocalChecked());
}

DHT22::DHT22(const char *device, unsigned int pin)
{
  chip = gpiod_chip_open_lookup(device);
  if (!chip)
    Nan::ThrowError("Unable to open device");

  line = gpiod_chip_get_line(chip, pin);
  if (!line)
    Nan::ThrowError("Unable to open GPIO line");
}

DHT22::~DHT22()
{
  // TODO: i think these are causing segfaults
  // if (line)
  // {
  //   gpiod_line_close_chip(line);
  //   line = NULL;
  // }

  // if (chip)
  // {
  //   gpiod_chip_close(chip);
  //   chip = NULL;
  // }
}

NAN_METHOD(DHT22::New)
{
  if (!info.IsConstructCall())
  {
    Nan::ThrowError("Unable to open GPIO line");
  }

  Nan::Utf8String device(info[0]);
  unsigned int pin = Nan::To<unsigned int>(info[1]).FromJust();

  DHT22 *obj = new DHT22(*device, pin);

  if (!obj->line)
    return;
  if (!obj->chip)
    return;

  obj->Wrap(info.This());
  info.GetReturnValue().Set(info.This());
}

NAN_METHOD(DHT22::read)
{
  // according to the DHT22 (AM2303) data sheet found here:
  // https://cdn-shop.adafruit.com/datasheets/DHT22.pdf
  //
  // the AM2303 transmits the current temperature and humidity via a single
  // signal data bus.
  //
  // process:
  // - STEP 1: HOST START signal - we pull the pin high for 500ms then pull it
  //   low. this will tell the sensor we're requesting a reading
  // - STEP 2: SENSOR ACK signal — we wait for the sensor to acknowledge the
  //   request by waiting up to 40us for the sensor to pull the pin high (but no
  //   quicker than ~20us). the sensor will leave the pin high for 80us then
  //   pull it low again for another 80ms.
  // - STEP 3: LISTEN - the AM2303 sends 40 bits of data over the pin by a
  //   series of pulses. the AM2303 starts each bit transmission by
  //   pulling the pin high for 50ms then pulling it low for either:
  //   - 25us to transmit a 0 bit
  //   - 70us to transmit a 1 bit
  // - STEP 4: DECODE and verify - the 40 bits represent 5 bytes of data in the
  //   following order can be interpreted as following:
  //   - 8 bits integral humidity data
  //   - 8 bits decimal humidity data
  //   - 8 bits integral temperature data
  //   - 8 bits decimal temperature data
  //   - 8 bits for a checksum
  DHT22 *obj = Nan::ObjectWrap::Unwrap<DHT22>(info.This());

  Nan::Utf8String consumer(info[0]);

  // STEP 1: HOST START
  // request pin output mode
  if (gpiod_line_request_output(obj->line, *consumer, 0) == -1)
    Nan::ThrowError("Could not get output mode for given pin");

  // set pin high for 500ms
  gpiod_line_set_value(obj->line, 1);
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  // set pin low and wait
  gpiod_line_set_value(obj->line, 0);

  // STEP 2: SENSOR ACK signal
  // we ignore these pulses

  while (!gpiod_line_get_value(obj->line))
  {
  }

  while (gpiod_line_get_value(obj->line))
  {
  }

  bool bits[40] = {0};
  int expected_pulses = 40;

  for (int i = 0; i < expected_pulses; i++)
  {
    int count_0 = 0;
    int count_1 = 0;

    while (!gpiod_line_get_value(obj->line))
    {
      count_0++;
    }

    while (gpiod_line_get_value(obj->line))
    {
      count_1++;
    }

    // if the number of high cycles exists twice the amount of low then
    // we assume that's an on bit
    bool bit = (float)count_1 / (float)count_0 > 2.0;
    bits[i] = bit;

    // this is required actually idky
    std::cout << "0s: ";
    std::cout << count_0;
    std::cout << " 1s: ";
    std::cout << count_1;
    std::cout << "\n";
  }

  uint8_t data[5] = {0};
  for (int byte_index = 0; byte_index < 5; byte_index++)
  {
    int offset = byte_index * 8;
    for (int i = 0; i < 8; i++)
    {
      if (bits[offset + i])
      {
        data[byte_index] = data[byte_index] + (1 << (7 - i));
      }
    }
  }

  // verify checksum of received data.
  if (data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF))
  {
    Nan::ThrowError("Data received incorrectly. Checksum failed.");
    return;
  }

  // Calculate humidity and temp for DHT22 sensor.
  float humidity = (data[0] * 256 + data[1]) / 10.0f;
  float temperature = ((data[2] & 0x7F) * 256 + data[3]) / 10.0f;
  if (data[2] & 0x80)
  {
    temperature *= -1.0f;
  }

  v8::Local<v8::Object> ret = Nan::New<v8::Object>();
  Nan::Set(ret, Nan::New("humidity").ToLocalChecked(), Nan::New<v8::Number>(humidity));
  Nan::Set(ret, Nan::New("temperature").ToLocalChecked(), Nan::New<v8::Number>(temperature));

  info.GetReturnValue().Set(ret);
}
