#include "env.h"
#include "PubSub.h"
#include "OutputManager.h"
#include "InputManager.h"

PubSub* PubSub::_instance = NULL;

PubSub* PubSub::Instance()
{
  if(!_instance) {
    _instance = new PubSub(ENV_PUBSUB_HOST, ENV_PUBSUB_PORT);
  }

  return _instance;
}

PubSub::PubSub(const char* host, const int port) :
  _client(PubSubClient()),
  _wifi(WiFiClient())
{
  _client.setClient(_wifi);
  _client.setServer(host, port);
  _client.setCallback([this] (char* topic, byte* payload, unsigned int length) {
    this->callback(topic, payload, length);
  });
}

boolean PubSub::connect() {
  Serial.print("Connecting to PubSub server..");
  while (!_client.connected()) {
    Serial.print(".");
    char buffer[32];
    String name = ENV_DEVICE_NAME;
    name.toCharArray(buffer, 32);
    _client.connect(buffer);
    delay(100);
  }

  Serial.println(" success!");
  registerDevice();
  return _client.connected();
}

void PubSub::addOutputManager(OutputManager* manager)
{
  outputManager = manager;
}

void PubSub::addInputManager(InputManager* manager)
{
  inputManager = manager;
}

/**
 * Get the topic by appending the local topic with the namespace of the device.
 */
char* getTopic(String topic)
{
  char buffer[128];
  topic = String(ENV_DEVICE_NAME + "/" + topic);
  topic.toCharArray(buffer, 128);

  return buffer;
}

void PubSub::loop()
{
  if (!_client.connected()) {
    long now = millis();
    if (now - lastReconnectAttempt > 10000) {
        lastReconnectAttempt = now;
        // Attempt to reconnect
        if (connect()) {
          lastReconnectAttempt = 0;
        }
      }
  } else {
    ping();
    _client.loop();
    inputManager->loop();
  }
}

void PubSub::registerDevice()
{
  StaticJsonBuffer<1024> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["name"] = ENV_DEVICE_NAME;

  publish(String("register"), root);
  lastPing = millis();
}

void PubSub::ping()
{
  long now = millis();
  if(now - lastPing > 60000) {
    publish("ping");
    lastPing = now;
  }
}

void PubSub::publish(String topic)
{
  char* str = getTopic(topic);
  _client.publish(str, "");
}

void PubSub::publish(String topic, JsonObject& payload)
{
  char* str = getTopic(topic);
  char buffer[1024];
  payload.printTo(buffer, sizeof(buffer));
  _client.publish(str, buffer);
}

void PubSub::publish(String topic, JsonArray& payload)
{
  char* str = getTopic(topic);
  char buffer[1024];
  payload.printTo(buffer, sizeof(buffer));
  _client.publish(str, buffer);
}

void PubSub::subscribe(String topic)
{
  char* str = getTopic(topic);
  _client.subscribe(str);
}

void PubSub::callback(char* topic, byte* payload, unsigned int length) {
  char data[80];

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i=0;i<length;i++)
  {
    data[i] = (char)payload[i];
    Serial.print((char)payload[i]);
  }
  Serial.println();

  StaticJsonBuffer<256> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(data);

  outputManager->callback(topic, root);
}