#include "dht22.hh"

NAN_MODULE_INIT(InitAll)
{
  // Nan::Set(target, Nan::New("version").ToLocalChecked(),
  //          Nan::GetFunction(Nan::New<v8::FunctionTemplate>(version)).ToLocalChecked());
  DHT22::Init(target);
}

NODE_MODULE(NativeExtension, InitAll)
