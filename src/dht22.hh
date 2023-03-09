#ifndef DHT22_HH
#define DHT22_HH

#include <chrono>
#include <thread>
#include <gpiod.h>
#include <nan.h>

class DHT22 : public Nan::ObjectWrap
{
public:
  static NAN_MODULE_INIT(Init);
  static NAN_METHOD(New);

private:
  explicit DHT22(const char *device = "0", unsigned int pin = 0);
  ~DHT22();

  static NAN_METHOD(read);

  static Nan::Persistent<v8::Function> constructor;

  gpiod_line *line;
  gpiod_chip *chip;
};

#endif // DHT22_HH