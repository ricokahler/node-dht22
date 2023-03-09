// adapted from here:
// https://github.com/adafruit/Adafruit_Python_DHT/blob/8f5e2c4d6ebba8836f6d31ec9a0c171948e3237d/source/Raspberry_Pi_2/pi_2_dht_read.c
#include "dht22.hh"

// This is the only processor specific magic value, the maximum amount of time to
// spin in a loop before bailing out and considering the read a timeout.  This should
// be a high value, but if you're running on a much faster platform than a Raspberry
// Pi or Beaglebone Black then it might need to be increased.
#define DHT_MAXCOUNT 320000

// Number of bit pulses to expect from the DHT.  Note that this is 41 because
// the first pulse is a constant 50 microsecond pulse, with 40 pulses to represent
// the data afterwards.
#define DHT_PULSES 41

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
  if (line)
  {
    gpiod_line_close_chip(line);
    line = NULL;
  }

  if (chip)
  {
    gpiod_chip_close(chip);
    chip = NULL;
  }
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
  DHT22 *obj = Nan::ObjectWrap::Unwrap<DHT22>(info.This());

  Nan::Utf8String consumer(info[0]);

  // request pin output mode
  if (gpiod_line_request_output(obj->line, *consumer, 0) == -1)
    Nan::ThrowError("Could not get output mode for given pin");

  // set pin high for 500ms
  gpiod_line_set_value(obj->line, 1);
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  // set pin low for 20ms.
  gpiod_line_set_value(obj->line, 0);
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  gpiod_line_set_value(obj->line, 1);

  gpiod_line_release(obj->line);

  // request pin input mode
  if (gpiod_line_request_input(obj->line, *consumer) == -1)
    Nan::ThrowError("Could not get input mode for given pin");

  // need a very short delay before reading pins or else value is sometimes still low.
  for (volatile int i = 0; i < 50; ++i)
  {
  }

  // wait for DHT to pull pin low.
  uint32_t count = 0;
  while (gpiod_line_get_value(obj->line))
  {
    if (++count >= DHT_MAXCOUNT)
    {
      // timeout waiting for response.
      gpiod_line_release(obj->line);
      Nan::ThrowError("Timeout waiting for sensor");
      return;
    }
  }

  // store the count that each DHT bit pulse is low and high.
  // make sure array is initialized to start at zero.
  unsigned int pulseCounts[DHT_PULSES * 2] = {0};

  // record pulse widths for the expected result bits.
  for (int i = 0; i < DHT_PULSES * 2; i += 2)
  {
    // count how long pin is low and store in pulseCounts[i]
    while (!gpiod_line_get_value(obj->line))
    {
      if (++pulseCounts[i] >= DHT_MAXCOUNT)
      {
        // timeout waiting for response.
        gpiod_line_release(obj->line);
        Nan::ThrowError("Timeout while reading response from sensor");
        return;
      }
    }

    // count how long pin is high and store in pulseCounts[i+1]
    while (gpiod_line_get_value(obj->line))
    {
      if (++pulseCounts[i + 1] >= DHT_MAXCOUNT)
      {
        // timeout waiting for response.
        gpiod_line_release(obj->line);
        Nan::ThrowError("Timeout while reading response from sensor");
        return;
      }
    }
  }

  // done with timing critical code, now interpret the results.

  // compute the average low pulse width to use as a 50 microsecond reference threshold.
  // ignore the first two readings because they are a constant 80 microsecond pulse.
  unsigned int threshold = 0;
  for (int i = 2; i < DHT_PULSES * 2; i += 2)
  {
    threshold += pulseCounts[i];
  }
  threshold /= DHT_PULSES - 1;

  // interpret each high pulse as a 0 or 1 by comparing it to the 50us reference.
  // if the count is less than 50us it must be a ~28us 0 pulse, and if it's higher
  // then it must be a ~70us 1 pulse.
  uint8_t data[5] = {0};
  for (int i = 3; i < DHT_PULSES * 2; i += 2)
  {
    int index = (i - 3) / 16;
    data[index] <<= 1;
    if (pulseCounts[i] >= threshold)
    {
      // one bit for long pulse.
      data[index] |= 1;
    }
    // else zero bit for short pulse.
  }

  // // useful debug info:
  // printf("Data: 0x%x 0x%x 0x%x 0x%x 0x%x\n", data[0], data[1], data[2], data[3], data[4]);

  // verify checksum of received data.
  if (data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF))
  {
    Nan::ThrowError("Data received incorrectly. Checksum failed.");
    return;
  }

  // Calculate humidity and temp for DHT22 sensor.
  // float humidity = (data[0] * 256 + data[1]) / 10.0f;
  float temperature = ((data[2] & 0x7F) * 256 + data[3]) / 10.0f;
  if (data[2] & 0x80)
  {
    temperature *= -1.0f;
  }

  info.GetReturnValue().Set(temperature);
}
